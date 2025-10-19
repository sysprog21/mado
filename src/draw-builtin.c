/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>

/* Cross-platform endianness detection
 *
 * Priority:
 * 1. Linux/glibc: <endian.h>
 * 2. macOS/BSD: <machine/endian.h>
 * 3. Bare-metal/embedded: Manual detection via compiler macros
 *
 * Note: We don't actually use any endian.h macros in this file currently,
 * but keep the includes for future compatibility.
 */
#if defined(__linux__) || defined(__GLIBC__)
#include <endian.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
#include <machine/endian.h>
#elif defined(__BYTE_ORDER__)
/* Compiler-provided endianness (GCC/Clang bare-metal) */
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __BYTE_ORDER __LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BYTE_ORDER __BIG_ENDIAN
#endif
#else
/* Fallback: Assume little-endian (x86, ARM, RISC-V default) */
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#include "twin_private.h"

/* Compositing Primitive Operations
 *
 * The following section contains low-level pixel compositing operations
 * that implement Porter-Duff compositing algebra for various pixel formats.
 * These functions are generated via C preprocessor macros to create all
 * combinations of source/mask/destination format operations.
 */

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

/* Naming convention
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

/* Built-in Renderer Implementation
 *
 * The following section implements the twin_composite() and twin_fill()
 * functions using the primitive operations defined above.
 */

/* op, src, dst */
static const twin_src_op comp2[2][4][3] = {
    [TWIN_OVER] =
        {
            [TWIN_A8] =
                {
                    _twin_a8_over_a8,
                    _twin_a8_over_rgb16,
                    _twin_a8_over_argb32,
                },
            [TWIN_RGB16] =
                {
                    _twin_rgb16_over_a8,
                    _twin_rgb16_over_rgb16,
                    _twin_rgb16_over_argb32,
                },
            [TWIN_ARGB32] =
                {
                    _twin_argb32_over_a8,
                    _twin_argb32_over_rgb16,
                    _twin_argb32_over_argb32,
                },
            {
                /* C */
                _twin_c_over_a8,
                _twin_c_over_rgb16,
                _twin_c_over_argb32,
            },
        },
    [TWIN_SOURCE] =
        {
            [TWIN_A8] =
                {
                    _twin_a8_source_a8,
                    _twin_a8_source_rgb16,
                    _twin_a8_source_argb32,
                },
            [TWIN_RGB16] =
                {
                    _twin_rgb16_source_a8,
                    _twin_rgb16_source_rgb16,
                    _twin_rgb16_source_argb32,
                },
            [TWIN_ARGB32] =
                {
                    _twin_argb32_source_a8,
                    _twin_argb32_source_rgb16,
                    _twin_argb32_source_argb32,
                },
            {
                /* C */
                _twin_c_source_a8,
                _twin_c_source_rgb16,
                _twin_c_source_argb32,
            },
        },
};

