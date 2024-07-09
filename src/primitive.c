/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

static inline twin_argb32_t in_over(twin_argb32_t dst,
                                    twin_argb32_t src,
                                    twin_a8_t msk)
{
    uint16_t t1, t2, t3, t4;
    twin_a8_t a;

    switch (msk) {
    case 0:
        return dst;
    case 0xff:
        break;
    default:
        src = (twin_in(src, 0, msk, t1) | twin_in(src, 8, msk, t2) |
               twin_in(src, 16, msk, t3) | twin_in(src, 24, msk, t4));
        break;
    }
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

static inline twin_argb32_t in(twin_argb32_t src, twin_a8_t msk)
{
    uint16_t t1, t2, t3, t4;

    return (twin_in(src, 0, msk, t1) | twin_in(src, 8, msk, t2) |
            twin_in(src, 16, msk, t3) | twin_in(src, 24, msk, t4));
}

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

static inline twin_argb32_t rgb16_to_argb32(twin_rgb16_t v)
{
    return twin_rgb16_to_argb32(v);
}

static inline twin_argb32_t a8_to_argb32(twin_a8_t v)
{
    return v << 24;
}

static inline twin_rgb16_t argb32_to_rgb16(twin_argb32_t v)
{
    return twin_argb32_to_rgb16(v);
}

static inline twin_a8_t argb32_to_a8(twin_argb32_t v)
{
    return v >> 24;
}

/*
 * Naming convention
 *
 *  _twin_<src>_in_<msk>_op_<dst>
 *
 * Use 'c' for constant
 */

#define dst_argb32_get (*dst.argb32)
#define dst_argb32_set (*dst.argb32++) =
#define dst_rgb16_get (rgb16_to_argb32(*dst.rgb16))
#define dst_rgb16_set (*dst.rgb16++) = argb32_to_rgb16
#define dst_a8_get (a8_to_argb32(*dst.a8))
#define dst_a8_set (*dst.a8++) = argb32_to_a8

#define src_c (src.c)
#define src_argb32 (*src.p.argb32++)
#define src_rgb16 (rgb16_to_argb32(*src.p.rgb16++))
#define src_a8 (a8_to_argb32(*src.p.a8++))

#define msk_c (argb32_to_a8(msk.c))
#define msk_argb32 (argb32_to_a8(*msk.p.argb32++))
#define msk_rgb16 \
    (0xff);       \
    (void) msk
#define msk_a8 (*msk.p.a8++)

#define CAT2(a, b) a##b
#define CAT3(a, b, c) a##b##c
#define CAT4(a, b, c, d) a##b##c##d
#define CAT6(a, b, c, d, e, f) a##b##c##d##e##f

#define _twin_in_op_name(src, op, msk, dst) \
    CAT6(_twin_, src, _in_, msk, op, dst)

#define _twin_op_name(src, op, dst) CAT4(_twin_, src, op, dst)

#define MAKE_TWIN_in_over(__dst, __src, __msk)                               \
    void _twin_in_op_name(__src, _over_, __msk, __dst)(                      \
        twin_pointer_t dst, twin_source_u src, twin_source_u msk, int width) \
    {                                                                        \
        twin_argb32_t dst32;                                                 \
        twin_argb32_t src32;                                                 \
        twin_a8_t msk8;                                                      \
        while (width--) {                                                    \
            dst32 = CAT3(dst_, __dst, _get);                                 \
            src32 = CAT2(src_, __src);                                       \
            msk8 = CAT2(msk_, __msk);                                        \
            dst32 = in_over(dst32, src32, msk8);                             \
            CAT3(dst_, __dst, _set)(dst32);                                  \
        }                                                                    \
    }

#define MAKE_TWIN_in_source(__dst, __src, __msk)                             \
    void _twin_in_op_name(__src, _source_, __msk, __dst)(                    \
        twin_pointer_t dst, twin_source_u src, twin_source_u msk, int width) \
    {                                                                        \
        twin_argb32_t dst32;                                                 \
        twin_argb32_t src32;                                                 \
        twin_a8_t msk8;                                                      \
        while (width--) {                                                    \
            src32 = CAT2(src_, __src);                                       \
            msk8 = CAT2(msk_, __msk);                                        \
            dst32 = in(src32, msk8);                                         \
            CAT3(dst_, __dst, _set)(dst32);                                  \
        }                                                                    \
    }

/* clang-format off */
#define MAKE_TWIN_in_op_msks(op, dst, src)        \
    CAT2(MAKE_TWIN_in_, op)(dst, src, argb32)     \
    CAT2(MAKE_TWIN_in_, op)(dst, src, rgb16)      \
    CAT2(MAKE_TWIN_in_, op)(dst, src, a8)         \
    CAT2(MAKE_TWIN_in_, op)(dst, src, c)
/* clang-format on */

#define MAKE_TWIN_in_op_srcs_msks(op, dst)                                     \
    MAKE_TWIN_in_op_msks(op, dst, argb32) MAKE_TWIN_in_op_msks(op, dst, rgb16) \
        MAKE_TWIN_in_op_msks(op, dst, a8) MAKE_TWIN_in_op_msks(op, dst, c)

#define MAKE_TWIN_in_op_dsts_srcs_msks(op)                                     \
    MAKE_TWIN_in_op_srcs_msks(op, argb32) MAKE_TWIN_in_op_srcs_msks(op, rgb16) \
        MAKE_TWIN_in_op_srcs_msks(op, a8)

MAKE_TWIN_in_op_dsts_srcs_msks(over) MAKE_TWIN_in_op_dsts_srcs_msks(source)

#define MAKE_TWIN_over(__dst, __src)                                       \
    void _twin_op_name(__src, _over_, __dst)(twin_pointer_t dst,           \
                                             twin_source_u src, int width) \
    {                                                                      \
        twin_argb32_t dst32;                                               \
        twin_argb32_t src32;                                               \
        while (width--) {                                                  \
            dst32 = CAT3(dst_, __dst, _get);                               \
            src32 = CAT2(src_, __src);                                     \
            dst32 = over(dst32, src32);                                    \
            CAT3(dst_, __dst, _set)(dst32);                                \
        }                                                                  \
    }

#define MAKE_TWIN_source(__dst, __src)                                       \
    void _twin_op_name(__src, _source_, __dst)(twin_pointer_t dst,           \
                                               twin_source_u src, int width) \
    {                                                                        \
        twin_argb32_t dst32;                                                 \
        twin_argb32_t src32;                                                 \
        while (width--) {                                                    \
            src32 = CAT2(src_, __src);                                       \
            dst32 = src32;                                                   \
            CAT3(dst_, __dst, _set)(dst32);                                  \
        }                                                                    \
    }

/* clang-format off */
#define MAKE_TWIN_op_srcs(op, dst)      \
    CAT2(MAKE_TWIN_, op)(dst, argb32)   \
    CAT2(MAKE_TWIN_, op)(dst, rgb16)    \
    CAT2(MAKE_TWIN_, op)(dst, a8)       \
    CAT2(MAKE_TWIN_, op)(dst, c)

#define MAKE_TWIN_op_dsts_srcs(op)      \
    MAKE_TWIN_op_srcs(op, argb32)       \
    MAKE_TWIN_op_srcs(op, rgb16)        \
    MAKE_TWIN_op_srcs(op, a8)

    MAKE_TWIN_op_dsts_srcs(over);
    MAKE_TWIN_op_dsts_srcs(source);

/* clang-format on */
