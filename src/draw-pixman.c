/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <pixman.h>
#include "twin_private.h"

static void twin_argb32_to_pixman_color(twin_argb32_t argb,
                                        pixman_color_t *color)
{
    /* Extract ARGB every Byte */
    uint16_t a = (argb >> 24) & 0xFF;
    uint16_t r = (argb >> 16) & 0xFF;
    uint16_t g = (argb >> 8) & 0xFF;
    uint16_t b = argb & 0xFF;

    /* 8bits -> 16bits (255 -> 65535) */
    color->alpha = (a << 8) + a;
    color->red = (r << 8) + r;
    color->green = (g << 8) + g;
    color->blue = (b << 8) + b;
}

static const pixman_format_code_t twin_pixman_format[3] = {
    [TWIN_A8] = PIXMAN_a8,
    [TWIN_RGB16] = PIXMAN_r5g6b5,
    [TWIN_ARGB32] = PIXMAN_a8r8g8b8};

static pixman_format_code_t twin_to_pixman_format(
    const twin_format_t twin_format)
{
    return twin_pixman_format[twin_format];
}

static const pixman_op_t twin_pixman_op[2] =
    {[TWIN_OVER] = PIXMAN_OP_OVER, [TWIN_SOURCE] = PIXMAN_OP_SRC};

static pixman_op_t twin_to_pixman_op(const twin_operator_t twin_op)
{
    return twin_pixman_op[twin_op];
}

#define create_pixman_image_from_twin_pixmap(pixmap)                       \
    ({                                                                     \
        typeof(pixmap) _pixmap = (pixmap);                                 \
        pixman_image_create_bits(twin_to_pixman_format((_pixmap)->format), \
                                 (_pixmap)->width, (_pixmap)->height,      \
                                 (_pixmap)->p.argb32, (_pixmap)->stride);  \
    })

static void pixmap_matrix_scale(pixman_image_t *src, twin_matrix_t *matrix)
{
    pixman_transform_t transform;
    pixman_transform_init_identity(&transform);

    pixman_fixed_t sx = matrix->m[0][0], sy = matrix->m[1][1];

    pixman_transform_scale(&transform, NULL, sx, sy);
    pixman_image_set_transform(src, &transform);
}

void twin_composite(twin_pixmap_t *_dst,
                    twin_coord_t dst_x,
                    twin_coord_t dst_y,
                    twin_operand_t *_src,
                    twin_coord_t src_x,
                    twin_coord_t src_y,
                    twin_operand_t *_msk,
                    twin_coord_t msk_x,
                    twin_coord_t msk_y,
                    twin_operator_t operator,
                    twin_coord_t width,
                    twin_coord_t height)
{
    pixman_image_t *src;
    if (_src->source_kind == TWIN_SOLID) {
        pixman_color_t source_pixel;
        twin_argb32_to_pixman_color(_src->u.argb, &source_pixel);
        src = pixman_image_create_solid_fill(&source_pixel);
    } else {
        twin_pixmap_t *src_pixmap = _src->u.pixmap;
        src = create_pixman_image_from_twin_pixmap(src_pixmap);

        if (!twin_matrix_is_identity(&(src_pixmap->transform)))
            pixmap_matrix_scale(src, &(src_pixmap->transform));
    }

    pixman_image_t *dst = create_pixman_image_from_twin_pixmap(_dst);

    /* Set origin */
    twin_coord_t ox, oy, offset_x = 0, offset_y = 0;
    twin_pixmap_get_origin(_dst, &ox, &oy);
    ox += dst_x;
    oy += dst_y;

    if (ox < _dst->clip.left) {
        offset_x = _dst->clip.left - ox;
        ox = _dst->clip.left;
    }
    if (oy < _dst->clip.top) {
        offset_y = _dst->clip.top - oy;
        oy = _dst->clip.top;
    }
    if (ox + width > _dst->clip.right)
        width = _dst->clip.right - ox;
    if (oy + height > _dst->clip.bottom)
        height = _dst->clip.bottom - oy;

    if (width < 0 || height < 0)
        return;

    if (!_msk) {
        pixman_image_composite(twin_to_pixman_op(operator), src, NULL, dst,
                               src_x + offset_x, src_y + offset_y, offset_x,
                               offset_y, ox, oy, width, height);
    } else {
        pixman_image_t *msk =
            create_pixman_image_from_twin_pixmap(_msk->u.pixmap);
        pixman_image_composite(twin_to_pixman_op(operator), src, msk, dst,
                               src_x + offset_x, src_y + offset_y,
                               msk_x + offset_x, msk_y + offset_y, ox, oy,
                               width, height);
        pixman_image_unref(msk);
    }

    pixman_image_unref(src);
    pixman_image_unref(dst);
}

void twin_fill(twin_pixmap_t *_dst,
               twin_argb32_t pixel,
               twin_operator_t operator,
               twin_coord_t left,
               twin_coord_t top,
               twin_coord_t right,
               twin_coord_t bottom)
{
    /* offset */
    left += _dst->origin_x;
    top += _dst->origin_y;
    right += _dst->origin_x;
    bottom += _dst->origin_y;

    /* clip */
    if (left < _dst->clip.left)
        left = _dst->clip.left;
    if (right > _dst->clip.right)
        right = _dst->clip.right;
    if (top < _dst->clip.top)
        top = _dst->clip.top;
    if (bottom > _dst->clip.bottom)
        bottom = _dst->clip.bottom;
    if (left >= right || top >= bottom)
        return;

    pixman_image_t *dst = create_pixman_image_from_twin_pixmap(_dst);
    pixman_color_t color;
    twin_argb32_to_pixman_color(pixel, &color);
    /* clang-format off */
    pixman_image_fill_rectangles(
        twin_to_pixman_op(operator), dst, &color, 1,
        &(pixman_rectangle16_t) {left, top, right - left, bottom - top});
    /* clang-format on */

    twin_pixmap_damage(_dst, left, top, right, bottom);

    pixman_image_unref(dst);
}