/* op, src, msk, dst */
static const twin_src_msk_op comp3[2][4][4][3] = {
    [TWIN_OVER] =
        {
            [TWIN_A8] =
                {
                    [TWIN_A8] =
                        {
                            _twin_a8_in_a8_over_a8,
                            _twin_a8_in_a8_over_rgb16,
                            _twin_a8_in_a8_over_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_a8_in_rgb16_over_a8,
                            _twin_a8_in_rgb16_over_rgb16,
                            _twin_a8_in_rgb16_over_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_a8_in_argb32_over_a8,
                            _twin_a8_in_argb32_over_rgb16,
                            _twin_a8_in_argb32_over_argb32,
                        },
                    {
                        /* C */
                        _twin_a8_in_c_over_a8,
                        _twin_a8_in_c_over_rgb16,
                        _twin_a8_in_c_over_argb32,
                    },
                },
            [TWIN_RGB16] =
                {
                    [TWIN_A8] =
                        {
                            _twin_rgb16_in_a8_over_a8,
                            _twin_rgb16_in_a8_over_rgb16,
                            _twin_rgb16_in_a8_over_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_rgb16_in_rgb16_over_a8,
                            _twin_rgb16_in_rgb16_over_rgb16,
                            _twin_rgb16_in_rgb16_over_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_rgb16_in_argb32_over_a8,
                            _twin_rgb16_in_argb32_over_rgb16,
                            _twin_rgb16_in_argb32_over_argb32,
                        },
                    {
                        /* C */
                        _twin_rgb16_in_c_over_a8,
                        _twin_rgb16_in_c_over_rgb16,
                        _twin_rgb16_in_c_over_argb32,
                    },
                },
            [TWIN_ARGB32] =
                {
                    [TWIN_A8] =
                        {
                            _twin_argb32_in_a8_over_a8,
                            _twin_argb32_in_a8_over_rgb16,
                            _twin_argb32_in_a8_over_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_argb32_in_rgb16_over_a8,
                            _twin_argb32_in_rgb16_over_rgb16,
                            _twin_argb32_in_rgb16_over_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_argb32_in_argb32_over_a8,
                            _twin_argb32_in_argb32_over_rgb16,
                            _twin_argb32_in_argb32_over_argb32,
                        },
                    {
                        /* C */
                        _twin_argb32_in_c_over_a8,
                        _twin_argb32_in_c_over_rgb16,
                        _twin_argb32_in_c_over_argb32,
                    },
                },
            {
                /* C */
                [TWIN_A8] =
                    {
                        _twin_c_in_a8_over_a8,
                        _twin_c_in_a8_over_rgb16,
                        _twin_c_in_a8_over_argb32,
                    },
                [TWIN_RGB16] =
                    {
                        _twin_c_in_rgb16_over_a8,
                        _twin_c_in_rgb16_over_rgb16,
                        _twin_c_in_rgb16_over_argb32,
                    },
                [TWIN_ARGB32] =
                    {
                        _twin_c_in_argb32_over_a8,
                        _twin_c_in_argb32_over_rgb16,
                        _twin_c_in_argb32_over_argb32,
                    },
                {
                    /* C */
                    _twin_c_in_c_over_a8,
                    _twin_c_in_c_over_rgb16,
                    _twin_c_in_c_over_argb32,
                },
            },
        },
    [TWIN_SOURCE] =
        {
            [TWIN_A8] =
                {
                    [TWIN_A8] =
                        {
                            _twin_a8_in_a8_source_a8,
                            _twin_a8_in_a8_source_rgb16,
                            _twin_a8_in_a8_source_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_a8_in_rgb16_source_a8,
                            _twin_a8_in_rgb16_source_rgb16,
                            _twin_a8_in_rgb16_source_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_a8_in_argb32_source_a8,
                            _twin_a8_in_argb32_source_rgb16,
                            _twin_a8_in_argb32_source_argb32,
                        },
                    {
                        /* C */
                        _twin_a8_in_c_source_a8,
                        _twin_a8_in_c_source_rgb16,
                        _twin_a8_in_c_source_argb32,
                    },
                },
            [TWIN_RGB16] =
                {
                    [TWIN_A8] =
                        {
                            _twin_rgb16_in_a8_source_a8,
                            _twin_rgb16_in_a8_source_rgb16,
                            _twin_rgb16_in_a8_source_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_rgb16_in_rgb16_source_a8,
                            _twin_rgb16_in_rgb16_source_rgb16,
                            _twin_rgb16_in_rgb16_source_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_rgb16_in_argb32_source_a8,
                            _twin_rgb16_in_argb32_source_rgb16,
                            _twin_rgb16_in_argb32_source_argb32,
                        },
                    {
                        /* C */
                        _twin_rgb16_in_c_source_a8,
                        _twin_rgb16_in_c_source_rgb16,
                        _twin_rgb16_in_c_source_argb32,
                    },
                },
            [TWIN_ARGB32] =
                {
                    [TWIN_A8] =
                        {
                            _twin_argb32_in_a8_source_a8,
                            _twin_argb32_in_a8_source_rgb16,
                            _twin_argb32_in_a8_source_argb32,
                        },
                    [TWIN_RGB16] =
                        {
                            _twin_argb32_in_rgb16_source_a8,
                            _twin_argb32_in_rgb16_source_rgb16,
                            _twin_argb32_in_rgb16_source_argb32,
                        },
                    [TWIN_ARGB32] =
                        {
                            _twin_argb32_in_argb32_source_a8,
                            _twin_argb32_in_argb32_source_rgb16,
                            _twin_argb32_in_argb32_source_argb32,
                        },
                    {
                        /* C */
                        _twin_argb32_in_c_source_a8,
                        _twin_argb32_in_c_source_rgb16,
                        _twin_argb32_in_c_source_argb32,
                    },
                },
            {
                /* C */
                [TWIN_A8] =
                    {
                        _twin_c_in_a8_source_a8,
                        _twin_c_in_a8_source_rgb16,
                        _twin_c_in_a8_source_argb32,
                    },
                [TWIN_RGB16] =
                    {
                        _twin_c_in_rgb16_source_a8,
                        _twin_c_in_rgb16_source_rgb16,
                        _twin_c_in_rgb16_source_argb32,
                    },
                [TWIN_ARGB32] =
                    {
                        _twin_c_in_argb32_source_a8,
                        _twin_c_in_argb32_source_rgb16,
                        _twin_c_in_argb32_source_argb32,
                    },
                {
                    /* C */
                    _twin_c_in_c_source_a8,
                    _twin_c_in_c_source_rgb16,
                    _twin_c_in_c_source_argb32,
                },
            },
        },
};


