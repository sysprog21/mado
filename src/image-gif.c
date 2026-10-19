/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 * This file integrates the GIF decoder originally developed by Marcel Rodrigues
 * as part of the gifdec project. You can find the original gifdec project at:
 * https://github.com/lecram/gifdec
 *
 * Frames decode on demand, straight into the pixmap the loader returns: an
 * animation costs one ARGB32 canvas and an open file, however many frames it
 * has, instead of a pixmap per frame.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "twin.h"
#include "twin_private.h"

typedef struct {
    uint8_t colors[0x100 * 3];
} gif_palette_t;

/* Graphic Control Extension: applies to the next image only */
typedef struct {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    bool transparency;
} gif_gce_t;

typedef struct {
    uint16_t length;
    uint16_t prefix;
    uint8_t suffix;
} entry_t;

/* LZW codes are at most 12 bits: the table never outgrows 0x1000 entries,
 * so it is allocated whole, once per decoder */
typedef struct {
    int n_entries;
    entry_t entries[0x1000];
} table_t;

typedef struct _twin_gif {
    twin_animation_t anim; /* first: the pixmap's animation handle */
    FILE *f;
    long anim_start;
    int repeats;  /* plays left after this one; -1: forever */
    bool counted; /* the NETSCAPE count was read: a rewind re-reads it */
    bool done;    /* played out: the last frame stays, nothing more to read */
    gif_gce_t gce;
    gif_palette_t *palette;
    gif_palette_t lct, gct;
    uint16_t fx, fy, fw, fh; /* frame rectangle, clipped to the canvas */
    uint16_t dw, dh;         /* frame size as the image data lays it out */
    unsigned frames;         /* frames drawn since the start or a rewind */
    table_t *table;
    twin_argb32_t *saved; /* pixels under a restore-to-previous frame */
    size_t saved_size;    /* ...and how many fit there */
} twin_gif_t;

/* LZW input: the current data sub-block and the bit position in it */
typedef struct {
    uint8_t buf[0xff];
    uint8_t len, pos;
    uint8_t shift, byte;
} lzw_in_t;

/* Stdio buffer for headers and extensions; image data is read a whole
 * sub-block at a time */
#define GIF_IO_BUFFER 512

#define MIN(A, B) ((A) < (B) ? (A) : (B))

/* Read exactly @n bytes.  A truncated file reads as zeros past its end,
 * which every caller treats as a terminator (a zero block size, a zero
 * separator), so a short file ends decoding instead of feeding it
 * uninitialized bytes or spinning on a size that never reaches zero. */
static bool gif_read(twin_gif_t *gif, void *buf, size_t n)
{
    size_t got = fread(buf, 1, n, gif->f);
    if (got == n)
        return true;
    memset((uint8_t *) buf + got, 0, n - got);
    return false;
}

static void gif_skip(twin_gif_t *gif, long n)
{
    fseek(gif->f, n, SEEK_CUR);
}

