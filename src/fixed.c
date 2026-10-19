/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

/* Backend tag referenced by every unit including twin.h; see twin_math.h. */
const char TWIN_MATH_ABI = 0;

/* Normalization puts the leading bit in the upper pair of square-root digits.
 * The near-one shortcut is retained for compatibility with existing paths.
 */
twin_fixed_t twin_fixed_sqrt(twin_fixed_t a)
{
    if (a <= 0)
        return 0;
#if TWIN_MATH_FLOAT
    return (twin_fixed_t) (twin_math_sqrt_positive((float) a) * 256.0f);
#else
    if ((uint32_t) a - (TWIN_FIXED_ONE - 128u) <= 256u)
        return TWIN_FIXED_ONE;

    int offset = (twin_clz((uint32_t) a) - 1) & ~1;
    /* Shift left 'a' to expand more digit for sqrt precision */
    a <<= offset;

    /* Calculate the digits need to shift back */
    offset >>= 1;
    offset -= (16 >> 1);

    /* Use digit-by-digit calculation to compute square root */
    twin_fixed_t z = 0;
    twin_fixed_t m = (a & 0x40000000) ? (1u << 30) : (1u << 28);
    for (; m; m >>= 2) {
        int b = z + m;
        z >>= 1;
        if (a >= b)
            a -= b, z += m;
    }

    /* Shift back the expanded digits */
    return (offset >= 0) ? z >> offset : z << (-offset);
#endif
}

twin_xfixed_t _twin_xfixed_sqrt(twin_xfixed_t a)
{
    /* Early return for non-positive inputs */
    if (a <= 0)
        return 0;

    /* Special case for values very close to 1 */
    if ((uint64_t) a - (TWIN_XFIXED_ONE - 32768u) <= 65536u)
        return TWIN_XFIXED_ONE;

    int offset = (twin_clzll((uint64_t) a) - 1) & ~1;

    /* Shift left to expand precision */
    a <<= offset;

    /* Calculate scaling offset */
    offset >>= 1;
    offset -= (32 >> 1);

#if TWIN_MATH_A64_FP
    /* Binary64 gives a seed within one integer of the exact normalized root.
     * Correct with integer squares to preserve every legacy Q31.32 result,
     * even when converting a near-perfect 63-bit square rounded its input.
     */
    uint64_t z = (uint64_t) twin_math_sqrt_positive64((double) a);
    if (z * z > (uint64_t) a)
        --z;
    else if ((z + 1) * (z + 1) <= (uint64_t) a)
        ++z;
#else
    /* Digit-by-digit square root calculation */
    twin_xfixed_t z = 0;
    twin_xfixed_t m =
        (a & (INT64_C(1) << 62)) ? (INT64_C(1) << 62) : (INT64_C(1) << 60);
    for (; m; m >>= 2) {
        twin_xfixed_t b = z + m;
        z >>= 1;
        if (a >= b)
            a -= b, z += m;
    }

#endif
    /* Shift back the expanded digits */
    return (offset >= 0) ? z >> offset : z << (-offset);
}
