/*
 * Copyright (c) 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "twin-ttf.h"

static double pos(FT_Pos x, outline_closure_t *c)
{
    FT_Face face = c->face;

    return (double) x / (double) face->units_per_EM;
}

static void command(char cmd, outline_closure_t *c)
{
    printf("\t'%c', ", cmd);
    c->offset++;
}

static int clamped;

/* Coordinates are 2.6 fixed point in a signed byte: clamp, never wrap */
static void cpos(FT_Pos x, outline_closure_t *c)
{
    int v = (int) floor(64.0 * pos(x, c) + 0.5);
    if (v > 127 || v < -128) {
        v = v > 127 ? 127 : -128;
        clamped++;
    }
    printf("%d, ", v);
    c->offset++;
}

static unsigned char *ucs4_to_utf8(FT_ULong ucs4, unsigned char dest[8])
{
    int bits;
    unsigned char *d = dest;

    if (ucs4 < 0x80) {
        *d++ = ucs4;
        bits = -6;
    } else if (ucs4 < 0x800) {
        *d++ = ((ucs4 >> 6) & 0x1F) | 0xC0;
        bits = 0;
    } else if (ucs4 < 0x10000) {
        *d++ = ((ucs4 >> 12) & 0x0F) | 0xE0;
        bits = 6;
    } else if (ucs4 < 0x200000) {
        *d++ = ((ucs4 >> 18) & 0x07) | 0xF0;
        bits = 12;
    } else if (ucs4 < 0x4000000) {
        *d++ = ((ucs4 >> 24) & 0x03) | 0xF8;
        bits = 18;
    } else if (ucs4 < 0x80000000) {
        *d++ = ((ucs4 >> 30) & 0x01) | 0xFC;
        bits = 24;
    } else
        return 0;

    for (; bits >= 0; bits -= 6) {
        *d++ = ((ucs4 >> bits) & 0x3F) | 0x80;
    }
    *d++ = '\0';
    return dest;
}

static void glyph(FT_Pos advance, FT_ULong ucs4, outline_closure_t *c)
{
    unsigned char utf8[8];
    printf("    /* 0x%lx (%s) */ ", ucs4, ucs4_to_utf8(ucs4, utf8));
    cpos(advance, c);
    printf("\n");
}

static int outline_moveto(const FT_Vector *to, void *user)
{
    outline_closure_t *c = user;
    command('m', c);
    cpos(to->x, c);
    cpos(to->y, c);
    printf("\n");
    return 0;
}

static int outline_lineto(const FT_Vector *to, void *user)
{
    outline_closure_t *c = user;
    command('l', c);
    cpos(to->x, c);
    cpos(to->y, c);
    printf("\n");
    return 0;
}

static int outline_conicto(const FT_Vector *control,
                           const FT_Vector *to,
                           void *user)
{
    outline_closure_t *c = user;
    command('2', c);
    cpos(control->x, c);
    cpos(control->y, c);
    cpos(to->x, c);
    cpos(to->y, c);
    printf("\n");
    return 0;
}

static int outline_cubicto(const FT_Vector *control1,
                           const FT_Vector *control2,
                           const FT_Vector *to,
                           void *user)
{
    outline_closure_t *c = user;
    command('c', c);
    cpos(control1->x, c);
    cpos(control1->y, c);
    cpos(control2->x, c);
    cpos(control2->y, c);
    cpos(to->x, c);
    cpos(to->y, c);
    printf("\n");
    return 0;
}

static const FT_Outline_Funcs outline_funcs = {
    outline_moveto, outline_lineto, outline_conicto, outline_cubicto, 0, 0,
};

#define UCS_PAGE_SHIFT 7
#define UCS_PER_PAGE (1 << UCS_PAGE_SHIFT)

static int ucs_page(FT_ULong ucs4)
{
    return ucs4 >> UCS_PAGE_SHIFT;
}

static FT_ULong ucs_first_in_page(FT_ULong ucs4)
{
    return ucs4 & ~(UCS_PER_PAGE - 1);
}

#define MAX_UCS4 0x110000
#define MAX_RANGES 64

static FT_ULong range_lo[MAX_RANGES], range_hi[MAX_RANGES];
static int n_ranges;

static int wanted(FT_ULong ucs4)
{
    if (!n_ranges)
        return ucs4 < MAX_UCS4;
    for (int i = 0; i < n_ranges; i++)
        if (range_lo[i] <= ucs4 && ucs4 <= range_hi[i])
            return 1;
    return 0;
}

/* Parse one code point; it must span the whole of [s, end) */
static int parse_ucs4(const char *s, const char *end, FT_ULong *out)
{
    char *stop;

    if (s == end || *s < '0' || *s > '9')
        return 0;
    *out = strtoul(s, &stop, 0);
    return stop == end && *out < MAX_UCS4;
}

/* "0x20-0x7e", "0xf095", or comma separated lists of either */
static int parse_ranges(char *spec)
{
    for (char *tok = strtok(spec, ","); tok; tok = strtok(NULL, ",")) {
        char *end = tok + strlen(tok);
        char *dash = strchr(tok, '-');
        if (n_ranges == MAX_RANGES)
            return 0;
        if (!parse_ucs4(tok, dash ? dash : end, &range_lo[n_ranges]))
            return 0;
        if (!dash)
            range_hi[n_ranges] = range_lo[n_ranges];
        else if (!parse_ucs4(dash + 1, end, &range_hi[n_ranges]) ||
                 range_hi[n_ranges] < range_lo[n_ranges])
            return 0;
        n_ranges++;
    }
    /* An empty list would silently mean the whole font */
    return n_ranges > 0;
}