#define operand_index(o) \
    ((o)->source_kind == TWIN_SOLID ? 3 : o->u.pixmap->format)

/* FIXME: source clipping is busted */
static void _twin_composite_simple(twin_pixmap_t *dst,
                                   twin_coord_t dst_x,
                                   twin_coord_t dst_y,
                                   twin_operand_t *src,
                                   twin_coord_t src_x,
                                   twin_coord_t src_y,
                                   twin_operand_t *msk,
                                   twin_coord_t msk_x,
                                   twin_coord_t msk_y,
                                   twin_operator_t operator,
                                   twin_coord_t width,
                                   twin_coord_t height)
{
    twin_coord_t iy;
    twin_coord_t left, top, right, bottom;
    twin_coord_t sdx, sdy;
    twin_source_u s;

    dst_x += dst->origin_x;
    dst_y += dst->origin_y;
    left = dst_x;
    top = dst_y;
    right = dst_x + width;
    bottom = dst_y + height;

    /* clip */
    if (left < dst->clip.left)
        left = dst->clip.left;
    if (top < dst->clip.top)
        top = dst->clip.top;
    if (right > dst->clip.right)
        right = dst->clip.right;
    if (bottom > dst->clip.bottom)
        bottom = dst->clip.bottom;

    if (left >= right || top >= bottom)
        return;

    if (src->source_kind == TWIN_PIXMAP) {
        src_x += src->u.pixmap->origin_x;
        src_y += src->u.pixmap->origin_y;
    } else {
        s.c = src->u.argb;
    }

    sdx = src_x - dst_x;
    sdy = src_y - dst_y;

    if (msk) {
        twin_source_u m;
        twin_coord_t mdx, mdy;

        if (msk->source_kind == TWIN_PIXMAP) {
            msk_x += msk->u.pixmap->origin_x;
            msk_y += msk->u.pixmap->origin_y;
        } else {
            m.c = msk->u.argb;
        }

        mdx = msk_x - dst_x;
        mdy = msk_y - dst_y;

        twin_src_msk_op op = comp3[operator][operand_index(src)][operand_index(msk)][dst->format];
        for (iy = top; iy < bottom; iy++) {
            if (src->source_kind == TWIN_PIXMAP)
                s.p = twin_pixmap_pointer(src->u.pixmap, left + sdx, iy + sdy);
            if (msk->source_kind == TWIN_PIXMAP)
                m.p = twin_pixmap_pointer(msk->u.pixmap, left + mdx, iy + mdy);
            (*op)(twin_pixmap_pointer(dst, left, iy), s, m, right - left);
        }
    } else {
        twin_src_op op = comp2[operator][operand_index(src)][dst->format];

        for (iy = top; iy < bottom; iy++) {
            if (src->source_kind == TWIN_PIXMAP)
                s.p = twin_pixmap_pointer(src->u.pixmap, left + sdx, iy + sdy);
            (*op)(twin_pixmap_pointer(dst, left, iy), s, right - left);
        }
    }
    twin_pixmap_damage(dst, left, top, right, bottom);
}

