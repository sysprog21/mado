/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "twin.h"
#include "twin_private.h"

#define D(x) twin_double_to_fixed(x)
#define GET_COLOR(ctx, idx) ctx->colors[idx]
#define PIXEL_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

enum {
    /* end of document This command determines the end of file. */
    TVG_CMD_END_DOCUMENT = 0,
    /* fill polygon This command fills an N-gon.*/
    TVG_CMD_FILL_POLYGON,
    /* fill rectangles This command fills a set of rectangles.*/
    TVG_CMD_FILL_RECTANGLES,
    /* fill path This command fills a free-form path.*/
    TVG_CMD_FILL_PATH,
    /* draw lines This command draws a set of lines.*/
    TVG_CMD_DRAW_LINES,
    /* draw line loop This command draws the outline of a polygon.*/
    TVG_CMD_DRAW_LINE_LOOP,
    /* draw line strip This command draws a list of end-to-end lines.*/
    TVG_CMD_DRAW_LINE_STRIP,
    /* draw line path This command draws a free-form path. */
    TVG_CMD_DRAW_LINE_PATH,
    /* outline fill polygon This command draws a filled polygon with an outline.
     */
    TVG_CMD_OUTLINE_FILL_POLYGON,
    /*
     * outline fill rectangles This command draws several filled
     * rectangles with an outline.
     */
    TVG_CMD_OUTLINE_FILL_RECTANGLES,
    /*
     * outline fill path This command combines the fill and draw
     * line path command into one
     */
    TVG_CMD_OUTLINE_FILL_PATH
};

enum {
    /* solid color */
    TVG_STYLE_FLAT = 0,
    /* linear gradient */
    TVG_STYLE_LINEAR,
    /* radial gradient */
    TVG_STYLE_RADIAL
};

enum {
    /* unit uses 16 bit */
    TVG_RANGE_DEFAULT = 0,
    /* unit takes only 8 bit */
    TVG_RANGE_REDUCED,
    /* unit uses 32 bit */
    TVG_RANGE_ENHANCED,
};

/*
 * The color encoding used in a TinyVG file.
 * This enum describes how the data in the color table
 * section of the format looks like.
 */
enum {
    TVG_COLOR_U8888 = 0,
    TVG_COLOR_U565,
    TVG_COLOR_F32,
    TVG_COLOR_CUSTOM,
};

/*
 * A TinyVG scale value defines the scale for all units inside a graphic.
 * The scale is defined by the number of decimal bits in a `i32`, thus scaling
 * can be trivially implemented by shifting the integers right by the scale
 * bits.
 */
enum {
    TVG_SCALE_1_1 = 0,
    TVG_SCALE_1_2,
    TVG_SCALE_1_4,
    TVG_SCALE_1_8,
    TVG_SCALE_1_16,
    TVG_SCALE_1_32,
    TVG_SCALE_1_64,
    TVG_SCALE_1_128,
    TVG_SCALE_1_256,
    TVG_SCALE_1_512,
    TVG_SCALE_1_1024,
    TVG_SCALE_1_2048,
    TVG_SCALE_1_4096,
    TVG_SCALE_1_8192,
    TVG_SCALE_1_16384,
    TVG_SCALE_1_32768,
};

/* path commands */
enum {
    TVG_PATH_LINE = 0,
    TVG_PATH_HLINE,
    TVG_PATH_VLINE,
    TVG_PATH_CUBIC,
    TVG_PATH_ARC_CIRCLE,
    TVG_PATH_ARC_ELLIPSE,
    TVG_PATH_CLOSE,
    TVG_PATH_QUAD
};

enum {
    TVG_SUCCESS = 0,
    TVG_E_INVALID_ARG,
    TVG_E_INVALID_STATE,
    TVG_E_INVALID_FORMAT,
    TVG_E_IO_ERROR,
    TVG_E_OUT_OF_MEMORY,
    TVG_E_NOT_SUPPORTED
};

/* clamp a value to a range */
#define TVG_CLAMP(x, mn, mx) (x > mx ? mx : (x < mn ? mn : x))
/* get the red channel of an RGB565 color */
#define TVG_RGB16_R(x) (x & 0x1F)
/* get the green channel of an RGB565 color */
#define TVG_RGB16_G(x) ((x >> 5) & 0x3F)
/* get the blue channel of an RGB565 color */
#define TVG_RGB16_B(x) ((x >> 11) & 0x1F)
/*
 * get the index of the command
 * essentially the command id
 */
#define TVG_CMD_INDEX(x) (x & 0x3F)
/* get the style kind flags in the command */
#define TVG_CMD_STYLE_KIND(x) ((x >> 6) & 0x3)
/*
 * get the packed size out of the size
 * and style kind packed value
 */
#define TVG_SIZE_AND_STYLE_SIZE(x) ((x & 0x3F) + 1)
/*
 * get the style kind out of the size
 * and style kind packed value
 */
#define TVG_SIZE_AND_STYLE_STYLE_KIND(x) ((x >> 6) & 0x3)
/* get the scale from the header */
#define TVG_HEADER_DATA_SCALE(x) (x & 0x0F)
/* get the color encoding from the header */
#define TVG_HEADER_DATA_COLOR_ENC(x) ((x >> 4) & 0x03)
/* get the color range from the header */
#define TVG_HEADER_DATA_RANGE(x) ((x >> 6) & 0x03)
/* get the path command index/id */
#define TVG_PATH_CMD_INDEX(x) (x & 0x7)
/* flag indicating the path has line/stroke to it */
#define TVG_PATH_CMD_HAS_LINE(x) ((x >> 4) & 0x1)
/* flag indicating the arc is a large arc */
#define TVG_ARC_LARGE(x) (x & 0x1)
/* flag indicating the sweep direction 0=left, 1=right */
#define TVG_ARC_SWEEP(x) ((x >> 1) & 1)

