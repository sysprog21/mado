/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 *
 * This file integrates the GIF decoder originally developed by Marcel Rodrigues
 * as part of the gifdec project. You can find the original gifdec project at:
 * https://github.com/lecram/gifdec
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "twin.h"
#include "twin_private.h"

typedef struct _twin_gif_palette {
    int size;
    uint8_t colors[0x100 * 3];
} twin_gif_palette_t;

typedef struct _twin_gif_gce {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} twin_gif_gce_t;

typedef struct _twin_gif {
    int fd;
    off_t anim_start;
    twin_coord_t width, height;
    twin_coord_t depth;
    twin_count_t loop_count;
    twin_gif_gce_t gce;
    twin_gif_palette_t *palette;
    twin_gif_palette_t lct, gct;
    uint16_t fx, fy, fw, fh;
    uint8_t bgindex;
    uint8_t *canvas, *frame;
} twin_gif_t;

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct {
    uint16_t length;
    uint16_t prefix;
    uint8_t suffix;
} entry_t;

typedef struct {
    int bulk;
    int n_entries;
    entry_t *entries;
} table_t;

static uint16_t read_num(int fd)
{
    uint8_t bytes[2];

    read(fd, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

static twin_gif_t *_twin_gif_open(const char *fname)
{
    int fd;
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int i;
    uint8_t *bgcolor;
    int gct_sz;
    twin_gif_t *gif;

    fd = open(fname, O_RDONLY);
    if (fd == -1)
        return NULL;
#ifdef _WIN32
    setmode(fd, O_BINARY);
#endif
    /* Header */
    read(fd, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        fprintf(stderr, "invalid signature\n");
        goto fail;
    }
    /* Version */
    read(fd, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        fprintf(stderr, "invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width = read_num(fd);
    height = read_num(fd);
    /* FDSZ */
    read(fd, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        fprintf(stderr, "no global color table_t\n");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    read(fd, &bgidx, 1);
    /* Aspect Ratio */
    read(fd, &aspect, 1);
    /* Create twin_gif_t Structure. */
    gif = calloc(1, sizeof(*gif));
    if (!gif)
        goto fail;
    gif->fd = fd;
    gif->width = width;
    gif->height = height;
    gif->depth = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    read(fd, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->frame = calloc(4, width * height);
    if (!gif->frame) {
        free(gif);
        goto fail;
    }
    gif->canvas = &gif->frame[width * height];
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    bgcolor = &gif->palette->colors[gif->bgindex * 3];
    if (bgcolor[0] || bgcolor[1] || bgcolor[2])
        for (i = 0; i < gif->width * gif->height; i++)
            memcpy(&gif->canvas[i * 3], bgcolor, 3);
    gif->anim_start = lseek(fd, 0, SEEK_CUR);
    goto ok;
fail:
    close(fd);
    return NULL;
ok:
    return gif;
}

static void discard_sub_blocks(twin_gif_t *gif)
{
    uint8_t size;

    do {
        read(gif->fd, &size, 1);
        lseek(gif->fd, size, SEEK_CUR);
    } while (size);
}

static table_t *new_table_t(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    table_t *table_t = malloc(sizeof(*table_t) + sizeof(entry_t) * init_bulk);
    if (table_t) {
        table_t->bulk = init_bulk;
        table_t->n_entries = (1 << key_size) + 2;
        table_t->entries = (entry_t *) &table_t[1];
        for (key = 0; key < (1 << key_size); key++)
            table_t->entries[key] = (entry_t){1, 0xFFF, key};
    }
    return table_t;
}

/* Add table_t entry_t. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table_t
 */
static int add_entry_t(table_t **table_tp,
                       uint16_t length,
                       uint16_t prefix,
                       uint8_t suffix)
{
    table_t *table_t = *table_tp;
    if (table_t->n_entries == table_t->bulk) {
        table_t->bulk *= 2;
        table_t = realloc(table_t,
                          sizeof(*table_t) + sizeof(entry_t) * table_t->bulk);
        if (!table_t)
            return -1;
        table_t->entries = (entry_t *) &table_t[1];
        *table_tp = table_t;
    }
    table_t->entries[table_t->n_entries] = (entry_t){length, prefix, suffix};
    table_t->n_entries++;
    if ((table_t->n_entries & (table_t->n_entries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t get_key(twin_gif_t *gif,
                        int key_size,
                        uint8_t *sub_len,
                        uint8_t *shift,
                        uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0) {
                read(gif->fd, sub_len, 1); /* Must be nonzero! */
                if (*sub_len == 0)
                    return 0x1000;
            }
            read(gif->fd, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table_t).
 */
static int read_image_data(twin_gif_t *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_t_is_full;
    int frm_off, frm_size, str_len, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    table_t *table_t;
    entry_t entry_t;
    off_t start, end;

    read(gif->fd, &byte, 1);
    key_size = (int) byte;
    if (key_size < 2 || key_size > 8)
        return -1;

    start = lseek(gif->fd, 0, SEEK_CUR);
    discard_sub_blocks(gif);
    end = lseek(gif->fd, 0, SEEK_CUR);
    lseek(gif->fd, start, SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table_t = new_table_t(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw * gif->fh;
    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size;
            table_t->n_entries = (1 << (key_size - 1)) + 2;
            table_t_is_full = 0;
        } else if (!table_t_is_full) {
            ret = add_entry_t(&table_t, str_len + 1, key, entry_t.suffix);
            if (ret == -1) {
                free(table_t);
                return -1;
            }
            if (table_t->n_entries == 0x1000) {
                ret = 0;
                table_t_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear)
            continue;
        if (key == stop || key == 0x1000)
            break;
        if (ret == 1)
            key_size++;
        entry_t = table_t->entries[key];
        str_len = entry_t.length;
        for (i = 0; i < str_len; i++) {
            p = frm_off + entry_t.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] =
                entry_t.suffix;
            if (entry_t.prefix == 0xFFF)
                break;
            else
                entry_t = table_t->entries[entry_t.prefix];
        }
        frm_off += str_len;
        if (key < table_t->n_entries - 1 && !table_t_is_full)
            table_t->entries[table_t->n_entries - 1].suffix = entry_t.suffix;
    }
    free(table_t);
    if (key == stop)
        read(gif->fd, &sub_len, 1); /* Must be zero! */
    lseek(gif->fd, end, SEEK_SET);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table_t).
 */
static int read_image(twin_gif_t *gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    gif->fx = read_num(gif->fd);
    gif->fy = read_num(gif->fd);

    if (gif->fx >= gif->width || gif->fy >= gif->height)
        return -1;

    gif->fw = read_num(gif->fd);
    gif->fh = read_num(gif->fd);

    gif->fw = MIN(gif->fw, gif->width - gif->fx);
    gif->fh = MIN(gif->fh, gif->height - gif->fy);

    read(gif->fd, &fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color table_t? */
    if (fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        read(gif->fd, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    /* Image Data. */
    return read_image_data(gif, interlace);
}

static void render_frame_rect(twin_gif_t *gif, uint8_t *buffer)
{
    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index * 3];
            if (!gif->gce.transparency || index != gif->gce.tindex)
                memcpy(&buffer[(i + k) * 3], color, 3);
        }
        i += gif->width;
    }
}

static void dispose(twin_gif_t *gif)
{
    int i, j, k;
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: /* Restore to background color. */
        bgcolor = &gif->palette->colors[gif->bgindex * 3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++)
                memcpy(&gif->canvas[(i + k) * 3], bgcolor, 3);
            i += gif->width;
        }
        break;
    case 3: /* Restore to previous, i.e., don't update canvas.*/
        break;
    default:
        /* Add frame non-transparent pixels to canvas. */
        render_frame_rect(gif, gif->canvas);
    }
}

static void read_plain_text_ext(twin_gif_t *gif)
{
    /* Discard plain text metadata. */
    lseek(gif->fd, 13, SEEK_CUR);
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void read_graphic_control_ext(twin_gif_t *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    lseek(gif->fd, 1, SEEK_CUR);
    read(gif->fd, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    read(gif->fd, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    lseek(gif->fd, 1, SEEK_CUR);
}

static void read_comment_ext(twin_gif_t *gif)
{
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void read_application_ext(twin_gif_t *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    lseek(gif->fd, 1, SEEK_CUR);
    /* Application Identifier. */
    read(gif->fd, app_id, 8);
    /* Application Authentication Code. */
    read(gif->fd, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        lseek(gif->fd, 2, SEEK_CUR);
        gif->loop_count = read_num(gif->fd);
        /* Skip block terminator. */
        lseek(gif->fd, 1, SEEK_CUR);
    } else {
        discard_sub_blocks(gif);
    }
}

static void read_ext(twin_gif_t *gif)
{
    uint8_t label;

    read(gif->fd, &label, 1);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        fprintf(stderr, "unknown extension: %02X\n", label);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
static int _twin_gif_get_frame(twin_gif_t *gif)
{
    char sep;

    dispose(gif);
    read(gif->fd, &sep, 1);
    while (sep != ',') {
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else
            return -1;
        read(gif->fd, &sep, 1);
    }
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

static void _twin_gif_render_frame(twin_gif_t *gif, uint8_t *buffer)
{
    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

static int _twin_gif_is_bgcolor(twin_gif_t *gif, uint8_t color[3])
{
    return !memcmp(&gif->palette->colors[gif->bgindex * 3], color, 3);
}

static void _twin_gif_rewind(twin_gif_t *gif)
{
    lseek(gif->fd, gif->anim_start, SEEK_SET);
}

static void _twin_gif_close(twin_gif_t *gif)
{
    close(gif->fd);
    free(gif->frame);
    free(gif);
}

static twin_animation_t *_twin_animation_from_gif_file(const char *path)
{
    twin_animation_t *anim = malloc(sizeof(twin_animation_t));
    if (!anim)
        return NULL;

    twin_gif_t *gif = _twin_gif_open(path);
    if (!gif) {
        free(anim);
        return NULL;
    }

    anim->n_frames = 0;
    anim->frames = NULL;
    anim->frame_delays = NULL;
    anim->loop = gif->loop_count == 0;
    anim->width = gif->width;
    anim->height = gif->height;

    int frame_count = 0;
    while (_twin_gif_get_frame(gif)) {
        frame_count++;
    }

    anim->n_frames = frame_count;
    anim->frames = malloc(sizeof(twin_pixmap_t *) * frame_count);
    anim->frame_delays = malloc(sizeof(twin_time_t) * frame_count);

    _twin_gif_rewind(gif);
    uint8_t *color, *frame;
    frame = malloc(gif->width * gif->height * 3);
    if (!frame) {
        free(anim);
        _twin_gif_close(gif);
        return NULL;
    }
    for (twin_count_t i = 0; i < frame_count; i++) {
        anim->frames[i] =
            twin_pixmap_create(TWIN_ARGB32, gif->width, gif->height);
        if (!anim->frames[i]) {
            free(anim);
            _twin_gif_close(gif);
            return NULL;
        }
        anim->frames[i]->format = TWIN_ARGB32;

        _twin_gif_render_frame(gif, frame);
        color = frame;
        twin_pointer_t p = twin_pixmap_pointer(anim->frames[i], 0, 0);
        twin_coord_t row = 0, col = 0;
        for (int j = 0; j < gif->width * gif->height; j++) {
            uint8_t r = *(color++);
            uint8_t g = *(color++);
            uint8_t b = *(color++);
            if (!_twin_gif_is_bgcolor(gif, color))
                *(p.argb32++) = 0xFF000000U | (r << 16) | (g << 8) | b;
            /* Construct background */
            else if (((row >> 3) + (col >> 3)) & 1)
                *(p.argb32++) = 0xFFAFAFAFU;
            else
                *(p.argb32++) = 0xFF7F7F7FU;
            col++;
            if (col == gif->width) {
                row++;
                col = 0;
            }
        }
        /* GIF delay in units of 1/100 second */
        anim->frame_delays[i] = gif->gce.delay * 10;
        _twin_gif_get_frame(gif);
    }
    anim->iter = twin_animation_iter_init(anim);
    if (!anim->iter) {
        free(anim);
        _twin_gif_close(gif);
        return NULL;
    }
    _twin_gif_close(gif);
    return anim;
}

twin_pixmap_t *_twin_gif_to_pixmap(const char *filepath, twin_format_t fmt)
{
    twin_pixmap_t *pix = NULL;

    /* Current implementation only produces TWIN_ARGB32 */
    if (fmt != TWIN_ARGB32)
        return NULL;
    FILE *infile = fopen(filepath, "rb");
    if (!infile) {
        fprintf(stderr, "Failed to open %s\n", filepath);
        return NULL;
    }
    twin_animation_t *gif = _twin_animation_from_gif_file(filepath);
    if (gif) {
        /* Allocate pixmap */
        pix = twin_pixmap_create(fmt, gif->width, gif->height);
        if (pix)
            pix->animation = gif;
    }
    fclose(infile);

    return pix;
}