static inline int operand_xindex(twin_operand_t *o)
{
    int ind = operand_index(o);

    return ind != TWIN_RGB16 ? ind : TWIN_ARGB32;
}

static twin_xform_t *twin_pixmap_init_xform(twin_pixmap_t *pixmap,
                                            twin_coord_t left,
                                            twin_coord_t width,
                                            twin_coord_t src_x,
                                            twin_coord_t src_y)
{
    twin_format_t fmt = pixmap->format;

    if (fmt == TWIN_RGB16)
        fmt = TWIN_ARGB32;

    size_t required_size =
        sizeof(twin_xform_t) + (size_t) width * twin_bytes_per_pixel(fmt);

    /* Reuse cached xform buffer if large enough */
    twin_xform_t *xform;
    if (pixmap->xform_cache && pixmap->xform_cache_size >= required_size) {
        xform = (twin_xform_t *) pixmap->xform_cache;
    } else {
        /* Need larger cache - reallocate */
        void *new_cache = realloc(pixmap->xform_cache, required_size);
        if (!new_cache)
            return NULL;
        pixmap->xform_cache = new_cache;
        pixmap->xform_cache_size = required_size;
        xform = (twin_xform_t *) new_cache;
    }

    xform->span.v = (twin_argb32_t *) (char *) (xform + 1);
    xform->pixmap = pixmap;
    xform->left = left;
    xform->width = width;
    xform->src_x = src_x;
    xform->src_y = src_y;

    return xform;
}

static void twin_pixmap_free_xform(twin_xform_t *xform)
{
    /* Xform buffer is now cached in pixmap - don't free */
    /* This function is kept for API compatibility but does nothing */
    (void) xform;
}

#define FX(x) twin_int_to_fixed(x)
#define XF(x) twin_fixed_to_int(x)

/* we are doing clipping on source... dunno if that makes much sense
 * but here we go ... if we decide that source clipping makes no sense
 * then we need to still test wether we fit in the pixmap boundaries
 * here. source clipping is useful if you try to extract one image
 * out of a big picture though.
 */
#define _pix_clipped(pix, x, y)                                    \
    ((x) < FX((pix)->clip.left) || (x) >= FX((pix)->clip.right) || \
     (y) < FX((pix)->clip.top) || (y) >= FX((pix)->clip.bottom))

#define _get_pix_8(d, pix, x, y)                                    \
    do {                                                            \
        (d) = _pix_clipped(pix, x, y)                               \
                  ? 0                                               \
                  : *((pix)->p.a8 + XF(y) * (pix)->stride + XF(x)); \
    } while (0)

#define _get_pix_16(d, pix, x, y)                                            \
    do {                                                                     \
        twin_rgb16_t p =                                                     \
            _pix_clipped(pix, x, y)                                          \
                ? 0                                                          \
                : *((pix)->p.argb32 + XF(y) * ((pix)->stride >> 1) + XF(x)); \
        *((twin_argb32_t *) (char *) (d)) = twin_rgb16_to_argb32(p);         \
    } while (0)

