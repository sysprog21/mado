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
    uint16_t t1, t2, t3, t4;
    twin_a8_t a;

    if (!src)
        return dst;
    a = ~(src >> 24);
    switch (a) {
    case 0:
        return src;
    case 0xff:
        dst = (twin_add(src, dst, 0, t1) | twin_add(src, dst, 8, t2) |
               twin_add(src, dst, 16, t3) | twin_add(src, dst, 24, t4));
        break;
    default:
        dst = (twin_over(src, dst, 0, a, t1) | twin_over(src, dst, 8, a, t2) |
               twin_over(src, dst, 16, a, t3) | twin_over(src, dst, 24, a, t4));
        break;
    }
    return dst;
}

/*
 * Convert RGB16 to ARGB32 and copy to destination
 * Used for compositing RGB16 pixmaps onto ARGB32 screen buffer
 */
void _twin_rgb16_source_argb32(twin_pointer_t dst, twin_source_u src, int width)
{
    twin_argb32_t src32;
    while (width--) {
        src32 = twin_rgb16_to_argb32(*src.p.rgb16++);
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
