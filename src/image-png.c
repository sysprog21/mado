/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <png.h>
#include <stdlib.h>
#include <unistd.h>

#include "twin_private.h"

/* libpng hands out straight alpha; twin composites premultiplied ARGB. */
static twin_argb32_t _twin_apply_alpha(twin_argb32_t v)
{
    uint16_t t1, t2, t3;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    twin_a8_t alpha = twin_get_8(v, 0);
#else
    twin_a8_t alpha = twin_get_8(v, 24);
#endif

    /* clear RGB data if alpha is zero */
    if (!alpha)
        return 0;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return (uint32_t) alpha << 24 |
           twin_int_mult(twin_get_8(v, 24), alpha, t1) << 16 |
           twin_int_mult(twin_get_8(v, 16), alpha, t2) << 8 |
           twin_int_mult(twin_get_8(v, 8), alpha, t3) << 0;
#else
    return (uint32_t) alpha << 24 |
           twin_int_mult(twin_get_8(v, 0), alpha, t1) << 16 |
           twin_int_mult(twin_get_8(v, 8), alpha, t2) << 8 |
           twin_int_mult(twin_get_8(v, 16), alpha, t3) << 0;
#endif
}

static void twin_premultiply_alpha(twin_pixmap_t *px)
{
    if (px->format != TWIN_ARGB32)
        return;

    for (twin_coord_t y = 0; y < px->height; y++) {
        twin_pointer_t p = {.b = px->p.b + y * px->stride};

        for (twin_coord_t x = 0; x < px->width; x++)
            p.argb32[x] = _twin_apply_alpha(p.argb32[x]);
    }
}

typedef struct {
    int fd;
} twin_png_priv_t;

static void _twin_png_read(png_structp png, png_bytep data, png_size_t size)
{
    twin_png_priv_t *priv = png_get_io_ptr(png);
    ssize_t n = read(priv->fd, data, size);
    if (n <= 0 || (png_size_t) n < size)
        png_error(png, "end of file !\n");
}

/* FIXME: Utilize pixman or similar accelerated routine to convert */
#if defined(__APPLE__)
static void _convertBGRtoARGB(uint8_t *data, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 4;
            uint8_t b = data[index];
            uint8_t g = data[index + 1];
            uint8_t r = data[index + 2];
            data[index] = 255; /* Alpha */
            data[index + 1] = r;
            data[index + 2] = g;
            data[index + 3] = b;
        }
    }
}
#endif

twin_pixmap_t *_twin_png_to_pixmap(const char *filepath, twin_format_t fmt)
{
    uint8_t signature[8];
    png_structp png = NULL;
    png_infop info = NULL;
    /* Assigned after setjmp(): volatile so the error path sees them */
    twin_pixmap_t *volatile pix = NULL;
    png_bytep *volatile rowp = NULL;
    int depth, ctype, interlace;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0)
        goto bail;

    size_t n = read(fd, signature, 8);
    if (png_sig_cmp(signature, 0, n) != 0)
        goto bail_close;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        goto bail_close;

    info = png_create_info_struct(png);
    if (!info)
        goto bail_free;

    if (setjmp(png_jmpbuf(png))) {
        if (pix)
            twin_pixmap_destroy(pix);
        pix = NULL;
        goto bail_free;
    }

    twin_png_priv_t priv = {.fd = fd};
    png_set_read_fn(png, &priv, _twin_png_read);

    png_set_sig_bytes(png, n);
    /* A pixmap's sides are twin_coord_t: refuse anything larger up front */
    png_set_user_limits(png, INT16_MAX, INT16_MAX);

    png_read_info(png, info);
    png_uint_32 width, height;
    png_get_IHDR(png, info, &width, &height, &depth, &ctype, &interlace, NULL,
                 NULL);
    if (!width || !height || width > INT16_MAX || height > INT16_MAX)
        goto bail_free;

    if (depth == 16)
        png_set_strip_16(png);
    if (ctype == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    png_get_IHDR(png, info, &width, &height, &depth, &ctype, &interlace, NULL,
                 NULL);

    switch (fmt) {
    case TWIN_A8:
        if (ctype != PNG_COLOR_TYPE_GRAY || depth != 8)
            goto bail_free;
        break;
    case TWIN_RGB16:
        /* unsupported for now */
        goto bail_free;
    case TWIN_ARGB32:
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);
        if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);

        png_get_IHDR(png, info, &width, &height, &depth, &ctype, &interlace,
                     NULL, NULL);

        if (depth != 8)
            goto bail_free;
        break;
    }

    rowp = twin_malloc((size_t) height * sizeof(png_bytep));
    if (!rowp)
        goto bail_free;
    pix = twin_pixmap_create(fmt, (twin_coord_t) width, (twin_coord_t) height);
    if (!pix)
        goto bail_free;
    /* libpng writes width * twin_bytes_per_pixel(fmt) bytes per row; rows
     * sit at the pixmap's stride, which is at least that, maybe padded */
    for (twin_coord_t i = 0; i < pix->height; i++)
        rowp[i] = pix->p.b + (size_t) pix->stride * i;

    png_read_image(png, rowp);

    png_read_end(png, NULL);

    if (fmt == TWIN_ARGB32) {
        /* Convert from BGR to ARGB if necessary */
#if defined(__APPLE__)
        _convertBGRtoARGB(pix->p.b, width, height);
#endif
        twin_premultiply_alpha(pix);
    }
    goto bail_free;

bail_free:
    twin_free(rowp);
    png_destroy_read_struct(&png, &info, NULL);
bail_close:
    close(fd);
bail:
    return pix;
}