#define _get_pix_32(d, pix, x, y)                                            \
    do {                                                                     \
        twin_argb32_t p =                                                    \
            _pix_clipped(pix, x, y)                                          \
                ? 0                                                          \
                : *((pix)->p.argb32 + XF(y) * ((pix)->stride >> 2) + XF(x)); \
        *((twin_argb32_t *) (char *) (d)) = p;                               \
    } while (0)

#define _pix_saucemix(tl, tr, bl, br, wx, wy)                     \
    ((((((br * wx) + (bl * (TWIN_FIXED_ONE - wx))) >> 16) * wy) + \
      ((((tr * wx) + (tl * (TWIN_FIXED_ONE - wx))) >> 16) *       \
       (TWIN_FIXED_ONE - wy))) >>                                 \
     16)

static void twin_pixmap_read_xform_8(twin_xform_t *xform, twin_coord_t line)
{
    twin_fixed_t dx, dy, sx, sy;
    unsigned int wx, wy;
    twin_a8_t pts[4];
    twin_a8_t *dst = xform->span.a8;
    twin_pixmap_t *pix = xform->pixmap;
    twin_matrix_t *tfm = &xform->pixmap->transform;

    /* for each row in the dest line */
    dy = twin_int_to_fixed(line);
    for (dx = 0; dx < twin_int_to_fixed(xform->width); dx += TWIN_FIXED_ONE) {
        sx = _twin_matrix_fx(tfm, dx, dy) + FX(xform->src_x);
        sy = _twin_matrix_fy(tfm, dx, dy) + FX(xform->src_y);
        _get_pix_8(pts[0], pix, sx, sy);
        _get_pix_8(pts[1], pix, sx + TWIN_FIXED_ONE, sy);
        _get_pix_8(pts[2], pix, sx, sy + TWIN_FIXED_ONE);
        _get_pix_8(pts[3], pix, sx + TWIN_FIXED_ONE, sy + TWIN_FIXED_ONE);
        wx = sx & 0xffff;
        wy = sy & 0xffff;
        *(dst++) = _pix_saucemix(pts[0], pts[1], pts[2], pts[3], wx, wy);
    }
}

static void twin_pixmap_read_xform_16(twin_xform_t *xform, twin_coord_t line)
{
    twin_fixed_t dx, dy, sx, sy;
    unsigned int wx, wy;
    twin_a8_t pts[4][4];
    twin_a8_t *dst = xform->span.a8;
    twin_pixmap_t *pix = xform->pixmap;
    twin_matrix_t *tfm = &xform->pixmap->transform;

    /* for each row in the dest line */
    dy = twin_int_to_fixed(line);
    for (dx = 0; dx < twin_int_to_fixed(xform->width); dx += TWIN_FIXED_ONE) {
        sx = _twin_matrix_fx(tfm, dx, dy) + FX(xform->src_x);
        sy = _twin_matrix_fy(tfm, dx, dy) + FX(xform->src_y);
        _get_pix_16(pts[0], pix, sx, sy);
        _get_pix_16(pts[1], pix, sx + TWIN_FIXED_ONE, sy);
        _get_pix_16(pts[2], pix, sx, sy + TWIN_FIXED_ONE);
        _get_pix_16(pts[3], pix, sx + TWIN_FIXED_ONE, sy + TWIN_FIXED_ONE);
        wx = sx & 0xffff;
        wy = sy & 0xffff;
        *(dst++) =
            _pix_saucemix(pts[0][0], pts[1][0], pts[2][0], pts[3][0], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][1], pts[1][1], pts[2][1], pts[3][1], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][2], pts[1][2], pts[2][2], pts[3][2], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][3], pts[1][3], pts[2][3], pts[3][3], wx, wy);
    }
}

