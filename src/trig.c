/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include "twin_private.h"

#if TWIN_MATH_FLOAT
#include <math.h>

/* Odd degree-7 minimax on t = x / 2^10 in [0,1]. Coefficients include pi/2
 * and Q16 amplitude; the input scale is an exact power of two. Exhaustive
 * angle tests bound the final quantized error below two Q16 LSBs, with or
 * without FMA contraction. Shared by the scalar and NEON kernels.
 */
#define SIN_C7 (-0x1.1bf946p+8f)
#define SIN_C5 (0x1.455cf2p+12f)
#define SIN_C3 (-0x1.4ab278p+15f)
#define SIN_C1 (0x1.921f5cp+16f)

static inline float sin_poly(twin_angle_t x)
{
    if (x == TWIN_ANGLE_90)
        return TWIN_FIXED_ONE;
    float t = (float) x * 0x1p-10f;
    float z = t * t;
    float p = SIN_C7;
    p = p * z + SIN_C5;
    p = p * z + SIN_C3;
    p = p * z + SIN_C1;
    return t * p;
}
#elif defined(CONFIG_MATH_TRIG_TABLE) && CONFIG_MATH_TRIG_TABLE
#include "math-trig.h"
static inline twin_fixed_t sin_poly(twin_angle_t x)
{
    return twin_math_sin_quarter[x];
}
#else
static inline twin_fixed_t sin_poly(twin_angle_t x)
{
    /* Endpoint-constrained odd degree-5 minimax approximation on [0,pi/2].
     * Coefficients are fitted for the quantized evaluation below; exhaustive
     * tests bound the error below six Q16 LSBs, with exact 0/90-degree values.
     * Retain the same multiplies, shifts and rounding as the original kernel.
     */
    const uint32_t A = 16, n = 10, p = 32, q = 31, r = 3;
    const uint32_t A1 = 3372089009, B1 = 2756325272, C1 = 2399578;
    /* x is in [0,1024]. Every product is bounded by max(A1, B1,
     * C1 * 1024), all below UINT32_MAX; no 64-bit multiply is needed.
     */
    uint32_t y = (C1 * x) >> n;
    y = B1 - ((x * y) >> r);
    y = x * (y >> n);
    y = x * (y >> n);
    y = A1 - (y >> (p - q));
    y = x * (y >> n);
    y = (y + (1u << (q - A - 1))) >> (q - A); /* Rounding */
    return y;
}

#endif

#if TWIN_MATH_A64_NEON
#include <arm_neon.h>

static inline void sincos_pair(twin_angle_t a, twin_fixed_t *s, twin_fixed_t *c)
{
    unsigned angle = (unsigned) a & (TWIN_ANGLE_360 - 1);
    if (!(angle & (TWIN_ANGLE_90 - 1))) {
        static const int values[4] = {0, 1, 0, -1};
        unsigned quadrant = angle / TWIN_ANGLE_90;
        *s = values[quadrant] * TWIN_FIXED_ONE;
        *c = values[(quadrant + 1) & 3] * TWIN_FIXED_ONE;
        return;
    }
    unsigned x = angle & (TWIN_ANGLE_90 - 1);
    if (angle & TWIN_ANGLE_90)
        x = TWIN_ANGLE_90 - x;
    int32x2_t phases = {(int32_t) x, (int32_t) (TWIN_ANGLE_90 - x)};
    float32x2_t t = vcvt_n_f32_s32(phases, 10);
    float32x2_t z = vmul_f32(t, t);
    float32x2_t p = vdup_n_f32(SIN_C7);
    p = p * z + vdup_n_f32(SIN_C5);
    p = p * z + vdup_n_f32(SIN_C3);
    p = p * z + vdup_n_f32(SIN_C1);
    p = vmul_f32(t, p);
    uint32x2_t signs = {angle & TWIN_ANGLE_180 ? UINT32_C(0x80000000) : 0,
                        angle > TWIN_ANGLE_90 && angle < TWIN_ANGLE_270
                            ? UINT32_C(0x80000000)
                            : 0};
    p = vreinterpret_f32_u32(veor_u32(vreinterpret_u32_f32(p), signs));
    int32x2_t result = vcvt_s32_f32(p);
    *s = vget_lane_s32(result, 0);
    *c = vget_lane_s32(result, 1);
}
#endif

void twin_sincos(twin_angle_t a, twin_fixed_t *sin, twin_fixed_t *cos)
{
#if TWIN_MATH_A64_NEON
    if (sin && cos) {
        sincos_pair(a, sin, cos);
        return;
    }
#endif
#if TWIN_MATH_FLOAT
    /* Apply signs before conversion so A64 can keep results in FP registers
     * until the final integer stores, avoiding a GP/FP round trip.
     */
    float sin_val = 0, cos_val = 0;
#else
    twin_fixed_t sin_val = 0, cos_val = 0;
#endif

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
#if TWIN_MATH_A64_FP
        *sin = twin_math_from_scaled(sin_val);
#else
        *sin = sin_val;
#endif
    }
    if (cos) {
        /* mirror first and fourth quadrant values across y axis */
        if (c)
            cos_val = -cos_val;
#if TWIN_MATH_A64_FP
        *cos = twin_math_from_scaled(cos_val);
#else
        *cos = cos_val;
#endif
    }
}

#if TWIN_MATH_FLOAT
twin_angle_t twin_acos(twin_fixed_t x)
{
    if (x <= -TWIN_FIXED_ONE)
        return TWIN_ANGLE_180;
    if (x >= TWIN_FIXED_ONE)
        return 0;
    return (twin_angle_t) (acosf((float) x * 0x1p-16f) *
                           (TWIN_ANGLE_360 / 6.2831853071795864769f));
}
#else
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

    /* Accumulation fits in int; avoid narrowing/sign extension each step
     * on RV32 (twin_angle_t is only 16 bits).
     */
    int angle = 0;

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

#endif
