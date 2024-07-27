/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twin_private.h"

#define TWIN_LOG2_SIN 10

/*
 * construction:
  int s = 10; for (int i = 0; i < (1 << s); i++) {
    if (i % 8 == 0) printf ("\n   ");
    printf (" 0x%04x,", floor (sin(pi/2 * i / (1 << s)) * 2**16 + 0.5));
  }
 */

static const uint16_t _twin_sin_table[1 << (TWIN_LOG2_SIN-1)] = {
    0x0000, 0x0065, 0x00c9, 0x012e, 0x0192, 0x01f7, 0x025b, 0x02c0, 0x0324,
    0x0389, 0x03ed, 0x0452, 0x04b6, 0x051b, 0x057f, 0x05e4, 0x0648, 0x06ad,
    0x0711, 0x0776, 0x07da, 0x083f, 0x08a3, 0x0908, 0x096c, 0x09d1, 0x0a35,
    0x0a9a, 0x0afe, 0x0b62, 0x0bc7, 0x0c2b, 0x0c90, 0x0cf4, 0x0d59, 0x0dbd,
    0x0e21, 0x0e86, 0x0eea, 0x0f4e, 0x0fb3, 0x1017, 0x107b, 0x10e0, 0x1144,
    0x11a8, 0x120d, 0x1271, 0x12d5, 0x1339, 0x139e, 0x1402, 0x1466, 0x14ca,
    0x152e, 0x1593, 0x15f7, 0x165b, 0x16bf, 0x1723, 0x1787, 0x17eb, 0x1850,
    0x18b4, 0x1918, 0x197c, 0x19e0, 0x1a44, 0x1aa8, 0x1b0c, 0x1b70, 0x1bd4,
    0x1c38, 0x1c9b, 0x1cff, 0x1d63, 0x1dc7, 0x1e2b, 0x1e8f, 0x1ef3, 0x1f56,
    0x1fba, 0x201e, 0x2082, 0x20e5, 0x2149, 0x21ad, 0x2210, 0x2274, 0x22d7,
    0x233b, 0x239f, 0x2402, 0x2466, 0x24c9, 0x252d, 0x2590, 0x25f4, 0x2657,
    0x26ba, 0x271e, 0x2781, 0x27e4, 0x2848, 0x28ab, 0x290e, 0x2971, 0x29d5,
    0x2a38, 0x2a9b, 0x2afe, 0x2b61, 0x2bc4, 0x2c27, 0x2c8a, 0x2ced, 0x2d50,
    0x2db3, 0x2e16, 0x2e79, 0x2edc, 0x2f3f, 0x2fa1, 0x3004, 0x3067, 0x30ca,
    0x312c, 0x318f, 0x31f1, 0x3254, 0x32b7, 0x3319, 0x337c, 0x33de, 0x3440,
    0x34a3, 0x3505, 0x3568, 0x35ca, 0x362c, 0x368e, 0x36f1, 0x3753, 0x37b5,
    0x3817, 0x3879, 0x38db, 0x393d, 0x399f, 0x3a01, 0x3a63, 0x3ac5, 0x3b27,
    0x3b88, 0x3bea, 0x3c4c, 0x3cae, 0x3d0f, 0x3d71, 0x3dd2, 0x3e34, 0x3e95,
    0x3ef7, 0x3f58, 0x3fba, 0x401b, 0x407c, 0x40de, 0x413f, 0x41a0, 0x4201,
    0x4262, 0x42c3, 0x4324, 0x4385, 0x43e6, 0x4447, 0x44a8, 0x4509, 0x456a,
    0x45cb, 0x462b, 0x468c, 0x46ec, 0x474d, 0x47ae, 0x480e, 0x486f, 0x48cf,
    0x492f, 0x4990, 0x49f0, 0x4a50, 0x4ab0, 0x4b10, 0x4b71, 0x4bd1, 0x4c31,
    0x4c90, 0x4cf0, 0x4d50, 0x4db0, 0x4e10, 0x4e70, 0x4ecf, 0x4f2f, 0x4f8e,
    0x4fee, 0x504d, 0x50ad, 0x510c, 0x516c, 0x51cb, 0x522a, 0x5289, 0x52e8,
    0x5348, 0x53a7, 0x5406, 0x5464, 0x54c3, 0x5522, 0x5581, 0x55e0, 0x563e,
    0x569d, 0x56fc, 0x575a, 0x57b9, 0x5817, 0x5875, 0x58d4, 0x5932, 0x5990,
    0x59ee, 0x5a4c, 0x5aaa, 0x5b08, 0x5b66, 0x5bc4, 0x5c22, 0x5c80, 0x5cde,
    0x5d3b, 0x5d99, 0x5df6, 0x5e54, 0x5eb1, 0x5f0f, 0x5f6c, 0x5fc9, 0x6026,
    0x6084, 0x60e1, 0x613e, 0x619b, 0x61f8, 0x6254, 0x62b1, 0x630e, 0x636b,
    0x63c7, 0x6424, 0x6480, 0x64dd, 0x6539, 0x6595, 0x65f2, 0x664e, 0x66aa,
    0x6706, 0x6762, 0x67be, 0x681a, 0x6876, 0x68d1, 0x692d, 0x6989, 0x69e4,
    0x6a40, 0x6a9b, 0x6af6, 0x6b52, 0x6bad, 0x6c08, 0x6c63, 0x6cbe, 0x6d19,
    0x6d74, 0x6dcf, 0x6e2a, 0x6e85, 0x6edf, 0x6f3a, 0x6f94, 0x6fef, 0x7049,
    0x70a3, 0x70fe, 0x7158, 0x71b2, 0x720c, 0x7266, 0x72c0, 0x731a, 0x7373,
    0x73cd, 0x7427, 0x7480, 0x74da, 0x7533, 0x758d, 0x75e6, 0x763f, 0x7698,
    0x76f1, 0x774a, 0x77a3, 0x77fc, 0x7855, 0x78ad, 0x7906, 0x795f, 0x79b7,
    0x7a10, 0x7a68, 0x7ac0, 0x7b18, 0x7b70, 0x7bc8, 0x7c20, 0x7c78, 0x7cd0,
    0x7d28, 0x7d7f, 0x7dd7, 0x7e2f, 0x7e86, 0x7edd, 0x7f35, 0x7f8c, 0x7fe3,
    0x803a, 0x8091, 0x80e8, 0x813f, 0x8195, 0x81ec, 0x8243, 0x8299, 0x82f0,
    0x8346, 0x839c, 0x83f2, 0x8449, 0x849f, 0x84f5, 0x854a, 0x85a0, 0x85f6,
    0x864c, 0x86a1, 0x86f7, 0x874c, 0x87a1, 0x87f6, 0x884c, 0x88a1, 0x88f6,
    0x894a, 0x899f, 0x89f4, 0x8a49, 0x8a9d, 0x8af2, 0x8b46, 0x8b9a, 0x8bef,
    0x8c43, 0x8c97, 0x8ceb, 0x8d3f, 0x8d93, 0x8de6, 0x8e3a, 0x8e8d, 0x8ee1,
    0x8f34, 0x8f88, 0x8fdb, 0x902e, 0x9081, 0x90d4, 0x9127, 0x9179, 0x91cc,
    0x921f, 0x9271, 0x92c4, 0x9316, 0x9368, 0x93ba, 0x940c, 0x945e, 0x94b0,
    0x9502, 0x9554, 0x95a5, 0x95f7, 0x9648, 0x969a, 0x96eb, 0x973c, 0x978d,
    0x97de, 0x982f, 0x9880, 0x98d0, 0x9921, 0x9972, 0x99c2, 0x9a12, 0x9a63,
    0x9ab3, 0x9b03, 0x9b53, 0x9ba3, 0x9bf2, 0x9c42, 0x9c92, 0x9ce1, 0x9d31,
    0x9d80, 0x9dcf, 0x9e1e, 0x9e6d, 0x9ebc, 0x9f0b, 0x9f5a, 0x9fa8, 0x9ff7,
    0xa045, 0xa094, 0xa0e2, 0xa130, 0xa17e, 0xa1cc, 0xa21a, 0xa268, 0xa2b5,
    0xa303, 0xa350, 0xa39e, 0xa3eb, 0xa438, 0xa485, 0xa4d2, 0xa51f, 0xa56c,
    0xa5b8, 0xa605, 0xa652, 0xa69e, 0xa6ea, 0xa736, 0xa782, 0xa7ce, 0xa81a,
    0xa866, 0xa8b2, 0xa8fd, 0xa949, 0xa994, 0xa9df, 0xaa2a, 0xaa76, 0xaac1,
    0xab0b, 0xab56, 0xaba1, 0xabeb, 0xac36, 0xac80, 0xacca, 0xad14, 0xad5e,
    0xada8, 0xadf2, 0xae3c, 0xae85, 0xaecf, 0xaf18, 0xaf62, 0xafab, 0xaff4,
    0xb03d, 0xb086, 0xb0ce, 0xb117, 0xb160, 0xb1a8, 0xb1f0, 0xb239, 0xb281,
    0xb2c9, 0xb311, 0xb358, 0xb3a0, 0xb3e8, 0xb42f, 0xb477, 0xb4be
};