static void twin_pixmap_read_xform_32(twin_xform_t *xform, twin_coord_t line)
{
    twin_fixed_t dx, dy, sx, sy;
    unsigned int wx, wy;
    twin_a8_t pts[4][4];
    twin_a8_t *dst = xform->span.a8;
    twin_pixmap_t *pix = xform->pixmap;
    twin_matrix_t *tfm = &xform->pixmap->transform;

    /* for each row in the dest line */
    dy = twin_int_to_fixed(line);
    for (dx = 0; dx < twin_int_to_fixed(xform->width); dx += TWIN_FIXED_ONE) {
        sx = _twin_matrix_fx(tfm, dx, dy) + FX(xform->src_x);
        sy = _twin_matrix_fy(tfm, dx, dy) + FX(xform->src_y);
        _get_pix_32(pts[0], pix, sx, sy);
        _get_pix_32(pts[1], pix, sx + TWIN_FIXED_ONE, sy);
        _get_pix_32(pts[2], pix, sx, sy + TWIN_FIXED_ONE);
        _get_pix_32(pts[3], pix, sx + TWIN_FIXED_ONE, sy + TWIN_FIXED_ONE);
        wx = sx & 0xffff;
        wy = sy & 0xffff;
        *(dst++) =
            _pix_saucemix(pts[0][0], pts[1][0], pts[2][0], pts[3][0], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][1], pts[1][1], pts[2][1], pts[3][1], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][2], pts[1][2], pts[2][2], pts[3][2], wx, wy);
        *(dst++) =
            _pix_saucemix(pts[0][3], pts[1][3], pts[2][3], pts[3][3], wx, wy);
    }
}

static void twin_pixmap_read_xform(twin_xform_t *xform, twin_coord_t line)
{
    if (xform->pixmap->format == TWIN_A8)
        twin_pixmap_read_xform_8(xform, line);
    else if (xform->pixmap->format == TWIN_RGB16)
        twin_pixmap_read_xform_16(xform, line);
    else if (xform->pixmap->format == TWIN_ARGB32)
        twin_pixmap_read_xform_32(xform, line);
}

static void _twin_composite_xform(twin_pixmap_t *dst,
                                  twin_coord_t dst_x,
                                  twin_coord_t dst_y,
                                  twin_operand_t *src,
                                  twin_coord_t src_x,
                                  twin_coord_t src_y,
                                  twin_operand_t *msk,
                                  twin_coord_t msk_x,
                                  twin_coord_t msk_y,
                                  twin_operator_t operator,
                                  twin_coord_t width,
                                  twin_coord_t height)
{
    twin_coord_t left, top, right, bottom;
    twin_xform_t *sxform = NULL, *mxform = NULL;
    twin_source_u s;

    dst_x += dst->origin_x;
    dst_y += dst->origin_y;
    left = dst_x;
    top = dst_y;
    right = dst_x + width;
    bottom = dst_y + height;

    /* clip */
    if (left < dst->clip.left)
        left = dst->clip.left;
    if (top < dst->clip.top)
        top = dst->clip.top;
    if (right > dst->clip.right)
        right = dst->clip.right;
    if (bottom > dst->clip.bottom)
        bottom = dst->clip.bottom;

    if (left >= right || top >= bottom)
        return;

    width = right - left;
    height = bottom - top;

    if (src->source_kind == TWIN_PIXMAP) {
        src_x += src->u.pixmap->origin_x;
        src_y += src->u.pixmap->origin_y;
        sxform =
            twin_pixmap_init_xform(src->u.pixmap, left, width, src_x, src_y);
        if (!sxform)
            return;
        s.p = sxform->span;
    } else {
        s.c = src->u.argb;
    }

    if (msk) {
        twin_source_u m;

        if (msk->source_kind == TWIN_PIXMAP) {
            msk_x += msk->u.pixmap->origin_x;
            msk_y += msk->u.pixmap->origin_y;
            mxform = twin_pixmap_init_xform(msk->u.pixmap, left, width, msk_x,
                                            msk_y);
            if (!mxform)
                return;
            m.p = mxform->span;
        } else {
            m.c = msk->u.argb;
        }

        twin_src_msk_op op = comp3[operator][operand_xindex(src)][operand_xindex(msk)]
		[dst->format];
        for (twin_coord_t iy = top; iy < bottom; iy++) {
            if (src->source_kind == TWIN_PIXMAP)
                twin_pixmap_read_xform(sxform, iy - top);
            if (msk->source_kind == TWIN_PIXMAP)
                twin_pixmap_read_xform(mxform, iy - top);
            (*op)(twin_pixmap_pointer(dst, left, iy), s, m, right - left);
        }
    } else {
        twin_src_op op = comp2[operator][operand_xindex(src)][dst->format];

        for (twin_coord_t iy = top; iy < bottom; iy++) {
            if (src->source_kind == TWIN_PIXMAP)
                twin_pixmap_read_xform(sxform, iy - top);
            (*op)(twin_pixmap_pointer(dst, left, iy), s, right - left);
        }
    }
    twin_pixmap_damage(dst, left, top, right, bottom);
    twin_pixmap_free_xform(sxform);
    twin_pixmap_free_xform(mxform);
}

