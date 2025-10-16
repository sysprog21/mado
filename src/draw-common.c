/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

#define TWIN_TITLE_HEIGHT 20

static void _twin_apply_stack_blur(twin_pixmap_t *trg_px,
                                   twin_pixmap_t *src_px,
                                   int radius,
                                   int first_str,
                                   int first_end,
                                   int second_str,
                                   int second_end,
                                   bool horiz_scan)
{
    int den = (radius + 1) * (radius + 1);
    twin_pointer_t src_ptr, trg_ptr, old_ptr, new_ptr;
    twin_argb32_t sumInR, sumOutR, sumR, sumInG, sumOutG, sumG, sumInB, sumOutB,
        sumB, sumInA, sumOutA, sumA, _cur, _old, _new, _src;
    uint16_t tmpR, tmpG, tmpB, tmpA;
    for (int first = first_str; first < first_end; first++) {
        sumInR = sumOutR = sumR = sumInG = sumOutG = sumG = sumInB = sumOutB =
            sumB = sumInA = sumOutA = sumA = 0x00000000;

        /* Initialize sumOut by padding. */
        if (horiz_scan)
            src_ptr = twin_pixmap_pointer(src_px, second_str, first);
        else
            src_ptr = twin_pixmap_pointer(src_px, first, second_str);
        _src = *src_ptr.argb32;

        for (int i = second_str; i < second_str + radius; i++) {
            sumOutR = _twin_add_ARGB(sumOutR, _src, 0, tmpR);
            sumOutG = _twin_add_ARGB(sumOutG, _src, 8, tmpG);
            sumOutB = _twin_add_ARGB(sumOutB, _src, 16, tmpB);
            sumOutA = _twin_add_ARGB(sumOutA, _src, 24, tmpA);
            for (int j = 0; j < (i - second_str) + 1; j++) {
                sumR = _twin_add_ARGB(sumR, _src, 0, tmpR);
                sumG = _twin_add_ARGB(sumG, _src, 8, tmpG);
                sumB = _twin_add_ARGB(sumB, _src, 16, tmpB);
                sumA = _twin_add_ARGB(sumA, _src, 24, tmpA);
            }
        }

        /* Initialize sumIn. */
        for (int i = second_str; i < second_str + radius; i++) {
            if (horiz_scan)
                src_ptr = twin_pixmap_pointer(src_px, i, first);
            else
                src_ptr = twin_pixmap_pointer(src_px, first, i);
            _src = *src_ptr.argb32;
            sumInR = _twin_add_ARGB(sumInR, _src, 0, tmpR);
            sumInG = _twin_add_ARGB(sumInG, _src, 8, tmpG);
            sumInB = _twin_add_ARGB(sumInB, _src, 16, tmpB);
            sumInA = _twin_add_ARGB(sumInA, _src, 24, tmpA);
            for (int j = 0; j < radius - (i - second_str); j++) {
                sumR = _twin_add_ARGB(sumR, _src, 0, tmpR);
                sumG = _twin_add_ARGB(sumG, _src, 8, tmpG);
                sumB = _twin_add_ARGB(sumB, _src, 16, tmpB);
                sumA = _twin_add_ARGB(sumA, _src, 24, tmpA);
            }
        }

        for (int cur = second_str; cur < second_end; cur++) {
            if (horiz_scan) {
                src_ptr = twin_pixmap_pointer(src_px, cur, first);
                trg_ptr = twin_pixmap_pointer(trg_px, cur, first);
                old_ptr = twin_pixmap_pointer(
                    src_px, max(cur - radius, second_str), first);
                new_ptr = twin_pixmap_pointer(
                    src_px, min(cur + radius, second_end - 1), first);
            } else {
                src_ptr = twin_pixmap_pointer(src_px, first, cur);
                trg_ptr = twin_pixmap_pointer(trg_px, first, cur);
                old_ptr = twin_pixmap_pointer(src_px, first,
                                              max(cur - radius, second_str));
                new_ptr = twin_pixmap_pointer(
                    src_px, first, min(cur + radius, second_end - 1));
            }
            _cur = *src_ptr.argb32;
            _old = *old_ptr.argb32;
            _new = *new_ptr.argb32;
            /* STEP 1 : sumOut + current */
            sumOutR = _twin_add_ARGB(sumOutR, _cur, 0, tmpR);
            sumOutG = _twin_add_ARGB(sumOutG, _cur, 8, tmpG);
            sumOutB = _twin_add_ARGB(sumOutB, _cur, 16, tmpB);
            sumOutA = _twin_add_ARGB(sumOutA, _cur, 24, tmpA);
            /* STEP 2 : sumIn + new */
            sumInR = _twin_add_ARGB(sumInR, _new, 0, tmpR);
            sumInG = _twin_add_ARGB(sumInG, _new, 8, tmpG);
            sumInB = _twin_add_ARGB(sumInB, _new, 16, tmpB);
            sumInA = _twin_add_ARGB(sumInA, _new, 24, tmpA);
            /* STEP 3 : sum + sumIn */
            sumR = _twin_add(sumR, sumInR, tmpR);
            sumG = _twin_add(sumG, sumInG, tmpG);
            sumB = _twin_add(sumB, sumInB, tmpB);
            sumA = _twin_add(sumA, sumInA, tmpA);
            /* STEP 4 : sum / denominator */
            *trg_ptr.argb32 =
                (_twin_div(sumR, den, 0, tmpR) | _twin_div(sumG, den, 8, tmpG) |
                 _twin_div(sumB, den, 16, tmpB) |
                 _twin_div(sumA, den, 24, tmpA));
            /* STEP 5 : sum - sumOut */
            sumR = _twin_sub(sumR, sumOutR, tmpR);
            sumG = _twin_sub(sumG, sumOutG, tmpG);
            sumB = _twin_sub(sumB, sumOutB, tmpB);
            sumA = _twin_sub(sumA, sumOutA, tmpA);
            /* STEP 6 : sumOut - old */
            sumOutR = _twin_sub_ARGB(sumOutR, _old, 0, tmpR);
            sumOutG = _twin_sub_ARGB(sumOutG, _old, 8, tmpG);
            sumOutB = _twin_sub_ARGB(sumOutB, _old, 16, tmpB);
            sumOutA = _twin_sub_ARGB(sumOutA, _old, 24, tmpA);
            /* STEP 7 : sumIn - current */
            sumInR = _twin_sub_ARGB(sumInR, _cur, 0, tmpR);
            sumInG = _twin_sub_ARGB(sumInG, _cur, 8, tmpG);
            sumInB = _twin_sub_ARGB(sumInB, _cur, 16, tmpB);
            sumInA = _twin_sub_ARGB(sumInA, _cur, 24, tmpA);
        }
    }
}