static uint16_t read_num(twin_gif_t *gif)
{
    uint8_t bytes[2];

    gif_read(gif, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

static void discard_sub_blocks(twin_gif_t *gif)
{
    uint8_t size;

    do {
        gif_read(gif, &size, 1);
        gif_skip(gif, size);
    } while (size);
}

/* A palette of 2 << (@flags & 7) colors, as image and screen descriptors
 * size them */
static void read_palette(twin_gif_t *gif, gif_palette_t *pal, uint8_t flags)
{
    gif_read(gif, pal->colors, 3 * (2 << (flags & 0x07)));
}

static void table_reset(table_t *table, int key_size)
{
    table->n_entries = (1 << key_size) + 2;
    for (int key = 0; key < (1 << key_size); key++) {
        /* clang-format off */
        table->entries[key] = (entry_t) {1, 0xFFF, key};
        /* clang-format on */
    }
}

/* Add a table entry; the caller stops adding once the table is full.
 * True if the key size must grow after this addition. */
static bool entry_add(table_t *table,
                      uint16_t length,
                      uint16_t prefix,
                      uint8_t suffix)
{
    /* clang-format off */
    table->entries[table->n_entries] = (entry_t) {length, prefix, suffix};
    /* clang-format on */
    table->n_entries++;
    return (table->n_entries & (table->n_entries - 1)) == 0;
}

/* Next @key_size-bit code, or 0x1000 at the data's terminating block */
static uint16_t get_key(twin_gif_t *gif, lzw_in_t *in, int key_size)
{
    uint16_t key = 0;
    int frag_size;

    for (int bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        int rpad = (in->shift + bits_read) % 8;
        if (rpad == 0) {
            /* Next byte, from the next sub-block once this one is used */
            if (in->pos == in->len) {
                gif_read(gif, &in->len, 1);
                if (!in->len)
                    return 0x1000;
                gif_read(gif, in->buf, in->len);
                in->pos = 0;
            }
            in->byte = in->buf[in->pos++];
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) (in->byte >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    in->shift = (in->shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h.  The
 * passes hold rows 0, 4, 2 and 1 modulo 8, 8, 4 and 2: count them with
 * ceiling division, which stays exact for frames shorter than a pass. */
static int interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h + 7) / 8;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h + 3) / 8;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h + 1) / 4;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Canvas row @y of the frame rectangle */
static twin_argb32_t *frame_row(twin_gif_t *gif, int y)
{
    return twin_pixmap_pointer(gif->anim.pixmap, gif->fx, gif->fy + y).argb32;
}

/* Canvas row for the frame's @y-th row of image data, or NULL for a row
 * clipped off the canvas.  Rows are laid out by the unclipped size. */
static twin_argb32_t *data_row(twin_gif_t *gif, int y, bool interlace)
{
    int row = interlace ? interlaced_line_index(gif->dh, y) : y;
    return row < gif->fh ? frame_row(gif, row) : NULL;
}

/* Decompress image pixels into the canvas, leaving transparent ones as
 * they were.  Return 0 on success or -1 on a bad code size. */
static int read_image_data(twin_gif_t *gif, bool interlace)
{
    int init_key_size, key_size;
    bool is_table_full = false;
    uint32_t frm_off, frm_size;
    int str_len = 0, i;
    uint16_t key, clear, stop;
    bool grow = false;
    table_t *table = gif->table;
    entry_t entry = {0};
    twin_argb32_t lut[0x100];
    lzw_in_t in = {.len = 0};
    uint8_t byte;

    gif_read(gif, &byte, 1);
    key_size = (int) byte;
    if (key_size < 2 || key_size > 8)
        return -1;

    for (i = 0; i < 0x100; i++) {
        const uint8_t *c = &gif->palette->colors[i * 3];
        lut[i] =
            0xFF000000U | (uint32_t) c[0] << 16 | (uint32_t) c[1] << 8 | c[2];
    }
    int tindex = gif->gce.transparency ? gif->gce.tindex : -1;

    clear = 1 << key_size;
    stop = clear + 1;
    table_reset(table, key_size);
    key_size++;
    init_key_size = key_size;
    key = get_key(gif, &in, key_size); /* clear code */
    frm_off = 0;
    frm_size = (uint32_t) gif->dw * gif->dh;
    while (frm_off < frm_size) {
        if (key == clear) {
            /* A clear right at a code-size bump cancels it: encoders that
             * clear early to keep codes short rely on this */
            key_size = init_key_size;
            table->n_entries = (1 << (key_size - 1)) + 2;
            is_table_full = false;
            grow = false;
        } else if (!is_table_full) {
            grow = entry_add(table, str_len + 1, key, entry.suffix);
            if (table->n_entries == 0x1000) {
                grow = false;
                is_table_full = true;
            }
        }
        key = get_key(gif, &in, key_size);
        if (key == clear)
            continue;
        if (key == stop || key == 0x1000)
            break;
        if (key >= table->n_entries)
            break;
        if (grow)
            key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        /* A string running past the frame ends a malformed stream */
        if ((uint32_t) str_len > frm_size - frm_off)
            break;
        /* The string is written last pixel first: start at its end and step
         * back one pixel per prefix, a row up at the left edge */
        uint32_t p = frm_off + str_len - 1;
        int x = p % gif->dw, y = p / gif->dw;
        twin_argb32_t *row = data_row(gif, y, interlace);
        for (i = 0; i < str_len; i++) {
            if (i && --x < 0) {
                x = gif->dw - 1;
                row = data_row(gif, --y, interlace);
            }
            if (row && x < gif->fw && entry.suffix != tindex)
                row[x] = lut[entry.suffix];
            if (entry.prefix == 0xFFF || entry.prefix >= table->n_entries)
                break;
            entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->n_entries - 1 && !is_table_full)
            table->entries[table->n_entries - 1].suffix = entry.suffix;
    }
    /* Skip the sub-blocks an early stop left; 0x1000 already read the
     * terminator */
    if (key != 0x1000)
        discard_sub_blocks(gif);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on a bad frame or out-of-memory.
 */
static int read_image(twin_gif_t *gif)
{
    uint8_t fisrz;

    /* Image Descriptor.  The rectangle is clipped to the canvas, to nothing
     * for a frame placed off it: such a frame is still decoded, so the
     * stream stays in step, but draws and disposes of no pixel. */
    uint16_t fx = read_num(gif), fy = read_num(gif);
    uint16_t fw = read_num(gif), fh = read_num(gif);
    const twin_pixmap_t *pix = gif->anim.pixmap;
    gif->fx = fx;
    gif->fy = fy;
    gif->fw = fx < pix->width ? MIN(fw, pix->width - fx) : 0;
    gif->fh = fy < pix->height ? MIN(fh, pix->height - fy) : 0;
    /* Clipped in either direction, nothing is left: never form row
     * pointers past the canvas for a zero-width frame */
    if (!gif->fw || !gif->fh)
        gif->fw = gif->fh = 0;
    gif->dw = fw;
    gif->dh = fh;

    gif_read(gif, &fisrz, 1);
    /* Ignore Sort Flag. */
    gif->palette = &gif->gct;
    if (fisrz & 0x80) { /* Local color table */
        read_palette(gif, &gif->lct, fisrz);
        gif->palette = &gif->lct;
    }

    /* Keep what this frame covers for its restore-to-previous disposal */
    if (gif->gce.disposal == 3) {
        size_t n = (size_t) gif->fw * gif->fh;
        if (n > gif->saved_size) {
            void *saved = twin_realloc(gif->saved, n * sizeof(twin_argb32_t));
            if (!saved)
                return -1;
            gif->saved = saved;
            gif->saved_size = n;
        }
        for (int y = 0; y < gif->fh; y++)
            memcpy(&gif->saved[y * gif->fw], frame_row(gif, y),
                   gif->fw * sizeof(twin_argb32_t));
    }
    /* Image Data. */
    return read_image_data(gif, fisrz & 0x40);
}

/* Undo the previous frame as its disposal method asks, before the next one
 * is drawn.  Restore to background clears to transparent, as browsers do:
 * the background color is a hint from before alpha compositing. */
static void dispose(twin_gif_t *gif, uint8_t disposal)
{
    size_t n = gif->fw * sizeof(twin_argb32_t);

    if (disposal == 2) { /* Restore to background */
        for (int y = 0; y < gif->fh; y++)
            memset(frame_row(gif, y), 0, n);
    } else if (disposal == 3 && gif->saved) { /* Restore to previous */
        for (int y = 0; y < gif->fh; y++)
            memcpy(frame_row(gif, y), &gif->saved[y * gif->fw], n);
    }
}

static void read_graphic_control_ext(twin_gif_t *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    gif_skip(gif, 1);
    gif_read(gif, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 7;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif);
    gif_read(gif, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    gif_skip(gif, 1);
}

static void read_application_ext(twin_gif_t *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    gif_skip(gif, 1);
    /* Application Identifier. */
    gif_read(gif, app_id, 8);
    /* Application Authentication Code. */
    gif_read(gif, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        gif_skip(gif, 2);
        /* Repeats after the first play, as browsers count; 0 is forever */
        uint16_t count = read_num(gif);
        if (!gif->counted)
            gif->repeats = count ? count : -1;
        gif->counted = true;
        /* Skip block terminator. */
        gif_skip(gif, 1);
    } else {
        discard_sub_blocks(gif);
    }
}

static void read_ext(twin_gif_t *gif)
{
    uint8_t label;

    if (!gif_read(gif, &label, 1))
        return;
    switch (label) {
    case 0x01: /* Plain text: skip its metadata, then the text */
        gif_skip(gif, 13);
        discard_sub_blocks(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE: /* Comment */
        discard_sub_blocks(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        log_error("Unknown extension: %02X", label);
        discard_sub_blocks(gif);
    }
}

/* Draw the next frame over the canvas.
 * Return 1 if got a frame; 0 if got GIF trailer; -1 if error.  Only a frame
 * disposes of the previous one, so the last frame stays on the canvas. */
static int gif_get_frame(twin_gif_t *gif)
{
    uint8_t disposal = gif->gce.disposal;
    uint8_t sep;

    gif->gce = (gif_gce_t) {0};
    if (!gif_read(gif, &sep, 1))
        return -1; /* truncated: no trailer */
    while (sep != ',') {
        if (sep == ';')
            return 0;
        if (sep != '!')
            return -1;
        read_ext(gif);
        if (!gif_read(gif, &sep, 1))
            return -1;
    }
    dispose(gif, disposal);
    if (read_image(gif) == -1) {
        /* Nothing drawn, so nothing to dispose of: the rectangle and the
         * saved pixels may still be an earlier frame's */
        gif->gce.disposal = 0;
        return -1;
    }
    /* GIF delays are 1/100 s; browsers play shorter ones at 100 ms, which
     * also keeps a delay of 0 from decoding frames in a tight loop */
    gif->anim.delay = gif->gce.delay < 2 ? 100 : gif->gce.delay * 10;
    gif->frames++;
    return 1;
}

static void gif_rewind(twin_gif_t *gif)
{
    fseek(gif->f, gif->anim_start, SEEK_SET);
    gif->gce = (gif_gce_t) {0};
    gif->frames = 0;
    twin_pixmap_t *pix = gif->anim.pixmap;
    memset(pix->p.v, 0, (size_t) pix->stride * pix->height);
}

/* True when the canvas changed */
static bool gif_advance(twin_animation_t *anim)
{
    twin_gif_t *gif = (twin_gif_t *) anim;
    if (gif->done)
        return false;
    int got = gif_get_frame(gif);
    if (got > 0)
        return true;
    /* Played out, or a single frame (a still already on the canvas): stop
     * reading past the end on every tick */
    if (!gif->repeats || (!got && gif->frames == 1)) {
        gif->done = true;
        return false;
    }
    if (gif->repeats > 0)
        gif->repeats--;
    /* Past the last frame, or a file that went bad since: start over once.
     * A failure here holds a blank canvas until the next tick tries again. */
    gif_rewind(gif);
    gif_get_frame(gif);
    return true; /* rewound: cleared, at least */
}

static void gif_close(twin_animation_t *anim)
{
    twin_gif_t *gif = (twin_gif_t *) anim;
    fclose(gif->f);
    twin_free(gif->table);
    twin_free(gif->saved);
    twin_free(gif);
}

/* A pixmap holding the first frame, with the animation that redraws it */
static twin_pixmap_t *gif_open(const char *fname)
{
    uint8_t sigver[6];
    uint8_t fdsz;

    twin_gif_t *gif = twin_calloc(1, sizeof(*gif));
    if (!gif)
        return NULL;
    gif->f = fopen(fname, "rb");
    if (!gif->f) {
        log_error("Failed to open %s", fname);
        twin_free(gif);
        return NULL;
    }
    setvbuf(gif->f, NULL, _IOFBF, GIF_IO_BUFFER);

    /* Header and version */
    gif_read(gif, sigver, 6);
    if (memcmp(sigver, "GIF89a", 6) && memcmp(sigver, "GIF87a", 6)) {
        log_error("Invalid signature");
        goto fail;
    }
    /* Width x Height: the canvas, unless a pixmap cannot be that size */
    uint16_t width = read_num(gif), height = read_num(gif);
    gif->anim.pixmap = _twin_pixmap_create_sized(TWIN_ARGB32, width, height);
    if (!gif->anim.pixmap) {
        log_error("Unsupported size %ux%u", width, height);
        goto fail;
    }
    /* FDSZ */
    gif_read(gif, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        log_error("No global color table");
        goto fail;
    }
    /* Background color index and aspect ratio: unused, transparency
     * stands in for the background */
    gif_skip(gif, 2);
    read_palette(gif, &gif->gct, fdsz);
    gif->repeats = -1; /* forever, until a NETSCAPE block gives a count */
    gif->anim_start = ftell(gif->f);

    gif->table = twin_malloc(sizeof(table_t));
    if (!gif->table)
        goto fail;
    gif->anim.advance = gif_advance;
    gif->anim.close = gif_close;
    if (gif_get_frame(gif) <= 0)
        goto fail;
    gif->anim.pixmap->animation = &gif->anim;
    return gif->anim.pixmap;

fail:
    /* The canvas does not own the decoder yet: free them apart */
    if (gif->anim.pixmap)
        twin_pixmap_destroy(gif->anim.pixmap);
    gif_close(&gif->anim);
    return NULL;
}

twin_pixmap_t *_twin_gif_to_pixmap(const char *filepath, twin_format_t fmt)
{
    /* Current implementation only produces TWIN_ARGB32 */
    if (fmt != TWIN_ARGB32)
        return NULL;
    return gif_open(filepath);
}
