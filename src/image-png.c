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
    int rb = 0;
    png_structp png = NULL;
    png_infop info = NULL;
    twin_pixmap_t *pix = NULL;
    int depth, ctype, interlace;
    png_bytep *rowp = NULL;

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

    png_read_info(png, info);
    png_uint_32 width, height;
    png_get_IHDR(png, info, &width, &height, &depth, &ctype, &interlace, NULL,
                 NULL);

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
        rb = width;
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
        rb = width * 4;
        break;
    }

    rowp = malloc(height * sizeof(png_bytep));
    if (!rowp)
        goto bail_free;
    pix = twin_pixmap_create(fmt, width, height);
    if (!pix)
        goto bail_free;
    for (size_t i = 0; i < height; i++)
        rowp[i] = pix->p.b + rb * i;

    png_read_image(png, rowp);

    png_read_end(png, NULL);

    if (fmt == TWIN_ARGB32) {
        /* Convert from BGR to ARGB if necessary */
#if defined(__APPLE__)
        _convertBGRtoARGB(pix->p.b, width, height);
#endif
        twin_premultiply_alpha(pix);
        twin_gaussian_blur(pix);
    }

bail_free:
    free(rowp);
    png_destroy_read_struct(&png, &info, NULL);
bail_close:
    close(fd);
bail:
    return pix;
}
