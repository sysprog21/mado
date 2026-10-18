/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Glyph-cached text
 *
 * twin_path_utf8() turns a whole string into one outline, which is the right
 * tool for transformed text but repeats the outline decode, curve
 * flattening and supersampled scan conversion for every glyph on every
 * repaint.  UI text is axis aligned and drawn at a handful of sizes, so this
 * path rasterizes each (font, size, code point) once into the coverage cache
 * and afterwards only composites the cached mask.
 *
 * Glyph origins snap to whole pixels; advances accumulate in 16.16 so line
 * length matches twin_text_width() exactly.
 */

#include "twin_private.h"

static void _twin_path_init_local(twin_path_t *path,
                                  twin_font_t *font,
                                  twin_fixed_t size)
{
    _twin_path_init(path);
    path->state.font = font;
    path->state.font_size = size;
}

static inline twin_font_t *_font_or_default(twin_font_t *font)
{
    return font ? font : g_twin_font;
}

void twin_font_extents(twin_font_t *font,
                       twin_fixed_t size,
                       twin_fixed_t *ascent,
                       twin_fixed_t *descent)
{
    font = _font_or_default(font);
    twin_fixed_t a, d;
    if (font->type == TWIN_FONT_TYPE_TTF) {
        a = (twin_fixed_t) (((int64_t) font->ascender * size) >> 6);
        d = (twin_fixed_t) (((int64_t) -font->descender * size) >> 6);
    } else {
        d = size / 3;
        a = size - d;
    }
    if (ascent)
        *ascent = a;
    if (descent)
        *descent = d;
}

twin_fixed_t twin_text_width(twin_font_t *font,
                             twin_fixed_t size,
                             const char *utf8)
{
    twin_path_t path;
    _twin_path_init_local(&path, _font_or_default(font), size);
    twin_fixed_t w = twin_width_utf8(&path, utf8);
    _twin_path_fini(&path);
    return w;
}

static uint64_t _glyph_key(const twin_font_t *font,
                           twin_fixed_t size,
                           twin_ucs4_t ucs4)
{
    uint64_t h = TWIN_HASH_INIT ^ 0x47; /* 'G': glyph domain */
    uintptr_t f = (uintptr_t) font;
    h = _twin_hash(h, &f, sizeof(f));
    h = _twin_hash(h, &size, sizeof(size));
    h = _twin_hash(h, &ucs4, sizeof(ucs4));
    return h;
}

/* Glyph outline at the origin, in device space; returns the advance */
static twin_fixed_t _glyph_outline(twin_path_t *path,
                                   twin_font_t *font,
                                   twin_fixed_t size,
                                   twin_ucs4_t ucs4)
{
    _twin_path_init_local(path, font, size);
    twin_path_move(path, 0, 0);
    /* Measured at the origin, as twin_text_width() measures */
    twin_fixed_t advance = twin_width_ucs4(path, ucs4);
    twin_path_ucs4(path, ucs4);
    /* Drop the trailing pen move so the bounds cover ink only */
    if (path->npoints)
        path->npoints--;
    if (path->nsublen && path->sublen[path->nsublen - 1] >= path->npoints)
        path->nsublen--;
    return advance;
}

/*
 * Ink bounds.  Point 0 is the pen origin that twin_path_ucs4() starts from;
 * its edges to and from the first contour cancel, so it carries no coverage
 * and must not stretch the mask down to the baseline.
 */
static void _glyph_ink_bounds(const twin_path_t *path, twin_rect_t *rect)
{
    _twin_path_bounds_from(path, 1, rect);
}

/*
 * Look the glyph up, rasterizing on a miss.  When the cache cannot hold it
 * (disabled, or larger than an entry may be), the coverage is rendered into
 * @tmp, which the caller frees.
 */
