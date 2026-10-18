/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

twin_fixed_t twin_cos(twin_angle_t a)
{
    twin_fixed_t cos_val = 0;
    twin_sincos(a, NULL, &cos_val);
    return cos_val;
}

static inline twin_fixed_t sin_poly(twin_angle_t x)
{
    /* S(x) = x * 2^(-n) * (A1 - 2 ^ (q-p) * x * (2^-n) * x * 2^(-n) * (B1 - 2 ^
     * (-r) * x * 2 ^ (-n) * C1 * x)) * 2 ^ (a-q)
     * @n: the angle scale
     * @A: the amplitude
     * @p,q,r: the scaling factor
     *
     * A1 = 2^q * a5, B1 = 2 ^ p * b5, C1 = 2 ^ (r+p-n) * c5
     * where a5, b5, c5 are the coefficients for 5th-order polynomial
     * a5 = 4 * (3 / pi - 9 / 16)
     * b5 = 2 * a5 - 5 / 2
     * c5 = a5 - 3 / 2
     */
    const uint64_t A = 16, n = 10, p = 32, q = 31, r = 3;
    const uint64_t A1 = 3370945099, B1 = 2746362156, C1 = 2339369;
    uint64_t y = (C1 * x) >> n;
    y = B1 - ((x * y) >> r);
    y = x * (y >> n);
    y = x * (y >> n);
    y = A1 - (y >> (p - q));
    y = x * (y >> n);
    y = (y + (1UL << (q - A - 1))) >> (q - A); /* Rounding */
    return y;
}

void twin_sincos(twin_angle_t a, twin_fixed_t *sin, twin_fixed_t *cos)
{
    twin_fixed_t sin_val = 0, cos_val = 0;

    /* limit to [0..360) */
    a = a & (TWIN_ANGLE_360 - 1);
    int c = a > TWIN_ANGLE_90 && a < TWIN_ANGLE_270;
    /* special case for 90 and 270 degrees */
    if ((a & ~(TWIN_ANGLE_180)) == TWIN_ANGLE_90) {
        sin_val = TWIN_FIXED_ONE;
        cos_val = 0;
    } else {
        /* mirror second and third quadrant values across y axis */
        if (a & TWIN_ANGLE_90)
            a = TWIN_ANGLE_180 - a;
        twin_angle_t x = a & (TWIN_ANGLE_90 - 1);
        if (sin)
            sin_val = sin_poly(x);
        if (cos)
            cos_val = sin_poly(TWIN_ANGLE_90 - x);
    }
    if (sin) {
        /* mirror third and fourth quadrant values across x axis */
        if (a & TWIN_ANGLE_180)
            sin_val = -sin_val;
        *sin = sin_val;
    }
    if (cos) {
        /* mirror first and fourth quadrant values across y axis */
        if (c)
            cos_val = -cos_val;
        *cos = cos_val;
    }
}

static const twin_angle_t atan_table[] = {
    0x1000, /* arctan(2^0)  = 45°    -> 4096  */
    0x0972, /* arctan(2^-1) = 26.565° -> 2418 */
    0x04fe, /* arctan(2^-2) = 14.036° -> 1278 */
    0x0289, /* arctan(2^-3) = 7.125°  -> 649  */
    0x0146, /* arctan(2^-4) = 3.576°  -> 326  */
    0x00a3, /* arctan(2^-5) = 1.790°  -> 163  */
    0x0051, /* arctan(2^-6) = 0.895°  -> 81   */
    0x0029, /* arctan(2^-7) = 0.448°  -> 41   */
    0x0014, /* arctan(2^-8) = 0.224°  -> 20   */
    0x000a, /* arctan(2^-9) = 0.112°  -> 10   */
    0x0005, /* arctan(2^-10) = 0.056° -> 5    */
    0x0003, /* arctan(2^-11) = 0.028° -> 3    */
    0x0001, /* arctan(2^-12) = 0.014° -> 1    */
    0x0001, /* arctan(2^-13) = 0.007° -> 1    */
    0x0000, /* arctan(2^-14) = 0.0035° -> 0   */
};

static twin_angle_t twin_atan2_first_quadrant(twin_fixed_t y, twin_fixed_t x)
{
    if (x == 0 && y == 0)
        return TWIN_ANGLE_0;
    if (x == 0)
        return TWIN_ANGLE_90;
    if (y == 0)
        return TWIN_ANGLE_0;

    twin_angle_t angle = 0;

    /* CORDIC iteration */
    /*
     * To enhance accuracy, the angle is mapped from the range 0-360 degrees to
     * 0-32768. Allows for finer resolution to additional CORDIC iterations for
     * more precise calculations.
     */
    for (int i = 0; i < 15; i++) {
        twin_fixed_t temp_x = x;
        if (y > 0) {
            x += (y >> i);
            y -= (temp_x >> i);
            angle += atan_table[i];
        } else {
            x -= (y >> i);
            y += (temp_x >> i);
            angle -= atan_table[i];
        }
    }

    /* Fixed-point conversion: angle / 32768 * TWIN_ANGLE_360
     * Simplified: angle * 2^12 / 2^15 = angle / 8
     * Use integer division (not right shift) to match original
     * truncate-towards-zero behavior for negative values. Right shift would
     * round towards negative infinity, causing precision errors for
     * non-multiples of 8. Avoids overflow since no intermediate multiplication
     * required.
     */
    return (twin_angle_t) (angle / 8);
}

twin_angle_t twin_acos(twin_fixed_t x)
{
    if (x <= -TWIN_FIXED_ONE)
        return TWIN_ANGLE_180;
    if (x >= TWIN_FIXED_ONE)
        return TWIN_ANGLE_0;

    twin_fixed_t y = twin_fixed_sqrt(TWIN_FIXED_ONE - twin_fixed_mul(x, x));
    if (x < 0)
        return TWIN_ANGLE_180 - twin_atan2_first_quadrant(y, -x);
    return twin_atan2_first_quadrant(y, x);
}