void twin_composite(twin_pixmap_t *dst,
                    twin_coord_t dst_x,
                    twin_coord_t dst_y,
                    twin_operand_t *src,
                    twin_coord_t src_x,
                    twin_coord_t src_y,
                    twin_operand_t *msk,
                    twin_coord_t msk_x,
                    twin_coord_t msk_y,
                    twin_operator_t operator,
                    twin_coord_t width,
                    twin_coord_t height)
{
    if ((src->source_kind == TWIN_PIXMAP &&
         !twin_matrix_is_identity(&src->u.pixmap->transform)) ||
        (msk && (msk->source_kind == TWIN_PIXMAP &&
                 !twin_matrix_is_identity(&msk->u.pixmap->transform)))) {
        _twin_composite_xform(dst, dst_x, dst_y, src, src_x, src_y, msk, msk_x,
                              msk_y, operator, width, height);
    } else {
        _twin_composite_simple(dst, dst_x, dst_y, src, src_x, src_y, msk, msk_x,
                               msk_y, operator, width, height);
    }
}

/*
 * array primary    index is OVER SOURCE
 * array secondary  index is ARGB32 RGB16 A8
 */
static twin_src_op fill[2][3] = {
    [TWIN_OVER] =
        {
            _twin_c_over_a8,
            _twin_c_over_rgb16,
            _twin_c_over_argb32,
        },
    [TWIN_SOURCE] =
        {
            _twin_c_source_a8,
            _twin_c_source_rgb16,
            _twin_c_source_argb32,
        },
};

void twin_fill(twin_pixmap_t *dst,
               twin_argb32_t pixel,
               twin_operator_t operator,
               twin_coord_t left,
               twin_coord_t top,
               twin_coord_t right,
               twin_coord_t bottom)
{
    /* offset */
    left += dst->origin_x;
    top += dst->origin_y;
    right += dst->origin_x;
    bottom += dst->origin_y;

    /* clip */
    if (left < dst->clip.left)
        left = dst->clip.left;
    if (right > dst->clip.right)
        right = dst->clip.right;
    if (top < dst->clip.top)
        top = dst->clip.top;
    if (bottom > dst->clip.bottom)
        bottom = dst->clip.bottom;
    if (left >= right || top >= bottom)
        return;

    twin_source_u src = {.c = pixel};
    twin_src_op op = fill[operator][dst->format];
    for (twin_coord_t iy = top; iy < bottom; iy++)
        (*op)(twin_pixmap_pointer(dst, left, iy), src, right - left);
    twin_pixmap_damage(dst, left, top, right, bottom);
}