/* F32 pixel format */
typedef struct {
    float r, g, b, a;
} tvg_f32_pixel_t;

/* rgba32 color struct */
typedef struct {
    uint8_t r, g, b, a;
} tvg_rgba32_t;

/* TVG internal color struct */
typedef struct {
    float r, g, b;
} tvg_rgb_t;

/* TVG internal color struct with alpha channel */
typedef struct {
    float r, g, b, a;
} tvg_rgba_t;

/* coordinate */
typedef struct {
    float x, y;
} tvg_point_t;

/* rectangle */
typedef struct {
    float x, y;
    float width, height;
} tvg_rect_t;

/* gradient data */
typedef struct {
    tvg_point_t point0;
    tvg_point_t point1;
    uint32_t color0;
    uint32_t color1;
} tvg_gradient_t;

/* style data */
typedef struct {
    uint8_t kind;
    union {
        uint32_t flat;
        tvg_gradient_t linear;
        tvg_gradient_t radial;
    };
} tvg_style_t;

/* fill header */
typedef struct {
    tvg_style_t style;
    size_t size;
} tvg_fill_header_t;

/* line header */
typedef struct {
    tvg_style_t style;
    float line_width;
    size_t size;
} tvg_line_header_t;

/* line and fill header */
typedef struct {
    tvg_style_t fill_style;
    tvg_style_t line_style;
    float line_width;
    size_t size;
} tvg_line_fill_header_t;

/* used to provide an input cursor over an arbitrary source */
typedef size_t (*tvg_input_func_t)(uint8_t *data, size_t size, void *state);

typedef struct {
    /* the input source */
    tvg_input_func_t in;
    /* the user defined input state */
    void *in_state;
    /* the target pixmap */
    twin_pixmap_t *pixmap;
    twin_path_t *path;
    /* the scaling used */
    uint8_t scale;
    /* the color encoding */
    uint8_t color_encoding;
    /* the coordinate range */
    uint8_t coord_range;
    /* the width and height of the drawing */
    uint32_t width, height;
    /* the size of the color table */
    size_t colors_size;
    /* the color table (must be freed) */
    twin_argb32_t *colors;
    uint32_t current_color;
} tvg_context_t;

/*
 * the result type. TVG_SUCCESS = OK
 * anything else, is a TVG_E_xxxx error
 * code
 */
typedef int tvg_result_t;

#define __goto_if_fail(res, label, msg)              \
    do {                                             \
        if (res != TVG_SUCCESS) {                    \
            log_error("%s, error = [%d]", msg, res); \
            goto label;                              \
        }                                            \
    } while (0);

#define __return_val_if_fail(res, msg)               \
    do {                                             \
        if (res != TVG_SUCCESS) {                    \
            log_error("%s, error = [%d]", msg, res); \
            return res;                              \
        }                                            \
    } while (0);

#define READ_VALUE(read, buffer, size)                           \
    do {                                                         \
        read = ctx->in((uint8_t *) buffer, size, ctx->in_state); \
        if (size > read) {                                       \
            return TVG_E_IO_ERROR;                               \
        }                                                        \
    } while (0);

static uint32_t tvg_map_zero_to_max(tvg_context_t *ctx, uint32_t value)
{
    if (value)
        return value;
    switch (ctx->coord_range) {
    case TVG_RANGE_DEFAULT:
        return 0xFFFF;
    case TVG_RANGE_REDUCED:
        return 0xFF;
    default:
        return 0xFFFFFFFF;
    }
}

static tvg_result_t tvg_read_coord(tvg_context_t *ctx, uint32_t *out_raw_value)
{
    size_t read;
    switch (ctx->coord_range) {
    case TVG_RANGE_DEFAULT: {
        uint16_t u16;
        READ_VALUE(read, &u16, 2);
        *out_raw_value = u16;
        return TVG_SUCCESS;
    }
    case TVG_RANGE_REDUCED: {
        uint8_t u8;
        READ_VALUE(read, &u8, 1);
        *out_raw_value = u8;
        return TVG_SUCCESS;
    }
    default:
        READ_VALUE(read, out_raw_value, 4);
        return TVG_SUCCESS;
    }
}

