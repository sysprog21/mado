/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twin_private.h"

#define uint32_lo(i) ((i) & 0xffff)
#define uint32_hi(i) ((i) >> 16)
#define uint32_carry16 ((1) << 16)
/* Check interval
 * For any variable interval checking:
 *     if (x > minx - epsilon && x < minx + epsilon) ...
 * it is faster to use bit operation:
 *     if ((signed)((x - (minx - epsilon)) | ((minx + epsilon) - x)) > 0) ...
 */
#define CHECK_INTERVAL(x, minx, epsilon) \
    ((int32_t) ((x - (minx - epsilon)) | (minx + epsilon - x)) > 0)

twin_fixed_t twin_fixed_sqrt(twin_fixed_t a)
{
    if (a <= 0)
        return 0;
    if (CHECK_INTERVAL(a, TWIN_FIXED_ONE, (1 << (8 - 1))))
        return TWIN_FIXED_ONE;

    /* Count leading zero */
    int offset = 0;
    for (twin_fixed_t i = a; !(0x40000000 & i); i <<= 1)
        ++offset;

    /* Shift left 'a' to expand more digit for sqrt precision */
    offset &= ~1;
    a <<= offset;
    /* Calculate the digits need to shift back */
    offset >>= 1;
    offset -= (16 >> 1);
    /* Use digit-by-digit calculation to compute square root */
    twin_fixed_t z = 0;
    for (twin_fixed_t m = 1UL << ((31 - __builtin_clz(a)) & ~1UL); m; m >>= 2) {
        int b = z + m;
        z >>= 1;
        if (a >= b)
            a -= b, z += m;
    }
    /* Shift back the expanded digits */
    return (offset >= 0) ? z >> offset : z << (-offset);
}

twin_sfixed_t _twin_sfixed_sqrt(twin_sfixed_t as)
{
    if (as <= 0)
        return 0;
    if (CHECK_INTERVAL(as, TWIN_SFIXED_ONE, (1 << (2 - 1))))
        return TWIN_SFIXED_ONE;

    int offset = 0;
    for (twin_sfixed_t i = as; !(0x4000 & i); i <<= 1)
        ++offset;

    offset &= ~1;
    as <<= offset;

    offset >>= 1;
    offset -= (4 >> 1);

    twin_sfixed_t z = 0;
    for (twin_sfixed_t m = 1UL << ((31 - __builtin_clz(as)) & ~1UL); m;
         m >>= 2) {
        int16_t b = z + m;
        z >>= 1;
        if (as >= b)
            as -= b, z += m;
    }

    return (offset >= 0) ? z >> offset : z << (-offset);
}
