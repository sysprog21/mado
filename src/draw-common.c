/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>

#include "shadow-gaussian-lut.h"
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
/* Reuse bottom-strip weights between frames when width is unchanged. */
static twin_a8_t *shadow_bottom_cache_weights;
static twin_coord_t shadow_bottom_cache_capacity;
static twin_coord_t shadow_bottom_cache_width;

/* Remember last vertical alpha ramp keyed by offset+alpha. */
static twin_a8_t shadow_alpha_y_cache[(CONFIG_VERTICAL_OFFSET > 0)
                                          ? CONFIG_VERTICAL_OFFSET
                                          : 1];
static twin_coord_t shadow_alpha_y_cache_offset = -1;
static twin_a8_t shadow_alpha_y_cache_alpha;
static bool shadow_alpha_y_cache_valid;

#define SHADOW_LUT_X_LEN (CONFIG_HORIZONTAL_OFFSET + CONFIG_SHADOW_FADE_TAIL)
#define SHADOW_LUT_Y_LEN (CONFIG_VERTICAL_OFFSET + CONFIG_SHADOW_FADE_TAIL)

/* Fast lookup: 17-entry approximation of (1 - t^2)^2 * 0.92 + 0.08. */
static inline twin_fixed_t shadow_gaussian_weight(twin_fixed_t t)
{
    if (t <= 0)
        return shadow_gaussian_lut[0];
    if (t >= TWIN_FIXED_ONE)
        return shadow_gaussian_lut[16];

    int32_t scaled = t;
    int32_t index =
        (int32_t) (((int64_t) scaled * 16 + 0x8000) >> 16); /* rounded */
    if (index < 0)
        index = 0;
    else if (index > 16)
        index = 16;
    return shadow_gaussian_lut[index];
}

/* Renders a shadow effect that emulates the CSS 'box-shadow' property.
 *
 * This function uses precomputed fixed-point lookup tables for performance,
 * approximating a Gaussian blur to create a soft shadow effect. The shadow's
 * appearance is controlled by several compile-time options:
 * - CONFIG_HORIZONTAL_OFFSET : Simulates the 'offset-x' value in CSS,
 *   determining the horizontal displacement of the shadow.
 * - CONFIG_VERTICAL_OFFSET : Simulates the 'offset-y' value, controlling the
 *   vertical displacement.
 * - CONFIG_SHADOW_BLUR : Approximates the 'blur-radius', defining the softness
 *   and extent of the shadow's edge.
 *
 * Limitations:
 * - This implementation does not support the 'spread-radius' or 'inset'
 *   keywords from the CSS 'box-shadow' specification. The shadow is always an
 *   outer shadow, and its initial size is based on the window dimensions.
 * - The shadow quality and performance are tied to the fixed-point precision
 *   and the size of the lookup table (i.e., 'shadow_gaussian_lut').
 */
