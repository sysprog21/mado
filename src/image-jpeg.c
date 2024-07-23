/*                                                                                                                                                            
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>

#include <jpeglib.h>

#include "twin_private.h"

/* colorspace definitions matching libjpeg */
typedef enum {
    TWIN_JCS_UNKNOWN,   /* error/unspecified */
    TWIN_JCS_GRAYSCALE, /* monochrome */
    TWIN_JCS_RGB,       /* red/green/blue */
    TWIN_JCS_YCbCr,     /* Y/Cb/Cr (also known as YUV) */
    TWIN_JCS_CMYK,      /* C/M/Y/K */
    TWIN_JCS_YCCK       /* Y/Cb/Cr/K */
} twin_jpeg_cspace_t;

#if BITS_IN_JSAMPLE != 8
#error This implementation supports only libjpeg with 8 bits per sample.
#endif

struct twin_jpeg_err_mgr {
    struct jpeg_error_mgr mgr;
    jmp_buf jbuf;
};

static void twin_jpeg_error_exit(j_common_ptr cinfo)
{
    struct twin_jpeg_err_mgr *jerr = (struct twin_jpeg_err_mgr *) cinfo->err;

    /* Optionally display the error message */
    (*cinfo->err->output_message)(cinfo);

    longjmp(jerr->jbuf, 1);
}

twin_pixmap_t *twin_jpeg_to_pixmap(const char *filepath, twin_format_t fmt)
{
    twin_pixmap_t *pix = NULL;

    /* Current implementation only produces TWIN_ARGB32 and TWIN_A8 */
    if (fmt != TWIN_ARGB32 && fmt != TWIN_A8)
        return NULL;

    FILE *infile = fopen(filepath, "rb");
    if (!infile) {
        fprintf(stderr, "Failed to open %s\n", filepath);
        return NULL;
    }

    /* Error handling */
    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));
    struct twin_jpeg_err_mgr jerr = {.mgr.error_exit = twin_jpeg_error_exit};
    cinfo.err = jpeg_std_error(&jerr.mgr);
    if (setjmp(jerr.jbuf)) {
        fprintf(stderr, "Failed to decode %s\n", filepath);
        if (pix)
            twin_pixmap_destroy(pix);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }

    /* Initialize libjpeg, hook it up to file, and read header */
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    (void) jpeg_read_header(&cinfo, true);

    /* Configure */
    twin_coord_t width = cinfo.image_width, height = cinfo.image_height;
    if (fmt == TWIN_ARGB32)
        cinfo.out_color_space = JCS_RGB;
    else
        cinfo.out_color_space = JCS_GRAYSCALE;

    /* Allocate pixmap */
    pix = twin_pixmap_create(fmt, width, height);
    if (!pix)
        longjmp(jerr.jbuf, 1);

    /* Start decompressing */
    (void) jpeg_start_decompress(&cinfo);

    if ((fmt == TWIN_A8 && cinfo.output_components != 1) ||
        (fmt == TWIN_ARGB32 &&
         (cinfo.output_components != 3 && cinfo.output_components != 4)))
        longjmp(jerr.jbuf, 1);

    int rowstride = cinfo.output_width * cinfo.output_components;

    JSAMPARRAY rowbuf = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                                   JPOOL_IMAGE, rowstride, 1);

    /* Process rows */
    while (cinfo.output_scanline < cinfo.output_height) {
        twin_pointer_t p = twin_pixmap_pointer(pix, 0, cinfo.output_scanline);
        (void) jpeg_read_scanlines(&cinfo, rowbuf, 1);
        if (fmt == TWIN_A8 || cinfo.output_components == 4)
            memcpy(p.a8, rowbuf, rowstride);
        else {
            JSAMPLE *s = *rowbuf;
            for (int i = 0; i < width; i++) {
                uint32_t r = *(s++);
                uint32_t g = *(s++);
                uint32_t b = *(s++);
                *(p.argb32++) = 0xFF000000U | (r << 16) | (g << 8) | b;
            }
        }
    }

    /* clean up */
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return pix;
}
