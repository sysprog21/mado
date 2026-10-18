/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 *
 * Essential compositing operations for screen rendering.
 * These functions are always needed regardless of renderer choice.
 */

#include "twin_private.h"

/*
 * Porter-Duff "over" operator for ARGB32 pixels
 * dst = src + dst * (1 - alpha_src)
 */
static inline twin_argb32_t over(twin_argb32_t dst, twin_argb32_t src)
{
    uint32_t a = 0xff - (src >> 24);

    if (a == 0)
        return src;
    if (!src)
        return dst;
    return _twin_sat_lanes(_twin_mul_lanes(dst & TWIN_LANES, a) +
                           (src & TWIN_LANES)) |
           _twin_sat_lanes(_twin_mul_lanes((dst >> 8) & TWIN_LANES, a) +
                           ((src >> 8) & TWIN_LANES))
               << 8;
}

/*
 * The built-in renderer generates these three span operations along with the
 * rest of its compositing table; other renderers (Pixman) get them here.
 */
#if !defined(CONFIG_RENDERER_BUILTIN)
/*
 * Convert RGB16 to ARGB32 and copy to destination
 * Used for compositing RGB16 pixmaps onto ARGB32 screen buffer
 */
void _twin_rgb16_source_argb32(twin_pointer_t dst, twin_source_u src, int width)
{
    twin_argb32_t src32;
    while (width--) {
        /* twin_rgb16_to_argb32() evaluates its argument several times */
        twin_rgb16_t v = *src.p.rgb16++;
        src32 = twin_rgb16_to_argb32(v);
        *dst.argb32++ = src32;
    }
}

/*
 * Porter-Duff "over" operator for ARGB32 to ARGB32
 * Used for compositing ARGB32 pixmaps onto ARGB32 screen buffer
 */
void _twin_argb32_over_argb32(twin_pointer_t dst, twin_source_u src, int width)
{
    twin_argb32_t dst32;
    twin_argb32_t src32;
    while (width--) {
        dst32 = *dst.argb32;
        src32 = *src.p.argb32++;
        dst32 = over(dst32, src32);
        *dst.argb32++ = dst32;
    }
}

/*
 * Copy ARGB32 source to ARGB32 destination
 * Used for direct copy operations (e.g., background tiling)
 */
void _twin_argb32_source_argb32(twin_pointer_t dst,
                                twin_source_u src,
                                int width)
{
    twin_argb32_t src32;
    while (width--) {
        src32 = *src.p.argb32++;
        *dst.argb32++ = src32;
    }
}
#endif /* !CONFIG_RENDERER_BUILTIN */

/* src scaled by a constant alpha */
static inline twin_argb32_t in_alpha(twin_argb32_t src, twin_a8_t alpha)
{
    return _twin_mul_lanes(src & TWIN_LANES, alpha) |
           _twin_mul_lanes((src >> 8) & TWIN_LANES, alpha) << 8;
}

void _twin_screen_blend_argb32(twin_argb32_t *dst,
                               const twin_argb32_t *src,
                               twin_a8_t alpha,
                               int width)
{
    while (width--) {
        *dst = over(*dst, in_alpha(*src++, alpha));
        dst++;
    }
}

void _twin_screen_blend_rgb16(twin_argb32_t *dst,
                              const twin_rgb16_t *src,
                              twin_a8_t alpha,
                              int width)
{
    while (width--) {
        twin_rgb16_t v = *src++;
        twin_argb32_t s = twin_rgb16_to_argb32(v);
        *dst = over(*dst, in_alpha(s, alpha));
        dst++;
    }
}

void _twin_screen_fill(twin_argb32_t *dst, twin_argb32_t color, int width)
{
    while (width--)
        *dst++ = color;
}
