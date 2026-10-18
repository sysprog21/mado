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

/* over(under, in_alpha(src, alpha)) for opaque src: the source's alpha
 * after scaling is exactly alpha, so each channel is
 * mul(c, alpha) + mul(under_c, 255 - alpha).  Both terms are bounded by
 * their factors, so the sum never exceeds 255 and the under term packs
 * into one constant add.  Alpha holds for a whole frame of a transition,
 * so the products come from a table built once per alpha. */
static struct {
    bool valid;
    twin_a8_t alpha;
    uint8_t mul[256];
} _onto_table;

static const uint8_t *_onto_mul(twin_a8_t alpha)
{
    if (!_onto_table.valid || _onto_table.alpha != alpha) {
        for (uint32_t c = 0; c < 256; c++)
            _onto_table.mul[c] = (uint8_t) _twin_mul_lanes(c, alpha);
        _onto_table.alpha = alpha;
        _onto_table.valid = true;
    }
    return _onto_table.mul;
}

static inline uint32_t _onto_under(twin_a8_t alpha, twin_argb32_t under)
{
    uint32_t k = 0xff - alpha;
    return _twin_mul_lanes(under & TWIN_LANES, k) |
           _twin_mul_lanes((under >> 8) & TWIN_LANES, k) << 8;
}

static inline twin_argb32_t _onto(twin_argb32_t s,
                                  const uint8_t *mul,
                                  uint32_t under)
{
    return ((uint32_t) mul[s >> 24] << 24 |
            (uint32_t) mul[(s >> 16) & 0xff] << 16 |
            (uint32_t) mul[(s >> 8) & 0xff] << 8 | mul[s & 0xff]) +
           under;
}

void _twin_screen_blend_onto_argb32(twin_argb32_t *dst,
                                    const twin_argb32_t *src,
                                    twin_a8_t alpha,
                                    twin_argb32_t under,
                                    int width)
{
    const uint8_t *mul = _onto_mul(alpha);
    uint32_t u = _onto_under(alpha, under);
    while (width--)
        *dst++ = _onto(*src++, mul, u);
}

void _twin_screen_blend_onto_rgb16(twin_argb32_t *dst,
                                   const twin_rgb16_t *src,
                                   twin_a8_t alpha,
                                   twin_argb32_t under,
                                   int width)
{
    const uint8_t *mul = _onto_mul(alpha);
    uint32_t u = _onto_under(alpha, under);
    while (width--) {
        twin_rgb16_t v = *src++;
        *dst++ = _onto(twin_rgb16_to_argb32(v), mul, u);
    }
}
