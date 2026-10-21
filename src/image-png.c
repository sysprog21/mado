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

/* libpng hands out straight alpha; twin composites premultiplied ARGB.
 * Pixels arrive in native ARGB order, so only the channels are scaled. */
static void twin_premultiply_alpha(twin_pixmap_t *px)
{
    for (twin_coord_t y = 0; y < px->height; y++) {
        twin_argb32_t *p = twin_pixmap_pointer(px, 0, y).argb32;

        for (twin_coord_t x = 0; x < px->width; x++) {
            twin_argb32_t v = p[x];
            twin_a8_t alpha = v >> 24;
            uint16_t t1, t2, t3;
            p[x] = (uint32_t) alpha << 24 | twin_in(v, 16, alpha, t1) |
                   twin_in(v, 8, alpha, t2) | twin_in(v, 0, alpha, t3);
        }
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

twin_pixmap_t *_twin_png_to_pixmap(const char *filepath, twin_format_t fmt)
{
    uint8_t signature[8];
    png_structp png = NULL;
    png_infop info = NULL;
    /* Assigned after setjmp(): volatile so the error path sees them */
    twin_pixmap_t *volatile pix = NULL;
    png_bytep *volatile rowp = NULL;
    int depth, ctype;

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
    png_get_IHDR(png, info, &width, &height, &depth, &ctype, NULL, NULL, NULL);

    if (depth == 16)
        png_set_strip_16(png);
    if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    switch (fmt) {
    case TWIN_A8:
        if (ctype != PNG_COLOR_TYPE_GRAY)
            goto bail_free;
        break;
    case TWIN_RGB16:
        /* unsupported for now */
        goto bail_free;
    case TWIN_ARGB32:
        if (ctype == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        /* Native twin_argb32_t: bytes B, G, R, A on little endian,
         * A, R, G, B on big endian */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        png_set_swap_alpha(png);
        png_set_filler(png, 0xff, PNG_FILLER_BEFORE);
#else
        png_set_bgr(png);
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);
#endif
        break;
    }
    png_set_interlace_handling(png);

    /* IHDR keeps the file's layout until the transforms are applied; check
     * what libpng will actually write against the pixmap's row size */
    png_read_update_info(png, info);
    if (png_get_rowbytes(png, info) != width * twin_bytes_per_pixel(fmt))
        goto bail_free;

    rowp = twin_malloc((size_t) height * sizeof(png_bytep));
    if (!rowp)
        goto bail_free;
    pix = _twin_pixmap_create_sized(fmt, width, height);
    if (!pix)
        goto bail_free;
    /* libpng writes width * twin_bytes_per_pixel(fmt) bytes per row; rows
     * sit at the pixmap's stride, which is at least that, maybe padded */
    for (twin_coord_t i = 0; i < pix->height; i++)
        rowp[i] = pix->p.b + (size_t) pix->stride * i;

    png_read_image(png, rowp);

    png_read_end(png, NULL);

    /* Opaque images (filler, not alpha) are final as read */
    if (fmt == TWIN_ARGB32 &&
        (png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA))
        twin_premultiply_alpha(pix);
    goto bail_free;

bail_free:
    twin_free(rowp);
    png_destroy_read_struct(&png, &info, NULL);
bail_close:
    close(fd);
bail:
    return pix;
}