void twin_shadow_border(twin_pixmap_t *shadow, twin_argb32_t color)
{
    twin_coord_t win_width = shadow->width - shadow->window->shadow_x;
    twin_coord_t win_height = shadow->height - shadow->window->shadow_y;
    twin_coord_t y_start;

    twin_a8_t base_alpha = (color >> 24) & 0xFF;
    twin_argb32_t base_rgb = color & 0x00FFFFFF;

    if (!base_alpha)
        return;

    /* Title-style windows leave the frame untouched by the blur. */
    switch (shadow->window->style) {
    case TwinWindowApplication:
        /* Start below title bar to avoid blurring over window frame */
        y_start = TWIN_TITLE_HEIGHT + CONFIG_SHADOW_BLUR / 2 + 1;
        break;
    case TwinWindowPlain:
    default:
        y_start = CONFIG_SHADOW_BLUR / 2 + 1;
        break;
    }

    const twin_coord_t x_offset = CONFIG_HORIZONTAL_OFFSET;
    const twin_coord_t y_offset = CONFIG_VERTICAL_OFFSET;
    const twin_coord_t fade_tail = CONFIG_SHADOW_FADE_TAIL;
    const twin_coord_t lut_x_len = x_offset + fade_tail;
    const twin_coord_t lut_y_len = y_offset + fade_tail;
    twin_coord_t right_extent = shadow->window->shadow_x;
    twin_coord_t bottom_extent = shadow->window->shadow_y;
    bool is_argb32 = (shadow->format == TWIN_ARGB32);

    if (right_extent <= 0 && bottom_extent <= 0)
        return;

    if (right_extent > lut_x_len)
        right_extent = lut_x_len;
    if (bottom_extent > lut_y_len)
        bottom_extent = lut_y_len;

    /* Clear stale shadow pixels before repainting the mask. */
    if (shadow->window->shadow_x > 0) {
        for (twin_coord_t y = 0; y < win_height; y++) {
            twin_pointer_t clear = twin_pixmap_pointer(shadow, win_width, y);
            if (is_argb32) {
                memset(
                    clear.argb32, 0,
                    (size_t) shadow->window->shadow_x * sizeof(*clear.argb32));
            } else {
                for (twin_coord_t x = 0; x < shadow->window->shadow_x; x++)
                    clear.argb32[x] = 0;
            }
        }
    }
    if (shadow->window->shadow_y > 0) {
        for (twin_coord_t y = win_height; y < shadow->height; y++) {
            twin_pointer_t clear = twin_pixmap_pointer(shadow, 0, y);
            if (is_argb32) {
                memset(clear.argb32, 0,
                       (size_t) shadow->width * sizeof(*clear.argb32));
            } else {
                for (twin_coord_t x = 0; x < shadow->width; x++)
                    clear.argb32[x] = 0;
            }
        }
    }

    /* Fixed-point lookup tables for edge falloffs (small: <= 10 entries). */
#if CONFIG_HORIZONTAL_OFFSET <= 0 || CONFIG_VERTICAL_OFFSET <= 0
#error "Drop shadow offsets must be positive."
#endif
    twin_a8_t alpha_lut_x[SHADOW_LUT_X_LEN]; /* Right-edge fade LUT. */
    twin_a8_t alpha_lut_y[SHADOW_LUT_Y_LEN]; /* Bottom fade LUT. */

    twin_fixed_t step_x =
        (x_offset > 1) ? (TWIN_FIXED_ONE / (x_offset - 1)) : 0;
    twin_fixed_t tx = 0;
    for (twin_coord_t i = 0; i < x_offset; i++) {
        twin_fixed_t weight = shadow_gaussian_weight(tx);
        alpha_lut_x[i] = (twin_a8_t) ((weight * base_alpha) >> 16);
        if (x_offset > 1 && tx < TWIN_FIXED_ONE)
            tx += step_x;
    }
    for (twin_coord_t i = x_offset; i < lut_x_len; i++)
        alpha_lut_x[i] = 0;

    bool reuse_alpha_y = false;
    if (shadow_alpha_y_cache_valid && shadow_alpha_y_cache_offset == y_offset &&
        shadow_alpha_y_cache_alpha == base_alpha) {
        memcpy(alpha_lut_y, shadow_alpha_y_cache,
               (size_t) y_offset * sizeof(*alpha_lut_y));
        reuse_alpha_y = true;
    }

    if (!reuse_alpha_y) {
        twin_fixed_t step_y =
            (y_offset > 1) ? (TWIN_FIXED_ONE / (y_offset - 1)) : 0;
        twin_fixed_t ty = 0;
        for (twin_coord_t i = 0; i < y_offset; i++) {
            twin_fixed_t weight = shadow_gaussian_weight(ty);
            alpha_lut_y[i] = (twin_a8_t) ((weight * base_alpha) >> 16);
            if (y_offset > 1 && ty < TWIN_FIXED_ONE)
                ty += step_y;
        }
        if (y_offset > 0) {
            memcpy(shadow_alpha_y_cache, alpha_lut_y,
                   (size_t) y_offset * sizeof(*alpha_lut_y));
            shadow_alpha_y_cache_offset = y_offset;
            shadow_alpha_y_cache_alpha = base_alpha;
            shadow_alpha_y_cache_valid = true;
        }
    }
    for (twin_coord_t i = y_offset; i < lut_y_len; i++)
        alpha_lut_y[i] = 0;

    /* Right edge strip */
    if (right_extent > 0 && y_start < shadow->height) {
        twin_coord_t y_end = win_height;
        if (y_end > shadow->height)
            y_end = shadow->height;
        for (twin_coord_t y = y_start; y < y_end; y++) {
            if (is_argb32) {
                twin_argb32_t *row =
                    (twin_argb32_t *) (shadow->p.b + y * shadow->stride);
                twin_argb32_t *dst = row + win_width;
                for (twin_coord_t i = 0; i < right_extent; i++)
                    dst[i] = (alpha_lut_x[i] << 24) | base_rgb;
            } else {
                twin_pointer_t dst = twin_pixmap_pointer(shadow, win_width, y);
                for (twin_coord_t i = 0; i < right_extent; i++)
                    dst.argb32[i] = (alpha_lut_x[i] << 24) | base_rgb;
            }
        }
    }

    /* Bottom edge strip (excluding the corner overlap). */
    if (bottom_extent > 0) {
        twin_coord_t bottom_start = win_height;
        if (bottom_start < 0)
            bottom_start = 0;
        twin_coord_t bottom_end = win_height + bottom_extent;
        if (bottom_end > shadow->height)
            bottom_end = shadow->height;

        twin_coord_t left_skip = x_offset;
        if (left_skip > win_width)
            left_skip = win_width;

        twin_coord_t bottom_width = win_width - left_skip - right_extent;
        if (bottom_width < 0)
            bottom_width = 0;

        twin_a8_t *alpha_bottom = NULL;
        bool alpha_bottom_ready = false;

        if (bottom_width > 0) {
            if (bottom_width > shadow_bottom_cache_capacity) {
                twin_a8_t *newbuf =
                    realloc(shadow_bottom_cache_weights, bottom_width);
                if (newbuf) {
                    shadow_bottom_cache_weights = newbuf;
                    shadow_bottom_cache_capacity = bottom_width;
                    shadow_bottom_cache_width = 0;
                }
            }
            if (shadow_bottom_cache_weights &&
                shadow_bottom_cache_capacity >= bottom_width) {
                alpha_bottom = shadow_bottom_cache_weights;
                if (shadow_bottom_cache_width == bottom_width)
                    alpha_bottom_ready = true;
            }
            if (!alpha_bottom)
                alpha_bottom = malloc(bottom_width);

            if (alpha_bottom && !alpha_bottom_ready) {
                twin_fixed_t bottom_step =
                    (bottom_width > 1) ? (TWIN_FIXED_ONE / (bottom_width - 1))
                                       : 0;
                twin_fixed_t u = 0;
                for (twin_coord_t x = 0; x < bottom_width; x++) {
                    twin_fixed_t weight = shadow_gaussian_weight(u);
                    alpha_bottom[x] = (twin_a8_t) ((weight * 255) >> 16);
                    if (bottom_width > 1 && u < TWIN_FIXED_ONE)
                        u += bottom_step;
                }
                if (alpha_bottom == shadow_bottom_cache_weights)
                    shadow_bottom_cache_width = bottom_width;
            }
        }

        for (twin_coord_t y = bottom_start; y < bottom_end; y++) {
            if (is_argb32) {
                twin_argb32_t *row =
                    (twin_argb32_t *) (shadow->p.b + y * shadow->stride);
                if (left_skip > 0)
                    memset(row, 0, (size_t) left_skip * sizeof(*row));

                if (bottom_width <= 0)
                    continue;

                twin_coord_t dist_y = y - win_height;
                twin_a8_t alpha_y = alpha_lut_y[dist_y];
                twin_argb32_t *dst = row + left_skip;

                if (alpha_bottom) {
                    for (twin_coord_t x = 0; x < bottom_width; x++) {
                        twin_a8_t alpha = (alpha_y * alpha_bottom[0]) >> 8;
                        dst[x] = (alpha << 24) | base_rgb;
                    }
                } else {
                    twin_fixed_t bottom_step =
                        (bottom_width > 1)
                            ? (TWIN_FIXED_ONE / (bottom_width - 1))
                            : 0;
                    twin_fixed_t u = 0;
                    for (twin_coord_t x = 0; x < bottom_width; x++) {
                        twin_fixed_t weight = shadow_gaussian_weight(u);
                        twin_a8_t alpha_x = (twin_a8_t) ((weight * 255) >> 16);
                        twin_a8_t alpha = (alpha_y * alpha_x) >> 8;
                        dst[x] = (alpha << 24) | base_rgb;
                        if (bottom_width > 1 && u < TWIN_FIXED_ONE)
                            u += bottom_step;
                    }
                }
            } else {
                twin_pointer_t clear = twin_pixmap_pointer(shadow, 0, y);
                for (twin_coord_t x = 0; x < left_skip; x++)
                    clear.argb32[x] = 0;

                if (bottom_width <= 0)
                    continue;

                twin_coord_t dist_y = y - win_height;
                twin_a8_t alpha_y = alpha_lut_y[dist_y];
                twin_pointer_t dst = twin_pixmap_pointer(shadow, left_skip, y);

                if (alpha_bottom) {
                    for (twin_coord_t x = 0; x < bottom_width; x++) {
                        twin_a8_t alpha = (alpha_y * alpha_bottom[0]) >> 8;
                        dst.argb32[x] = (alpha << 24) | base_rgb;
                    }
                } else {
                    twin_fixed_t bottom_step =
                        (bottom_width > 1)
                            ? (TWIN_FIXED_ONE / (bottom_width - 1))
                            : 0;
                    twin_fixed_t u = 0;
                    for (twin_coord_t x = 0; x < bottom_width; x++) {
                        twin_fixed_t weight = shadow_gaussian_weight(u);
                        twin_a8_t alpha_x = (twin_a8_t) ((weight * 255) >> 16);
                        twin_a8_t alpha = (alpha_y * alpha_x) >> 8;
                        dst.argb32[x] = (alpha << 24) | base_rgb;
                        if (bottom_width > 1 && u < TWIN_FIXED_ONE)
                            u += bottom_step;
                    }
                }
            }
        }

        if (alpha_bottom && alpha_bottom != shadow_bottom_cache_weights)
            free(alpha_bottom);
    }

    /* Bottom-right corner combines both falloffs. */
    if (right_extent > 0 && bottom_extent > 0) {
        twin_coord_t corner_end = win_height + bottom_extent;
        if (corner_end > shadow->height)
            corner_end = shadow->height;
        for (twin_coord_t y = win_height; y < corner_end; y++) {
            twin_coord_t dist_y = y - win_height;
            twin_a8_t alpha_y = alpha_lut_y[dist_y];
            if (is_argb32) {
                twin_argb32_t *row =
                    (twin_argb32_t *) (shadow->p.b + y * shadow->stride);
                twin_argb32_t *dst = row + win_width;
                for (twin_coord_t i = 0; i < right_extent; i++) {
                    twin_a8_t alpha = (alpha_lut_x[i] * alpha_y) >> 8;
                    dst[i] = (alpha << 24) | base_rgb;
                }
            } else {
                twin_pointer_t dst = twin_pixmap_pointer(shadow, win_width, y);
                for (twin_coord_t i = 0; i < right_extent; i++) {
                    twin_a8_t alpha = (alpha_lut_x[i] * alpha_y) >> 8;
                    dst.argb32[i] = (alpha << 24) | base_rgb;
                }
            }
        }
    }
}

#undef SHADOW_LUT_X_LEN
#undef SHADOW_LUT_Y_LEN
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