/* The symbol becomes part of a C identifier */
static int valid_symbol(const char *s)
{
    if (!*s)
        return 0;
    for (; *s; s++)
        if (!(*s == '_' || (*s >= '0' && *s <= '9') ||
              (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')))
            return 0;
    return 1;
}

static int convert_font(const char *in_name, const char *symbol)
{
    FT_Library ftLibrary;
    FT_Face face;
    FT_UInt gindex;
    FT_ULong ucs4;
    FT_Int32 load_flags;
    outline_closure_t closure;
    int *offsets;
    int ncharmap;

    if (FT_Init_FreeType(&ftLibrary))
        return 0;
    if (FT_New_Face(ftLibrary, in_name, 0, &face))
        return 0;
    if (FT_Select_Charmap(face, ft_encoding_unicode))
        return 0;

    load_flags = FT_LOAD_NO_SCALE | FT_LOAD_LINEAR_DESIGN;
    closure.face = face;
    closure.offset = 0;
    offsets = calloc(face->num_glyphs + 1, sizeof(int));
    if (!offsets) {
        fprintf(stderr, "out of memory\n");
        return 0;
    }

    printf("/*\n * Generated by tools/ttf/twin-ttf from %s %s.\n",
           face->family_name, face->style_name);
    printf(" * Do not edit: regenerate from the font instead.\n */\n\n");
    printf("#include \"twin_private.h\"\n\n");
    printf("/* clang-format off */\n");
    printf("static const signed char outlines[] = {\n");
    /* Offset 0 is reserved: it marks code points the font lacks */
    printf("    0,\n");
    closure.offset = 1;
    for (ucs4 = FT_Get_First_Char(face, &gindex);
         gindex != 0 && ucs4 < MAX_UCS4;
         ucs4 = FT_Get_Next_Char(face, ucs4, &gindex)) {
        if (!wanted(ucs4) || offsets[gindex])
            continue;
        if (FT_Load_Glyph(face, gindex, load_flags) != 0)
            continue;
        /* twin_charmap_t stores 16-bit offsets: past that, glyphs would
         * silently start in the middle of another one */
        if (closure.offset > UINT16_MAX) {
            fprintf(stderr,
                    "error: outlines exceed %u bytes at U+%04lX; keep fewer "
                    "code points with -r\n",
                    UINT16_MAX, (unsigned long) ucs4);
            free(offsets);
            return 0;
        }
        offsets[gindex] = closure.offset;
        glyph(face->glyph->linearHoriAdvance, ucs4, &closure);
        FT_Outline_Decompose(&face->glyph->outline, &outline_funcs, &closure);
        command('e', &closure);
        printf("\n");
    }
    printf("};\n");
    printf("/* clang-format on */\n\n");
    /* The runtime falls back to charmap[0], so it must exist */
    if (closure.offset == 1) {
        fprintf(stderr, "error: no glyphs in the requested ranges\n");
        free(offsets);
        return 0;
    }
    printf("static const twin_charmap_t charmap[] = {\n");
    ncharmap = 0;
    for (ucs4 = FT_Get_First_Char(face, &gindex);
         gindex != 0 && ucs4 < MAX_UCS4;
         ucs4 = FT_Get_Next_Char(face, ucs4, &gindex)) {
        FT_ULong page = ucs_first_in_page(ucs4);
        FT_ULong off;
        int any = 0;

        for (off = 0; off < UCS_PER_PAGE; off++)
            if (wanted(page + off) &&
                offsets[FT_Get_Char_Index(face, page + off)])
                any = 1;
        if (any) {
            printf("    {0x%04x, {\n", ucs_page(page));
            for (off = 0; off < UCS_PER_PAGE; off++) {
                FT_UInt g = FT_Get_Char_Index(face, page + off);
                int o = wanted(page + off) ? offsets[g] : 0;
                if ((off & 7) == 0)
                    printf("        ");
                printf("0x%04x,%s", o, (off & 7) == 7 ? "\n" : " ");
            }
            printf("    }},\n");
            ncharmap++;
        }
        ucs4 = page + UCS_PER_PAGE - 1;
    }
    printf("};\n\n");
    printf("twin_font_t twin_font_%s = {\n", symbol);
    printf("    .type = TWIN_FONT_TYPE_TTF,\n");
    printf("    .name = \"%s\",\n", face->family_name);
    printf("    .style = \"%s\",\n", face->style_name);
    printf("    .n_charmap = %d,\n", ncharmap);
    printf("    .charmap = charmap,\n");
    printf("    .outlines = outlines,\n");
    printf("    .ascender = ");
    cpos(face->ascender, &closure);
    printf("\n    .descender = ");
    cpos(face->descender, &closure);
    printf("\n    .height = ");
    cpos(face->height, &closure);
    printf("\n};\n");
    if (clamped)
        fprintf(stderr, "warning: clamped %d coordinates\n", clamped);
    free(offsets);
    return 1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-n symbol] [-r ranges] font\n"
            "  -n symbol  emit twin_font_<symbol> (default: \"font\")\n"
            "  -r ranges  code points to keep, e.g. 0x20-0x7e,0xb0,0xf095\n",
            prog);
}

int main(int argc, char **argv)
{
    const char *symbol = "font";
    int i;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            symbol = argv[++i];
            if (!valid_symbol(symbol)) {
                fprintf(stderr, "invalid symbol name: %s\n", symbol);
                return 1;
            }
        } else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            if (!parse_ranges(argv[++i])) {
                usage(argv[0]);
                return 1;
            }
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (i != argc - 1) {
        usage(argv[0]);
        return 1;
    }
    return convert_font(argv[i], symbol) ? 0 : 1;
}