void twin_stack_blur(twin_pixmap_t *px,
                     int radius,
                     twin_coord_t left,
                     twin_coord_t right,
                     twin_coord_t top,
                     twin_coord_t bottom)
{
    if (px->format != TWIN_ARGB32)
        return;
    twin_pixmap_t *tmp_px =
        twin_pixmap_create(px->format, px->width, px->height);
    memcpy(tmp_px->p.v, px->p.v,
           px->width * px->height * twin_bytes_per_pixel(px->format));
    /*
     * Originally, performing a 2D convolution on each pixel takes O(width *
     * height * k²). However, by first scanning horizontally and then vertically
     * across the pixel map, and applying a 1D convolution to each pixel, the
     * complexity is reduced to O(2 * width * height * k).
     */
    /* Horizontally scan. */
    _twin_apply_stack_blur(tmp_px, px, radius, top, bottom, left, right, true);
    /* Vertically scan. */
    _twin_apply_stack_blur(px, tmp_px, radius, left, right, top, bottom, false);
    twin_pixmap_destroy(tmp_px);
    return;
}

#if defined(CONFIG_DROP_SHADOW)
void twin_shadow_border(twin_pixmap_t *shadow,
                        twin_argb32_t color,
                        twin_coord_t offset_x,
                        twin_coord_t offset_y)
{
    twin_coord_t right_edge = (*shadow).width - (*shadow).window->shadow_x,
                 bottom_edge = (*shadow).height - (*shadow).window->shadow_y,
                 right_span = right_edge, clip, corner_offset, y_start,
                 offset = min(offset_x, offset_y);

    switch (shadow->window->style) {
    /*
     * Draw a black border starting from the top y position of the window's
     * client area plus CONFIG_SHADOW_BLUR / 2 + 1, to prevent twin_stack_blur()
     * from blurring shadows over the window frame.
     */
    case TwinWindowApplication:
        y_start = TWIN_TITLE_HEIGHT + CONFIG_SHADOW_BLUR / 2 + 1;
        break;
    case TwinWindowPlain:
    default:
        y_start = CONFIG_SHADOW_BLUR / 2 + 1;
        break;
    }

    for (twin_coord_t y = y_start; y < (*shadow).height; y++) {
        /* Render the right edge of the shadow border. */
        if (y < bottom_edge) {
            /*
             * Create a shadow with a diagonal shape, extending from the
             * top-left to the bottom-right.
             */
            clip = min((twin_coord_t) (y - y_start), offset_x);
            twin_cover(shadow, color, right_edge, y, clip);
        } else {
            /* Calculate the range of the corner. */
            right_span++;
        }
        /* Render the bottom edge of the shadow border. */
        if (y >= bottom_edge && y < bottom_edge + offset_y) {
            /*
             * Create a shadow with a diagonal shape, extending from the
             * top-left to the bottom-right.
             */
            clip = max(0, y - bottom_edge);
            twin_cover(shadow, color, clip, y, right_edge - clip);
            /* Render the bottom-right corner of the shadow border. */
            /*
             * Handle the case where the vertical shadow offset is larger than
             * the horizontal shadow offset.
             */
            corner_offset =
                min((twin_coord_t) (right_span - right_edge), offset);
            for (twin_coord_t i = 0; i < corner_offset; i++) {
                /* The corner's pixels are symmetrical to the diagonal. */
                twin_cover(shadow, color, right_edge + i, y, 1);
                twin_cover(shadow, color, right_edge + (y - bottom_edge),
                           bottom_edge + i, 1);
            }
        }
    }
}
#endif