static tvg_result_t tvg_read_color(tvg_context_t *ctx, twin_argb32_t *out_color)
{
    size_t read;
    twin_fixed_t max_color_f = twin_int_to_fixed(255);
    switch (ctx->color_encoding) {
    case TVG_COLOR_F32: {
        tvg_f32_pixel_t data;
        READ_VALUE(read, &data, sizeof(tvg_f32_pixel_t));
        /* 255.0 * A */
        twin_fixed_t a_f =
            twin_fixed_mul(max_color_f, twin_double_to_fixed(data.a));
        /* 255.0 * R */
        twin_fixed_t r_f =
            twin_fixed_mul(max_color_f, twin_double_to_fixed(data.r));
        /* 255.0 * G */
        twin_fixed_t g_f =
            twin_fixed_mul(max_color_f, twin_double_to_fixed(data.g));
        /* 255.0 * B */
        twin_fixed_t b_f =
            twin_fixed_mul(max_color_f, twin_double_to_fixed(data.b));
        uint8_t a = (uint8_t) twin_fixed_to_int(a_f);
        uint8_t r = (uint8_t) twin_fixed_to_int(r_f);
        uint8_t g = (uint8_t) twin_fixed_to_int(g_f);
        uint8_t b = (uint8_t) twin_fixed_to_int(b_f);
        *out_color = PIXEL_ARGB(a, r, g, b);
        return TVG_SUCCESS;
    }
    case TVG_COLOR_U565: {
        uint16_t data;
        READ_VALUE(read, &data, 2);
        /* (255.0 * R) / 15.0 */
        twin_fixed_t r_f = twin_fixed_div(
            twin_fixed_mul(max_color_f,
                           twin_double_to_fixed((float) TVG_RGB16_R(data))),
            twin_int_to_fixed(15));
        /* (255.0 * G) / 31.0 */
        twin_fixed_t g_f = twin_fixed_div(
            twin_fixed_mul(max_color_f,
                           twin_double_to_fixed((float) TVG_RGB16_G(data))),
            twin_int_to_fixed(31));
        /* (255.0 * B) / 15.0 */
        twin_fixed_t b_f = twin_fixed_div(
            twin_fixed_mul(max_color_f,
                           twin_double_to_fixed((float) TVG_RGB16_B(data))),
            twin_int_to_fixed(15));
        uint8_t a = 0xff;
        uint8_t r = (uint8_t) twin_fixed_to_int(r_f);
        uint8_t g = (uint8_t) twin_fixed_to_int(g_f);
        uint8_t b = (uint8_t) twin_fixed_to_int(b_f);
        *out_color = PIXEL_ARGB(a, r, g, b);
        return TVG_SUCCESS;
    }
    case TVG_COLOR_U8888: {
        tvg_rgba32_t data;
        READ_VALUE(read, &data.r, 1);
        READ_VALUE(read, &data.g, 1);
        READ_VALUE(read, &data.b, 1);
        READ_VALUE(read, &data.a, 1);
        *out_color = PIXEL_ARGB(data.a, data.r, data.g, data.b);
        return TVG_SUCCESS;
    }
    case TVG_COLOR_CUSTOM:
        return TVG_E_NOT_SUPPORTED;
    default:
        return TVG_E_INVALID_FORMAT;
    }
}

static float tvg_downscale_coord(tvg_context_t *ctx, uint32_t coord)
{
    uint16_t factor = (((uint16_t) 1) << ctx->scale);
    return (float) coord / (float) factor;
}

static tvg_result_t tvg_read_varuint(tvg_context_t *ctx, uint32_t *out_value)
{
    int count = 0;
    uint32_t result = 0;
    uint8_t byte;
    size_t read = 0;
    while (true) {
        READ_VALUE(read, &byte, 1);
        const uint32_t val = ((uint32_t) (byte & 0x7F)) << (7 * count);
        result |= val;
        if ((byte & 0x80) == 0)
            break;
        ++count;
    }
    *out_value = result;
    return TVG_SUCCESS;
}

static tvg_result_t tvg_read_unit(tvg_context_t *ctx, float *out_value)
{
    uint32_t val;
    tvg_result_t res = tvg_read_coord(ctx, &val);
    __return_val_if_fail(res, "Failed to read coordinate");
    *out_value = tvg_downscale_coord(ctx, val);
    return TVG_SUCCESS;
}