/*
 * angles are measured from -2048 .. 2048
 */


twin_fixed_t twin_sin(twin_angle_t a)
{
    twin_fixed_t sin;

    /* limit to [0..360) */
    a = a & (TWIN_ANGLE_360 - 1);
    /* special case for 90 degrees - no room in table */
    if ((a & ~(TWIN_ANGLE_180)) == TWIN_ANGLE_90)
        sin = TWIN_FIXED_ONE;
    else {
        /* mirror second and third quadrant values across y axis */
        if (a & TWIN_ANGLE_90)
            a = TWIN_ANGLE_180 - a;
        if (a & TWIN_ANGLE_45){
            /* sin(a) = cos(90-a) = sqrt(1 - sin(90-a)^2) */
            a = TWIN_ANGLE_90 - a;
            sin = _twin_sin_table[a & (TWIN_ANGLE_45 - 1)];
            sin = twin_fixed_sqrt(TWIN_FIXED_ONE - twin_fixed_mul(sin, sin));
        }else{
            sin = _twin_sin_table[a & (TWIN_ANGLE_90 - 1)];
        }
    }
    /* mirror third and fourth quadrant values across x axis */
    if (a & TWIN_ANGLE_180)
        sin = -sin;
    return sin;
}

twin_fixed_t twin_cos(twin_angle_t a)
{
    return twin_sin(a + TWIN_ANGLE_90);
}

twin_fixed_t twin_tan(twin_angle_t a)
{
    twin_fixed_t s = twin_sin(a);
    twin_fixed_t c = twin_cos(a);

    if (c == 0) {
        if (s > 0)
            return TWIN_FIXED_MAX;
        else
            return TWIN_FIXED_MIN;
    }
    if (s == 0)
        return 0;
    return ((s << 15) / c) << 1;
}