static twin_argb32_t _twin_apply_alpha(twin_argb32_t v)
{
    uint16_t t1, t2, t3;
    twin_a8_t alpha = twin_get_8(v,
#if __BYTE_ORDER == __BIG_ENDIAN
                                 0
#else
                                 24
#endif
    );

    /* clear RGB data if alpha is zero */
    if (!alpha)
        return 0;

#if __BYTE_ORDER == __BIG_ENDIAN
    /* twin needs ARGB format */
    return alpha << 24 | twin_int_mult(twin_get_8(v, 24), alpha, t1) << 16 |
           twin_int_mult(twin_get_8(v, 16), alpha, t2) << 8 |
           twin_int_mult(twin_get_8(v, 8), alpha, t3) << 0;
#else
    return alpha << 24 | twin_int_mult(twin_get_8(v, 0), alpha, t1) << 16 |
           twin_int_mult(twin_get_8(v, 8), alpha, t2) << 8 |
           twin_int_mult(twin_get_8(v, 16), alpha, t3) << 0;
#endif
}

void twin_premultiply_alpha(twin_pixmap_t *px)
{
    if (px->format != TWIN_ARGB32)
        return;

    for (twin_coord_t y = 0; y < px->height; y++) {
        twin_pointer_t p = {.b = px->p.b + y * px->stride};

        for (twin_coord_t x = 0; x < px->width; x++)
            p.argb32[x] = _twin_apply_alpha(p.argb32[x]);
    }
}

void twin_cover(twin_pixmap_t *dst,
                twin_argb32_t color,
                twin_coord_t x,
                twin_coord_t y,
                twin_coord_t width)
{
    if (x < 0 || y < 0 || width < 0 || x + width > dst->width ||
        y >= dst->height)
        return;
    for (twin_coord_t i = 0; i < width; i++) {
        twin_pointer_t pt = twin_pixmap_pointer(dst, x + i, y);
        *pt.argb32 = color;
    }
}