static tvg_result_t tvg_read_point(tvg_context_t *ctx, tvg_point_t *out_point)
{
    float f32;
    tvg_result_t res = tvg_read_unit(ctx, &f32);
    __return_val_if_fail(res, "Failed to read unit");
    out_point->x = f32;
    res = tvg_read_unit(ctx, &f32);
    __return_val_if_fail(res, "Failed to read unit");
    out_point->y = f32;
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_header(tvg_context_t *ctx, int dim_only)
{
    size_t read = 0;
    uint8_t data[2];
    /* read the magic number */
    READ_VALUE(read, data, 2);
    if (data[0] != 0x72 || data[1] != 0x56) {
        return TVG_E_INVALID_FORMAT;
    }
    /* read the version */
    READ_VALUE(read, data, 1);
    /* we only support version 1 */
    if (data[0] != 1) {
        return TVG_E_NOT_SUPPORTED;
    }
    /* read the scale, encoding, and coordinate range: */
    READ_VALUE(read, data, 1);
    /* it's all packed in a byte, so crack it up */
    ctx->scale = TVG_HEADER_DATA_SCALE(data[0]);
    ctx->color_encoding = TVG_HEADER_DATA_COLOR_ENC(data[0]);
    ctx->coord_range = TVG_HEADER_DATA_RANGE(data[0]);
    /* now read the width and height: */
    uint32_t tmp;
    tvg_result_t res = tvg_read_coord(ctx, &tmp);
    __return_val_if_fail(res, "Failed to read coordinate");
    ctx->width = tvg_map_zero_to_max(ctx, tmp);
    res = tvg_read_coord(ctx, &tmp);
    __return_val_if_fail(res, "Failed to read coordinate");
    ctx->height = tvg_map_zero_to_max(ctx, tmp);
    if (dim_only) {
        return TVG_SUCCESS;
    }
    /* next read the color table */
    uint32_t color_count;
    res = tvg_read_varuint(ctx, &color_count);
    __return_val_if_fail(res, "Failed to read varuint");
    if (color_count == 0) {
        return TVG_E_INVALID_FORMAT;
    }
    ctx->colors = malloc(color_count * sizeof(twin_argb32_t));
    if (!ctx->colors) {
        return TVG_E_OUT_OF_MEMORY;
    }
    ctx->colors_size = (size_t) color_count;
    for (size_t i = 0; i < ctx->colors_size; ++i) {
        res = tvg_read_color(ctx, &ctx->colors[i]);
        if (res != TVG_SUCCESS) {
            free(ctx->colors);
            ctx->colors = NULL;
            return res;
        }
    }
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_gradient(tvg_context_t *ctx,
                                       tvg_gradient_t *out_gradient)
{
    uint32_t u32;
    tvg_point_t pt;
    tvg_result_t res = tvg_read_point(ctx, &pt);
    __return_val_if_fail(res, "Failed to read point");
    out_gradient->point0 = pt;
    res = tvg_read_point(ctx, &pt);
    __return_val_if_fail(res, "Failed to read point");
    out_gradient->point1 = pt;
    res = tvg_read_varuint(ctx, &u32);
    __return_val_if_fail(res, "Failed to read varuint");
    out_gradient->color0 = u32;
    if (u32 > ctx->colors_size) {
        log_error("Invalid color index");
        return TVG_E_INVALID_FORMAT;
    }
    res = tvg_read_varuint(ctx, &u32);
    __return_val_if_fail(res, "Failed to read varuint");
    if (u32 > ctx->colors_size) {
        log_error("Invalid color index");
        return TVG_E_INVALID_FORMAT;
    }
    out_gradient->color1 = u32;
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_style(tvg_context_t *ctx,
                                    int kind,
                                    tvg_style_t *out_style)
{
    tvg_result_t res;
    uint32_t flat;
    tvg_gradient_t grad;
    out_style->kind = kind;
    switch (kind) {
    case TVG_STYLE_FLAT:
        res = tvg_read_varuint(ctx, &flat);
        __return_val_if_fail(res, "Failed to read varuint");
        out_style->flat = flat;
        break;
    case TVG_STYLE_LINEAR:
        res = tvg_parse_gradient(ctx, &grad);
        __return_val_if_fail(res, "Failed to parse gradient");
        out_style->linear = grad;
        break;
    case TVG_STYLE_RADIAL:
        res = tvg_parse_gradient(ctx, &grad);
        __return_val_if_fail(res, "Failed to parse gradient");
        out_style->radial = grad;
        break;
    default:
        res = TVG_E_INVALID_FORMAT;
        break;
    }
    return res;
}

static tvg_result_t tvg_parse_fill_header(tvg_context_t *ctx,
                                          int kind,
                                          tvg_fill_header_t *out_header)
{
    uint32_t u32;
    tvg_result_t res = tvg_read_varuint(ctx, &u32);
    __return_val_if_fail(res, "Failed to read varuint");
    size_t count = (size_t) u32 + 1;
    out_header->size = count;
    res = tvg_parse_style(ctx, kind, &out_header->style);
    __return_val_if_fail(res, "Failed to parse style");
    return res;
}

static tvg_result_t tvg_parse_line_header(tvg_context_t *ctx,
                                          int kind,
                                          tvg_line_header_t *out_header)
{
    uint32_t u32;
    tvg_result_t res = tvg_read_varuint(ctx, &u32);
    __return_val_if_fail(res, "Failed to read varuint");
    size_t count = (size_t) u32 + 1;
    out_header->size = count;
    res = tvg_parse_style(ctx, kind, &out_header->style);
    __return_val_if_fail(res, "Failed to parse style");
    res = tvg_read_unit(ctx, &out_header->line_width);
    __return_val_if_fail(res, "Failed to read unit");

    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_line_fill_header(
    tvg_context_t *ctx,
    int kind,
    tvg_line_fill_header_t *out_header)
{
    uint8_t d;
    size_t read = 0;
    READ_VALUE(read, &d, 1);
    tvg_result_t res = TVG_SUCCESS;

    size_t count = TVG_SIZE_AND_STYLE_SIZE(d);
    out_header->size = count;
    res = tvg_parse_style(ctx, kind, &out_header->fill_style);
    __return_val_if_fail(res, "Failed to parse style");
    res = tvg_parse_style(ctx, TVG_SIZE_AND_STYLE_STYLE_KIND(d),
                          &out_header->line_style);
    __return_val_if_fail(res, "Failed to parse style");
    res = tvg_read_unit(ctx, &out_header->line_width);
    __return_val_if_fail(res, "Failed to read unit");

    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_path(tvg_context_t *ctx, size_t size)
{
    tvg_result_t res = TVG_SUCCESS;
    tvg_point_t start_point, cur_point, pt;
    float f32;
    uint8_t path_info;
    twin_path_t *path = ctx->path;
    res = tvg_read_point(ctx, &pt);
    __goto_if_fail(res, error, "Failed to read point");
    twin_path_move(path, D(pt.x), D(pt.y));
    start_point = pt;
    cur_point = pt;
    size_t read = 0;
    for (size_t j = 0; j < size; ++j) {
        READ_VALUE(read, &path_info, 1);
        float line_width = 0.0f;
        if (TVG_PATH_CMD_HAS_LINE(path_info)) {
            res = tvg_read_unit(ctx, &line_width);
            __goto_if_fail(res, error, "Failed to read unit");
        }
        switch (TVG_PATH_CMD_INDEX(path_info)) {
        case TVG_PATH_LINE:
            res = tvg_read_point(ctx, &pt);
            __goto_if_fail(res, error, "Failed to read point");
            twin_path_draw(path, D(pt.x), D(pt.y));
            cur_point = pt;
            break;
        case TVG_PATH_HLINE:
            res = tvg_read_unit(ctx, &f32);
            __goto_if_fail(res, error, "Failed to read unit");
            pt.x = f32;
            pt.y = cur_point.y;
            twin_path_draw(path, D(pt.x), D(pt.y));
            cur_point = pt;
            break;
        case TVG_PATH_VLINE:
            res = tvg_read_unit(ctx, &f32);
            __goto_if_fail(res, error, "Failed to read unit");
            pt.x = cur_point.x;
            pt.y = f32;
            twin_path_draw(path, D(pt.x), D(pt.y));
            cur_point = pt;
            break;
        case TVG_PATH_CUBIC: {
            tvg_point_t ctrl_point1, ctrl_point2, end_point;
            res = tvg_read_point(ctx, &ctrl_point1);
            __goto_if_fail(res, error, "Failed to read point");
            res = tvg_read_point(ctx, &ctrl_point2);
            __goto_if_fail(res, error, "Failed to read point");
            res = tvg_read_point(ctx, &end_point);
            __goto_if_fail(res, error, "Failed to read point");
            twin_path_curve(path, D(ctrl_point1.x), D(ctrl_point1.y),
                            D(ctrl_point2.x), D(ctrl_point2.y), D(end_point.x),
                            D(end_point.y));
            cur_point = end_point;
        } break;
        case TVG_PATH_ARC_CIRCLE: {
            uint8_t circle_info;
            READ_VALUE(read, &circle_info, 1);
            float radius;
            res = tvg_read_unit(ctx, &radius);
            __goto_if_fail(res, error, "Failed to read unit");
            res = tvg_read_point(ctx, &pt);
            __goto_if_fail(res, error, "Failed to read point");
            twin_path_arc_circle(
                path, TVG_ARC_LARGE(circle_info), TVG_ARC_SWEEP(circle_info),
                D(radius), D(cur_point.x), D(cur_point.y), D(pt.x), D(pt.y));
            cur_point = pt;
        } break;
        case TVG_PATH_ARC_ELLIPSE: {
            uint8_t ellipse_info;
            READ_VALUE(read, &ellipse_info, 1);
            float radius_x, radius_y, rotation;
            res = tvg_read_unit(ctx, &radius_x);
            __goto_if_fail(res, error, "Failed to read unit");
            res = tvg_read_unit(ctx, &radius_y);
            __goto_if_fail(res, error, "Failed to read unit");
            res = tvg_read_unit(ctx, &rotation);
            __goto_if_fail(res, error, "Failed to read unit");
            res = tvg_read_point(ctx, &pt);
            __goto_if_fail(res, error, "Failed to read point");
            twin_path_arc_ellipse(
                path, TVG_ARC_LARGE(ellipse_info), TVG_ARC_SWEEP(ellipse_info),
                D(radius_x), D(radius_y), D(cur_point.x), D(cur_point.y),
                D(pt.x), D(pt.y), rotation * TWIN_ANGLE_360 / 360);
            cur_point = pt;
        } break;
        case TVG_PATH_CLOSE:
            twin_path_draw(path, D(start_point.x), D(start_point.y));
            cur_point = start_point;
            break;
        case TVG_PATH_QUAD: {
            tvg_point_t ctrl_point, end_point;
            res = tvg_read_point(ctx, &ctrl_point);
            __goto_if_fail(res, error, "Failed to read point");
            res = tvg_read_point(ctx, &end_point);
            __goto_if_fail(res, error, "Failed to read point");
            twin_path_quadratic_curve(path, D(ctrl_point.x), D(ctrl_point.y),
                                      D(end_point.x), D(end_point.y));
            cur_point = end_point;
        } break;
        default:
            res = TVG_E_INVALID_FORMAT;
            goto error;
        }
    }
error:
    return res;
}

static tvg_result_t tvg_parse_rect(tvg_context_t *ctx, tvg_rect_t *out_rect)
{
    tvg_point_t pt;
    tvg_result_t res = tvg_read_point(ctx, &pt);
    __return_val_if_fail(res, "Failed to read point");
    float w, h;
    res = tvg_read_unit(ctx, &w);
    __return_val_if_fail(res, "Failed to read unit");
    res = tvg_read_unit(ctx, &h);
    __return_val_if_fail(res, "Failed to read unit");
    out_rect->x = pt.x;
    out_rect->y = pt.y;
    out_rect->width = w;
    out_rect->height = h;
    return TVG_SUCCESS;
}

static void _stroke_path_with_style(tvg_context_t *ctx,
                                    const tvg_style_t *fill_style,
                                    twin_fixed_t pen_width)
{
    switch (fill_style->kind) {
    case TVG_STYLE_FLAT:
        twin_paint_stroke(ctx->pixmap, GET_COLOR(ctx, fill_style->flat),
                          ctx->path, pen_width);
        break;
    case TVG_STYLE_LINEAR:
        /* TODO: Implement linear gradient color */
        twin_paint_stroke(ctx->pixmap,
                          GET_COLOR(ctx, fill_style->linear.color0), ctx->path,
                          pen_width);
        break;
    case TVG_STYLE_RADIAL:
        /* TODO: Implement radial gradient color */
        twin_paint_stroke(ctx->pixmap,
                          GET_COLOR(ctx, fill_style->radial.color0), ctx->path,
                          pen_width);
        break;
    }
}

static void _fill_path_with_style(tvg_context_t *ctx,
                                  const tvg_style_t *fill_style)
{
    switch (fill_style->kind) {
    case TVG_STYLE_FLAT:
        twin_paint_path(ctx->pixmap, GET_COLOR(ctx, fill_style->flat),
                        ctx->path);
        break;
    case TVG_STYLE_LINEAR:
        /* TODO: Implement linear gradient color */
        twin_paint_path(ctx->pixmap, GET_COLOR(ctx, fill_style->linear.color0),
                        ctx->path);
        break;
    case TVG_STYLE_RADIAL:
        /* TODO: Implement radial gradient color */
        twin_paint_path(ctx->pixmap, GET_COLOR(ctx, fill_style->radial.color0),
                        ctx->path);
        break;
    }
}

static tvg_result_t tvg_parse_fill_rectangles(tvg_context_t *ctx,
                                              size_t size,
                                              const tvg_style_t *fill_style)
{
    size_t count = size;
    tvg_result_t res;
    tvg_rect_t r;
    twin_path_t *path = ctx->path;
    while (count--) {
        res = tvg_parse_rect(ctx, &r);
        __return_val_if_fail(res, "Failed to parse rect");
        twin_path_rectangle(path, D(r.x), D(r.y), D(r.width), D(r.height));
        _fill_path_with_style(ctx, fill_style);
        twin_path_empty(path);
    }
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_line_fill_rectangles(
    tvg_context_t *ctx,
    size_t size,
    const tvg_style_t *fill_style,
    const tvg_style_t *line_style,
    float line_width)
{
    size_t count = size;
    tvg_result_t res;
    tvg_rect_t r;
    if (line_width == 0) {
        line_width = .01;
    }
    twin_path_t *path = ctx->path;
    while (count--) {
        res = tvg_parse_rect(ctx, &r);
        __return_val_if_fail(res, "Failed to parse rect");

        twin_path_rectangle(path, D(r.x), D(r.y), D(r.width), D(r.height));
        _fill_path_with_style(ctx, fill_style);
        _stroke_path_with_style(ctx, line_style, D(line_width));
        twin_path_empty(path);
    }
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_fill_paths(tvg_context_t *ctx,
                                         size_t size,
                                         const tvg_style_t *style)
{
    tvg_result_t res = TVG_SUCCESS;
    uint32_t *sizes = malloc(size * sizeof(uint32_t));
    if (!sizes) {
        return TVG_E_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < size; ++i) {
        res = tvg_read_varuint(ctx, &sizes[i]);
        ++sizes[i];
        __goto_if_fail(res, error, "Failed to read varuint");
    }
    twin_path_t *path = ctx->path;
    /* parse path */
    for (size_t i = 0; i < size; ++i) {
        res = tvg_parse_path(ctx, sizes[i]);
        __goto_if_fail(res, error, "Failed to parse path");
    }
    _fill_path_with_style(ctx, style);
    twin_path_empty(path);
error:
    free(sizes);
    return res;
}

static tvg_result_t tvg_parse_line_paths(tvg_context_t *ctx,
                                         size_t size,
                                         const tvg_style_t *line_style,
                                         float line_width)
{
    tvg_result_t res = TVG_SUCCESS;
    uint32_t *sizes = malloc(size * sizeof(uint32_t));
    if (!sizes) {
        return TVG_E_OUT_OF_MEMORY;
    }
    twin_path_t *path = ctx->path;
    for (size_t i = 0; i < size; ++i) {
        res = tvg_read_varuint(ctx, &sizes[i]);
        ++sizes[i];
        __goto_if_fail(res, error, "Failed to read varuint");
    }
    /* parse path */
    for (size_t i = 0; i < size; ++i) {
        res = tvg_parse_path(ctx, sizes[i]);
        __goto_if_fail(res, error, "Failed to parse path");
    }
    _stroke_path_with_style(ctx, line_style, D(line_width));
    twin_path_empty(path);
error:
    free(sizes);
    return res;
}

static tvg_result_t tvg_parse_line_fill_paths(tvg_context_t *ctx,
                                              size_t size,
                                              const tvg_style_t *fill_style,
                                              const tvg_style_t *line_style,
                                              float line_width)
{
    tvg_result_t res = TVG_SUCCESS;
    uint32_t *sizes = malloc(size * sizeof(uint32_t));
    if (!sizes) {
        return TVG_E_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < size; ++i) {
        res = tvg_read_varuint(ctx, &sizes[i]);
        ++sizes[i];
        __goto_if_fail(res, error, "Failed to read varuint");
    }
    twin_path_t *path = ctx->path;

    /* parse path */
    for (size_t i = 0; i < size; ++i) {
        res = tvg_parse_path(ctx, sizes[i]);
        __goto_if_fail(res, error, "Failed to parse path");
    }
    if (line_width == 0) {
        line_width = .1;
    }
    _fill_path_with_style(ctx, fill_style);
    _stroke_path_with_style(ctx, line_style, D(line_width));
    twin_path_empty(path);
error:
    free(sizes);
    return res;
}

static tvg_result_t tvg_parse_fill_polygon(tvg_context_t *ctx,
                                           size_t size,
                                           const tvg_style_t *fill_style)
{
    size_t count = size;
    tvg_point_t pt;
    tvg_result_t res = tvg_read_point(ctx, &pt);
    __return_val_if_fail(res, "Failed to read point");
    twin_path_t *path = ctx->path;
    twin_path_move(path, D(pt.x), D(pt.y));
    while (--count) {
        res = tvg_read_point(ctx, &pt);
        __return_val_if_fail(res, "Failed to read point");
        twin_path_draw(path, D(pt.x), D(pt.y));
    }
    twin_path_close(path);
    _fill_path_with_style(ctx, fill_style);
    twin_path_empty(path);
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_polyline(tvg_context_t *ctx,
                                       size_t size,
                                       const tvg_style_t *line_style,
                                       float line_width,
                                       bool close)
{
    tvg_point_t pt;
    tvg_result_t res = tvg_read_point(ctx, &pt);
    __return_val_if_fail(res, "Failed to read point");
    twin_path_t *path = ctx->path;
    twin_path_move(path, D(pt.x), D(pt.y));
    for (size_t i = 1; i < size; ++i) {
        res = tvg_read_point(ctx, &pt);
        __return_val_if_fail(res, "Failed to read point");
        twin_path_draw(path, D(pt.x), D(pt.y));
    }
    if (close) {
        twin_path_close(path);
    }
    if (line_width == 0) {
        line_width = .01;
    }
    _stroke_path_with_style(ctx, line_style, D(line_width));
    twin_path_empty(path);
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_line_fill_polyline(tvg_context_t *ctx,
                                                 size_t size,
                                                 const tvg_style_t *fill_style,
                                                 const tvg_style_t *line_style,
                                                 float line_width,
                                                 bool close)
{
    tvg_point_t pt;
    tvg_result_t res = tvg_read_point(ctx, &pt);
    twin_path_t *path = ctx->path;
    twin_path_move(path, D(pt.x), D(pt.y));
    for (size_t i = 1; i < size; ++i) {
        res = tvg_read_point(ctx, &pt);
        __return_val_if_fail(res, "Failed to read point");
    }
    if (close) {
        twin_path_close(path);
    }
    _fill_path_with_style(ctx, fill_style);
    if (line_width == 0) {
        line_width = .01;
    }
    _stroke_path_with_style(ctx, line_style, D(line_width));
    twin_path_empty(path);
    return res;
}

static tvg_result_t tvg_parse_lines(tvg_context_t *ctx,
                                    size_t size,
                                    const tvg_style_t *line_style,
                                    float line_width)
{
    tvg_point_t pt;
    tvg_result_t res;
    twin_path_t *path = ctx->path;
    for (size_t i = 0; i < size; ++i) {
        res = tvg_read_point(ctx, &pt);
        __return_val_if_fail(res, "Failed to read point");
        twin_path_move(path, D(pt.x), D(pt.y));
        res = tvg_read_point(ctx, &pt);
        __return_val_if_fail(res, "Failed to read point");
        twin_path_draw(path, D(pt.x), D(pt.y));
    }
    if (line_width == 0) {
        line_width = .01;
    }
    _stroke_path_with_style(ctx, line_style, D(line_width));
    twin_path_empty(path);
    return TVG_SUCCESS;
}

static tvg_result_t tvg_parse_commands(tvg_context_t *ctx)
{
    tvg_result_t res = TVG_SUCCESS;
    uint8_t cmd = 255;
    size_t read = 0;
    while (cmd != 0) {
        READ_VALUE(read, &cmd, 1);
        switch (TVG_CMD_INDEX(cmd)) {
        case TVG_CMD_END_DOCUMENT:
            break;
        case TVG_CMD_FILL_POLYGON: {
            tvg_fill_header_t data;
            res = tvg_parse_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse fill header");
            res = tvg_parse_fill_polygon(ctx, data.size, &data.style);
            __return_val_if_fail(res, "Failed to parse polygon");
        } break;
        case TVG_CMD_FILL_RECTANGLES: {
            tvg_fill_header_t data;
            res = tvg_parse_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse fill header");
            res = tvg_parse_fill_rectangles(ctx, data.size, &data.style);
            __return_val_if_fail(res, "Failed to parse rectangles");
        } break;
        case TVG_CMD_FILL_PATH: {
            tvg_fill_header_t data;
            res = tvg_parse_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse fill header");
            res = tvg_parse_fill_paths(ctx, data.size, &data.style);
            __return_val_if_fail(res, "Failed to parse paths");
        } break;
        case TVG_CMD_DRAW_LINES: {
            tvg_line_header_t data;
            res = tvg_parse_line_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line header");
            res = tvg_parse_lines(ctx, data.size, &data.style, data.line_width);
            __return_val_if_fail(res, "Failed to parse lines");
        } break;
        case TVG_CMD_DRAW_LINE_LOOP: {
            tvg_line_header_t data;
            res = tvg_parse_line_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line header");
            res = tvg_parse_polyline(ctx, data.size, &data.style,
                                     data.line_width, true);
            __return_val_if_fail(res, "Failed to parse polylines");
        } break;
        case TVG_CMD_DRAW_LINE_STRIP: {
            tvg_line_header_t data;
            res = tvg_parse_line_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line header");
            res = tvg_parse_polyline(ctx, data.size, &data.style,
                                     data.line_width, false);
            __return_val_if_fail(res, "Failed to parse polylines");
        } break;
        case TVG_CMD_DRAW_LINE_PATH: {
            tvg_line_header_t data;
            res = tvg_parse_line_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line header");
            res = tvg_parse_line_paths(ctx, data.size, &data.style,
                                       data.line_width);
            __return_val_if_fail(res, "Failed to parse paths");
        } break;
        case TVG_CMD_OUTLINE_FILL_POLYGON: {
            tvg_line_fill_header_t data;
            res =
                tvg_parse_line_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line and fill header");
            res = tvg_parse_line_fill_polyline(ctx, data.size, &data.fill_style,
                                               &data.line_style,
                                               data.line_width, true);
            __return_val_if_fail(res, "Failed to parse polyline");
        } break;
        case TVG_CMD_OUTLINE_FILL_RECTANGLES: {
            tvg_line_fill_header_t data;
            res =
                tvg_parse_line_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line and fill header");
            res = tvg_parse_line_fill_rectangles(
                ctx, data.size, &data.fill_style, &data.line_style,
                data.line_width);
            __return_val_if_fail(res, "Failed to parse rectangles");
        } break;
        case TVG_CMD_OUTLINE_FILL_PATH: {
            tvg_line_fill_header_t data;
            res =
                tvg_parse_line_fill_header(ctx, TVG_CMD_STYLE_KIND(cmd), &data);
            __return_val_if_fail(res, "Failed to parse line and fill header");
            res = tvg_parse_line_fill_paths(ctx, data.size, &data.fill_style,
                                            &data.line_style, data.line_width);
            __return_val_if_fail(res, "Failed to parse paths");
        } break;
        default:
            return TVG_E_INVALID_FORMAT;
        }
    }
    return TVG_SUCCESS;
}

static tvg_result_t tvg_document_dimensions(tvg_input_func_t in,
                                            void *in_state,
                                            uint32_t *out_width,
                                            uint32_t *out_height)
{
    /* initialize the context */
    tvg_context_t ctx;
    if (!in) {
        return TVG_E_INVALID_ARG;
    }
    ctx.in = in;
    ctx.in_state = in_state;
    ctx.colors = NULL;
    ctx.colors_size = 0;
    /* parse the header, early outing before the color table */
    tvg_result_t res = tvg_parse_header(&ctx, 1);
    __return_val_if_fail(res, "Failed to parse header");
    *out_width = ctx.width;
    *out_height = ctx.height;
    return res;
}

static tvg_result_t tvg_render_document(tvg_input_func_t in,
                                        void *in_state,
                                        twin_pixmap_t *pix,
                                        twin_fixed_t width_scale,
                                        twin_fixed_t height_scale)
{
    /* initialize the context */
    tvg_context_t ctx;
    if (!in) {
        return TVG_E_INVALID_ARG;
    }
    ctx.in = in;
    ctx.in_state = in_state;
    ctx.pixmap = pix;
    ctx.colors = NULL;
    ctx.colors_size = 0;
    /* parse the header */
    tvg_result_t res = tvg_parse_header(&ctx, 0);
    __goto_if_fail(res, error, "Failed to parse header");
    ctx.path = twin_path_create();
    if (!ctx.path) {
        log_error("Failed to create path");
        goto error;
    }
    twin_path_scale(ctx.path, width_scale, height_scale);
    res = tvg_parse_commands(&ctx);
    __goto_if_fail(res, error, "Failed to parse commands");
error:
    if (ctx.path) {
        twin_path_destroy(ctx.path);
        ctx.path = NULL;
    }
    if (ctx.colors) {
        free(ctx.colors);
        ctx.colors = NULL;
        ctx.colors_size = 0;
    }
    return res;
}

static size_t inp_func(uint8_t *data, size_t to_read, void *state)
{
    FILE *f = (FILE *) state;
    return fread(data, 1, to_read, f);
}

twin_pixmap_t *_twin_tvg_to_pixmap(const char *filepath, twin_format_t fmt)
{
    FILE *infile = NULL;
    twin_pixmap_t *pix = NULL;
    uint32_t width, height;
    tvg_result_t res;

    /* Current implementation only produces TWIN_ARGB32 */
    if (fmt != TWIN_ARGB32) {
        log_error("Unsupported color format");
        goto bail;
    }

    if (!filepath) {
        log_error("Invalid filepath");
        goto bail;
    }

    infile = fopen(filepath, "rb");
    if (!infile) {
        log_error("Failed to open %s", filepath);
        goto bail;
    }

    res = tvg_document_dimensions(inp_func, infile, &width, &height);
    if (res != TVG_SUCCESS) {
        log_error("Failed to get document dimensions");
        goto bail_infile;
    }

    if (fseek(infile, 0, SEEK_SET) != 0) {
        log_error("Failed to seek file");
        goto bail_infile;
    }

    pix = twin_pixmap_create(fmt, width, height);
    if (!pix) {
        log_error("Failed to create pixmap");
        goto bail_infile;
    }

    res = tvg_render_document(inp_func, infile, pix, TWIN_FIXED_ONE,
                              TWIN_FIXED_ONE);
    if (res != TVG_SUCCESS) {
        log_error("Failed to render document");
        goto bail_pixmap;
    }

    fclose(infile);
    return pix;

bail_pixmap:
    twin_pixmap_destroy(pix);
bail_infile:
    fclose(infile);
bail:
    return NULL;
}

twin_pixmap_t *twin_tvg_to_pixmap_scale(const char *filepath,
                                        twin_format_t fmt,
                                        twin_coord_t w,
                                        twin_coord_t h)
{
    FILE *infile = NULL;
    twin_pixmap_t *pix = NULL;
    uint32_t width, height;
    tvg_result_t res;

    /* Current implementation only produces TWIN_ARGB32 */
    if (fmt != TWIN_ARGB32) {
        log_error("Unsupported color format");
        goto bail;
    }

    if (!filepath) {
        log_error("Invalid filepath");
        goto bail;
    }

    infile = fopen(filepath, "rb");
    if (!infile) {
        log_error("Failed to open %s", filepath);
        goto bail;
    }

    res = tvg_document_dimensions(inp_func, infile, &width, &height);
    __goto_if_fail(res, bail_infile, "Failed to get document dimensions");

    if (fseek(infile, 0, SEEK_SET) != 0) {
        log_error("Failed to seek file");
        goto bail_infile;
    }

    pix = twin_pixmap_create(fmt, w, h);
    if (!pix) {
        log_error("Failed to create pixmap");
        goto bail_infile;
    }
    twin_fixed_t width_scale = D((double) w / (double) width);
    twin_fixed_t height_scale = D((double) h / (double) height);
    twin_fixed_t scale = MIN(width_scale, height_scale);
    res = tvg_render_document(inp_func, infile, pix, scale, scale);
    __goto_if_fail(res, bail_pixmap, "Failed to render document");

    fclose(infile);
    return pix;

bail_pixmap:
    twin_pixmap_destroy(pix);
bail_infile:
    fclose(infile);
bail:
    return NULL;
}