static const twin_cache_entry_t *_glyph_get(twin_font_t *font,
                                            twin_fixed_t size,
                                            twin_ucs4_t ucs4,
                                            twin_cache_entry_t **tmp)
{
    uint64_t key = _glyph_key(font, size, ucs4);
    twin_cache_entry_t *e = _twin_cache_lookup(key);
    if (e)
        return e;

    twin_path_t path;
    twin_rect_t b;
    twin_fixed_t advance = _glyph_outline(&path, font, size, ucs4);
    _glyph_ink_bounds(&path, &b);
    twin_coord_t w = b.right - b.left, h = b.bottom - b.top;
    if (w <= 0 || h <= 0) {
        w = h = 0;
        b.left = b.top = 0;
    }

    e = _twin_cache_insert(key, w, h);
    if (!e) {
        e = *tmp = _twin_cache_entry_alloc(w, h);
        if (!e) {
            _twin_path_fini(&path);
            return NULL;
        }
    }
    e->left = b.left;
    e->top = b.top;
    e->advance = advance;
    if (w && h) {
        twin_pixmap_t mask;
        _twin_pixmap_init_a8(&mask, w, h, e->stride, e->data);
        twin_fill_path(&mask, &path, -b.left, -b.top, NULL);
    }
    _twin_path_fini(&path);
    return e;
}

twin_fixed_t twin_text_draw(twin_pixmap_t *dst,
                            twin_font_t *font,
                            twin_fixed_t size,
                            twin_fixed_t x,
                            twin_fixed_t y,
                            twin_argb32_t color,
                            const char *utf8)
{
    font = _font_or_default(font);

    /* Whole line outside the clip vertically: measure only.  Glyphs may
     * overshoot the nominal extents (accents, brackets, stroke pens), so
     * allow half an em of slack either way. */
    twin_fixed_t ascent, descent;
    twin_font_extents(font, size, &ascent, &descent);
    twin_coord_t slack = twin_fixed_to_int(size / 2) + 2;
    twin_coord_t baseline = twin_fixed_to_int(y + TWIN_FIXED_HALF);
    twin_coord_t line_top = baseline - twin_fixed_to_int(ascent) - slack;
    twin_coord_t line_bottom = baseline + twin_fixed_to_int(descent) + slack;
    twin_rect_t clip = twin_pixmap_get_clip(dst);
    if (line_bottom <= clip.top || line_top >= clip.bottom)
        return twin_text_width(font, size, utf8);

    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = color};
    twin_fixed_t pen = x;
    twin_ucs4_t ucs4;
    int len;
    twin_pixmap_t mask; /* header reused for every glyph */
    _twin_pixmap_init_a8(&mask, 0, 0, 0, NULL);
    twin_operand_t msk = {.source_kind = TWIN_PIXMAP, .u.pixmap = &mask};

    while ((len = _twin_utf8_to_ucs4(utf8, &ucs4)) > 0) {
        twin_coord_t gx = twin_fixed_to_int(pen + TWIN_FIXED_HALF);

        /* Past the right edge: the rest is only measured below.  Ink can
         * start left of the pen (negative bearing), hence the slack. */
        if (gx - slack > clip.right)
            break;

        twin_cache_entry_t *tmp = NULL;
        const twin_cache_entry_t *e = _glyph_get(font, size, ucs4, &tmp);
        if (!e)
            break; /* out of memory: measure this glyph and the rest */
        utf8 += len;
        if (e->width && e->height && gx + e->left + e->width > clip.left) {
            mask.width = mask.clip.right = e->width;
            mask.height = mask.clip.bottom = e->height;
            mask.stride = e->stride;
            mask.p.v = (void *) e->data;
            twin_composite(dst, gx + e->left, baseline + e->top, &src, 0, 0,
                           &msk, 0, 0, TWIN_OVER, e->width, e->height);
        }
        pen += e->advance;
        twin_free(tmp);
    }
    /* Anything not drawn (past the right clip) still counts */
    if (*utf8)
        pen += twin_text_width(font, size, utf8);
    return pen - x;
}
