/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_PRIVATE_H_
#define _TWIN_PRIVATE_H_

#include <stdio.h>
#include <string.h>

#include "twin.h"

/* FIXME: Both twin_private.h and log.h are private header files. They should
 * be moved to src/ directory.
 */
#include "../src/log.h"

/* Boilerplate for compiler compatibility */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

/* since C23 */
#ifndef __has_c_attribute
#define __has_c_attribute(x) 0
#endif

#if defined(__clang__) || defined(__GNUC__)
#define maybe_unused __attribute__((unused))
#else
#define maybe_unused
#endif

/* suppress warnings when intentional switch case fall-through is desired. */
#if defined(__has_c_attribute) && __has_c_attribute(fallthrough) /* C23+ */
#define fallthrough [[fallthrough]]
#elif defined(__has_attribute) && __has_attribute(fallthrough)
/* GNU-style attribute. The __has_attribute() macro was first available in Clang
 * 2.9 and incorporated into GCC since GCC 9.
 */
#define fallthrough __attribute__((fallthrough))
#elif defined(__GNUC__) && __GNUC__ >= 7
#define fallthrough __attribute__((__fallthrough__))
#else
#define fallthrough ((void) 0)
#endif

/*
 * Fixed-point type definitions
 *
 * "Qm.f" is a representation of a fixed-point type, where "m" bits are used
 * for the integer part, "f" bits are used for the fractional part, and 1 bit
 * is used for the sign. The total number of used bits is 1 + m + f.
 *
 * twin_sfixed_t - A fixed-point type in the Q11.4 format.
 *
 *        Hex                 Binary   Decimal       Actual
 * Max 0x7fff    0111 1111 1111 1111     32767    2047.9375
 * Min 0x8000    1000 0000 0000 0000    -32768         2048
 *
 * twin_dfixed_t - A fixed-point type in the Q23.8 format.
 *
 *            Hex                                     Binary
 * Max 0x7fffffff    0111 1111 1111 1111 1111 1111 1111 1111
 * Min 0x80000000    1000 0000 0000 0000 0000 0000 0000 0000
 *         Decimal           Actual
 * Max  2147483647    8388607.99609
 * Min -2147483648         -8388608
 *
 * twin_gfixed_t - A fixed-point type in the Q1.6 format, used in Glyph
 *                 coordinates.
 *
 *      Hex       Binary   Decimal      Actual
 * Max 0x7f    0111 1111       127    1.984375
 * Min 0x80    1000 0000      -128          -2
 *
 * All of the above tables are based on two's complement representation.
 */
typedef int16_t twin_sfixed_t;
typedef int32_t twin_dfixed_t;
typedef int8_t twin_gfixed_t;

#define twin_sfixed_floor(f) ((f) & ~0xf)
#define twin_sfixed_trunc(f) ((f) >> 4)
#define twin_sfixed_ceil(f) (((f) + 0xf) & ~0xf)
#define twin_sfixed_mod(f) ((f) & 0xf)

#define twin_int_to_sfixed(i) ((twin_sfixed_t) ((i) * 16))

#define twin_sfixed_to_fixed(s) (((twin_fixed_t) (s)) << 12)
#define twin_fixed_to_sfixed(f) ((twin_sfixed_t) ((f) >> 12))

#define twin_sfixed_to_dfixed(s) (((twin_dfixed_t) (s)) << 4)
#define twin_dfixed_to_sfixed(d) ((twin_sfixed_t) ((d) >> 4))

/*
 * twin_sfixed_t a = b'10100;
 * twin_sfixed_t b = b'10000;
 * exact a is 1*(2^0) + 1*(2^(-2)) = 1.25
 * exact b is 1*(2^0) = 1
 *
 * The result of twin_sfixed_div(a, b) is of type twin_sfixed_t
 * (a << 4) / (b) = (b'101000000) / (b'10000) = b'10100
 * exact result is 1*(2^0) + 1*(2^(-2)) = 1.25
 *
 * twin_dfixed_t a = b'10100;
 * twin_dfixed_t b = b'10000;
 * exact a is 1*(2^(-4)) + 1*(2^(-6)) = 0.078125
 * exact b is 1*(2^(-4)) = 0.0625
 *
 * The result of twin_dfixed_mul(a, b) is of type twin_dfixed_t
 * (a * b) >> 8 = (b'101000000) >> 8 = b'1
 * exact result is 1*(2^(-8)) = 0.00390625
 */

#define twin_sfixed_mul(a, b) \
    ((((twin_sfixed_t) (a)) * ((twin_sfixed_t) (b))) >> 4)
#define twin_sfixed_div(a, b) \
    ((((twin_sfixed_t) (a)) << 4) / ((twin_sfixed_t) (b)))

#define twin_dfixed_mul(a, b) \
    ((((twin_dfixed_t) (a)) * ((twin_dfixed_t) (b))) >> 8)
#define twin_dfixed_div(a, b) \
    ((((twin_dfixed_t) (a)) << 8) / ((twin_dfixed_t) (b)))

/*
 * 'double' is a no-no in any shipping code, but useful during
 * development
 */
#define twin_double_to_sfixed(d) ((twin_sfixed_t) ((d) * 16.0))
#define twin_sfixed_to_double(f) ((double) (f) / 16.0)

#define TWIN_SFIXED_ONE (0x10)
#define TWIN_SFIXED_HALF (0x08)
#define TWIN_SFIXED_TOLERANCE (TWIN_SFIXED_ONE >> 2)
#define TWIN_SFIXED_MIN (-0x7fff)
#define TWIN_SFIXED_MAX (0x7fff)


#define TWIN_GFIXED_ONE (0x40)

/*
 * Compositing stuff
 */
#define twin_int_mult(a, b, t) \
    ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))
#define twin_int_div(a, b) (((uint16_t) (a) * 255) / (b))
#define twin_get_8(v, i) ((uint16_t) (uint8_t) ((v) >> (i)))
#define twin_sat(t) ((uint8_t) ((t) | (0 - ((t) >> 8))))

#define twin_in(s, i, m, t) \
    ((twin_argb32_t) twin_int_mult(twin_get_8(s, i), (m), (t)) << (i))

#define twin_over(s, d, i, m, t)                                           \
    (((t) = twin_int_mult(twin_get_8(d, i), (m), (t)) + twin_get_8(s, i)), \
     (twin_argb32_t) twin_sat(t) << (i))

#define twin_add(s, d, i, t)                                                  \
    (((t) = twin_get_8(d, i) + twin_get_8(s, i)), (twin_argb32_t) twin_sat(t) \
                                                      << (i))

#define twin_argb32_to_rgb16(s) \
    ((((s) >> 3) & 0x001f) | (((s) >> 5) & 0x07e0) | (((s) >> 8) & 0xf800))
#define twin_rgb16_to_argb32(s)                       \
    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) |     \
     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)) | 0xff000000)

typedef union {
    twin_pointer_t p;
    twin_argb32_t c;
} twin_source_u;

typedef void (*twin_src_msk_op)(twin_pointer_t dst,
                                twin_source_u src,
                                twin_source_u msk,
                                int width);

typedef void (*twin_src_op)(twin_pointer_t dst, twin_source_u src, int width);

typedef struct _twin_xform {
    twin_pixmap_t *pixmap;
    twin_pointer_t span;
    twin_coord_t left;
    twin_coord_t width;
    twin_coord_t src_x;
    twin_coord_t src_y;
} twin_xform_t;

/* twin_primitive.c */

typedef void twin_in_op_func(twin_pointer_t dst,
                             twin_source_u src,
                             twin_source_u msk,
                             int width);

typedef void twin_op_func(twin_pointer_t dst, twin_source_u src, int width);

/* Geometrical objects */
typedef struct _twin_spoint {
    twin_sfixed_t x, y;
} twin_spoint_t;

struct _twin_path {
    twin_spoint_t *points;
    int size_points;
    int npoints;
    int *sublen;
    int size_sublen;
    int nsublen;
    twin_state_t state;
};

typedef struct _twin_gpoint {
    twin_gfixed_t x, y;
} twin_gpoint_t;

/*
 * FIXME: should be refactored to reduce the number of functions.
 */
twin_in_op_func _twin_argb32_in_argb32_over_argb32;
twin_in_op_func _twin_argb32_in_rgb16_over_argb32;
twin_in_op_func _twin_argb32_in_a8_over_argb32;
twin_in_op_func _twin_argb32_in_c_over_argb32;
twin_in_op_func _twin_rgb16_in_argb32_over_argb32;
twin_in_op_func _twin_rgb16_in_rgb16_over_argb32;
twin_in_op_func _twin_rgb16_in_a8_over_argb32;
twin_in_op_func _twin_rgb16_in_c_over_argb32;
twin_in_op_func _twin_a8_in_argb32_over_argb32;
twin_in_op_func _twin_a8_in_rgb16_over_argb32;
twin_in_op_func _twin_a8_in_a8_over_argb32;
twin_in_op_func _twin_a8_in_c_over_argb32;
twin_in_op_func _twin_c_in_argb32_over_argb32;
twin_in_op_func _twin_c_in_rgb16_over_argb32;
twin_in_op_func _twin_c_in_a8_over_argb32;
twin_in_op_func _twin_c_in_c_over_argb32;
twin_in_op_func _twin_argb32_in_argb32_over_rgb16;
twin_in_op_func _twin_argb32_in_rgb16_over_rgb16;
twin_in_op_func _twin_argb32_in_a8_over_rgb16;
twin_in_op_func _twin_argb32_in_c_over_rgb16;
twin_in_op_func _twin_rgb16_in_argb32_over_rgb16;
twin_in_op_func _twin_rgb16_in_rgb16_over_rgb16;
twin_in_op_func _twin_rgb16_in_a8_over_rgb16;
twin_in_op_func _twin_rgb16_in_c_over_rgb16;
twin_in_op_func _twin_a8_in_argb32_over_rgb16;
twin_in_op_func _twin_a8_in_rgb16_over_rgb16;
twin_in_op_func _twin_a8_in_a8_over_rgb16;
twin_in_op_func _twin_a8_in_c_over_rgb16;
twin_in_op_func _twin_c_in_argb32_over_rgb16;
twin_in_op_func _twin_c_in_rgb16_over_rgb16;
twin_in_op_func _twin_c_in_a8_over_rgb16;
twin_in_op_func _twin_c_in_c_over_rgb16;
twin_in_op_func _twin_argb32_in_argb32_over_a8;
twin_in_op_func _twin_argb32_in_rgb16_over_a8;
twin_in_op_func _twin_argb32_in_a8_over_a8;
twin_in_op_func _twin_argb32_in_c_over_a8;
twin_in_op_func _twin_rgb16_in_argb32_over_a8;
twin_in_op_func _twin_rgb16_in_rgb16_over_a8;
twin_in_op_func _twin_rgb16_in_a8_over_a8;
twin_in_op_func _twin_rgb16_in_c_over_a8;
twin_in_op_func _twin_a8_in_argb32_over_a8;
twin_in_op_func _twin_a8_in_rgb16_over_a8;
twin_in_op_func _twin_a8_in_a8_over_a8;
twin_in_op_func _twin_a8_in_c_over_a8;
twin_in_op_func _twin_c_in_argb32_over_a8;
twin_in_op_func _twin_c_in_rgb16_over_a8;
twin_in_op_func _twin_c_in_a8_over_a8;
twin_in_op_func _twin_c_in_c_over_a8;
twin_in_op_func _twin_argb32_in_argb32_over_c;

twin_in_op_func _twin_argb32_in_argb32_source_argb32;
twin_in_op_func _twin_argb32_in_rgb16_source_argb32;
twin_in_op_func _twin_argb32_in_a8_source_argb32;
twin_in_op_func _twin_argb32_in_c_source_argb32;
twin_in_op_func _twin_rgb16_in_argb32_source_argb32;
twin_in_op_func _twin_rgb16_in_rgb16_source_argb32;
twin_in_op_func _twin_rgb16_in_a8_source_argb32;
twin_in_op_func _twin_rgb16_in_c_source_argb32;
twin_in_op_func _twin_a8_in_argb32_source_argb32;
twin_in_op_func _twin_a8_in_rgb16_source_argb32;
twin_in_op_func _twin_a8_in_a8_source_argb32;
twin_in_op_func _twin_a8_in_c_source_argb32;
twin_in_op_func _twin_c_in_argb32_source_argb32;
twin_in_op_func _twin_c_in_rgb16_source_argb32;
twin_in_op_func _twin_c_in_a8_source_argb32;
twin_in_op_func _twin_c_in_c_source_argb32;
twin_in_op_func _twin_argb32_in_argb32_source_rgb16;
twin_in_op_func _twin_argb32_in_rgb16_source_rgb16;
twin_in_op_func _twin_argb32_in_a8_source_rgb16;
twin_in_op_func _twin_argb32_in_c_source_rgb16;
twin_in_op_func _twin_rgb16_in_argb32_source_rgb16;
twin_in_op_func _twin_rgb16_in_rgb16_source_rgb16;
twin_in_op_func _twin_rgb16_in_a8_source_rgb16;
twin_in_op_func _twin_rgb16_in_c_source_rgb16;
twin_in_op_func _twin_a8_in_argb32_source_rgb16;
twin_in_op_func _twin_a8_in_rgb16_source_rgb16;
twin_in_op_func _twin_a8_in_a8_source_rgb16;
twin_in_op_func _twin_a8_in_c_source_rgb16;
twin_in_op_func _twin_c_in_argb32_source_rgb16;
twin_in_op_func _twin_c_in_rgb16_source_rgb16;
twin_in_op_func _twin_c_in_a8_source_rgb16;
twin_in_op_func _twin_c_in_c_source_rgb16;
twin_in_op_func _twin_argb32_in_argb32_source_a8;
twin_in_op_func _twin_argb32_in_rgb16_source_a8;
twin_in_op_func _twin_argb32_in_a8_source_a8;
twin_in_op_func _twin_argb32_in_c_source_a8;
twin_in_op_func _twin_rgb16_in_argb32_source_a8;
twin_in_op_func _twin_rgb16_in_rgb16_source_a8;
twin_in_op_func _twin_rgb16_in_a8_source_a8;
twin_in_op_func _twin_rgb16_in_c_source_a8;
twin_in_op_func _twin_a8_in_argb32_source_a8;
twin_in_op_func _twin_a8_in_rgb16_source_a8;
twin_in_op_func _twin_a8_in_a8_source_a8;
twin_in_op_func _twin_a8_in_c_source_a8;
twin_in_op_func _twin_c_in_argb32_source_a8;
twin_in_op_func _twin_c_in_rgb16_source_a8;
twin_in_op_func _twin_c_in_a8_source_a8;
twin_in_op_func _twin_c_in_c_source_a8;
twin_in_op_func _twin_argb32_in_argb32_source_c;

twin_op_func _twin_argb32_over_argb32;
twin_op_func _twin_rgb16_over_argb32;
twin_op_func _twin_a8_over_argb32;
twin_op_func _twin_c_over_argb32;
twin_op_func _twin_argb32_over_rgb16;
twin_op_func _twin_rgb16_over_rgb16;
twin_op_func _twin_a8_over_rgb16;
twin_op_func _twin_c_over_rgb16;
twin_op_func _twin_argb32_over_a8;
twin_op_func _twin_rgb16_over_a8;
twin_op_func _twin_a8_over_a8;
twin_op_func _twin_c_over_a8;
twin_op_func _twin_argb32_source_argb32;
twin_op_func _twin_rgb16_source_argb32;
twin_op_func _twin_a8_source_argb32;
twin_op_func _twin_c_source_argb32;
twin_op_func _twin_argb32_source_rgb16;
twin_op_func _twin_rgb16_source_rgb16;
twin_op_func _twin_a8_source_rgb16;
twin_op_func _twin_c_source_rgb16;
twin_op_func _twin_argb32_source_a8;
twin_op_func _twin_rgb16_source_a8;
twin_op_func _twin_a8_source_a8;
twin_op_func _twin_c_source_a8;

twin_op_func _twin_vec_argb32_over_argb32;
twin_op_func _twin_vec_argb32_source_argb32;

twin_argb32_t *_twin_fetch_rgb16(twin_pixmap_t *pixmap,
                                 int x,
                                 int y,
                                 int w,
                                 twin_argb32_t *span);

twin_argb32_t *_twin_fetch_argb32(twin_pixmap_t *pixmap,
                                  int x,
                                  int y,
                                  int w,
                                  twin_argb32_t *span);

/*
 * Geometry helper functions
 */
twin_dfixed_t _twin_distance_to_line_squared(twin_spoint_t *p,
                                             twin_spoint_t *p1,
                                             twin_spoint_t *p2);

/*
 * Polygon stuff
 */

/*
 * Fixed point helper functions
 */
twin_sfixed_t _twin_sfixed_sqrt(twin_sfixed_t as);

/*
 * Matrix stuff
 */
twin_sfixed_t _twin_matrix_x(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_sfixed_t _twin_matrix_y(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_fixed_t _twin_matrix_fx(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_fixed_t _twin_matrix_fy(twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y);

twin_sfixed_t _twin_matrix_dx(twin_matrix_t *m,
                              twin_fixed_t dx,
                              twin_fixed_t dy);

twin_sfixed_t _twin_matrix_dy(twin_matrix_t *m,
                              twin_fixed_t dx,
                              twin_fixed_t dy);

twin_fixed_t _twin_matrix_determinant(twin_matrix_t *matrix);

twin_sfixed_t _twin_matrix_len(twin_matrix_t *m,
                               twin_fixed_t dx,
                               twin_fixed_t dy);

twin_point_t _twin_matrix_expand(twin_matrix_t *matrix);

/*
 * Path stuff
 */

/*
 * A path
 */
twin_spoint_t _twin_path_current_spoint(twin_path_t *path);

twin_spoint_t _twin_path_subpath_first_spoint(twin_path_t *path);

void _twin_path_smove(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y);

void _twin_path_sdraw(twin_path_t *path, twin_sfixed_t x, twin_sfixed_t y);

void _twin_path_scurve(twin_path_t *path,
                       twin_sfixed_t x1,
                       twin_sfixed_t y1,
                       twin_sfixed_t x2,
                       twin_sfixed_t y2,
                       twin_sfixed_t x3,
                       twin_sfixed_t y3);

void _twin_path_sfinish(twin_path_t *path);

/*
 * Glyph stuff.  Coordinates are stored in 2.6 fixed point format
 */

/*
 * Check these whenever glyphs are changed
 */
#define TWIN_GLYPH_MAX_SNAP_X 4
#define TWIN_GLYPH_MAX_SNAP_Y 7

#define twin_glyph_left(g) ((g)[0])
#define twin_glyph_right(g) ((g)[1])
#define twin_glyph_ascent(g) ((g)[2])
#define twin_glyph_descent(g) ((g)[3])
#define twin_glyph_n_snap_x(g) ((g)[4])
#define twin_glyph_n_snap_y(g) ((g)[5])
#define twin_glyph_snap_x(g) (&g[6])
#define twin_glyph_snap_y(g) (twin_glyph_snap_x(g) + twin_glyph_n_snap_x(g))

/*
 * dispatch stuff
 */
typedef struct _twin_queue {
    struct _twin_queue *next;
    struct _twin_queue *order;
    bool walking;
    bool deleted;
} twin_queue_t;

struct _twin_timeout {
    twin_queue_t queue;
    twin_time_t time;
    twin_time_t delay;
    twin_timeout_proc_t proc;
    void *closure;
};

struct _twin_work {
    twin_queue_t queue;
    int priority;
    twin_work_proc_t proc;
    void *closure;
};

struct _twin_file {
    twin_queue_t queue;
    int file;
    twin_file_op_t ops;
    twin_file_proc_t proc;
    void *closure;
};

typedef enum _twin_order {
    TWIN_BEFORE = -1,
    TWIN_AT = 0,
    TWIN_AFTER = 1
} twin_order_t;

typedef twin_order_t (*twin_queue_proc_t)(twin_queue_t *a, twin_queue_t *b);

void _twin_queue_insert(twin_queue_t **head,
                        twin_queue_proc_t proc,
                        twin_queue_t *new);

void _twin_queue_remove(twin_queue_t **head, twin_queue_t *old);

void _twin_queue_reorder(twin_queue_t **head,
                         twin_queue_proc_t proc,
                         twin_queue_t *elem);

void _twin_queue_delete(twin_queue_t **head, twin_queue_t *old);

twin_queue_t *_twin_queue_set_order(twin_queue_t **head);

void _twin_queue_review_order(twin_queue_t *first);

int _twin_run_file(twin_time_t delay);

void _twin_run_timeout(void);

twin_time_t _twin_timeout_delay(void);

void _twin_run_work(void);

void _twin_box_init(twin_box_t *box,
                    twin_box_t *parent,
                    twin_window_t *window,
                    twin_box_dir_t dir,
                    twin_dispatch_proc_t dispatch);

twin_dispatch_result_t _twin_box_dispatch(twin_widget_t *widget,
                                          twin_event_t *event);

void _twin_widget_init(twin_widget_t *widget,
                       twin_box_t *parent,
                       twin_window_t *window,
                       twin_widget_layout_t preferred,
                       twin_dispatch_proc_t dispatch);

void _twin_widget_paint_shape(twin_widget_t *widget,
                              twin_shape_t shape,
                              twin_coord_t left,
                              twin_coord_t top,
                              twin_coord_t right,
                              twin_coord_t bottom,
                              twin_fixed_t radius);

twin_dispatch_result_t _twin_widget_dispatch(twin_widget_t *widget,
                                             twin_event_t *event);

void _twin_widget_queue_paint(twin_widget_t *widget);

void _twin_widget_queue_layout(twin_widget_t *widget);

bool _twin_widget_contains(twin_widget_t *widget,
                           twin_coord_t x,
                           twin_coord_t y);

void _twin_widget_bevel(twin_widget_t *widget,
                        twin_fixed_t bevel_width,
                        bool down);

void _twin_label_init(twin_label_t *label,
                      twin_box_t *parent,
                      const char *value,
                      twin_argb32_t foreground,
                      twin_fixed_t font_size,
                      twin_style_t font_style,
                      twin_dispatch_proc_t dispatch);

twin_dispatch_result_t _twin_label_dispatch(twin_widget_t *widget,
                                            twin_event_t *event);

twin_dispatch_result_t _twin_toplevel_dispatch(twin_widget_t *widget,
                                               twin_event_t *event);

void _twin_toplevel_init(twin_toplevel_t *toplevel,
                         twin_dispatch_proc_t dispatch,
                         twin_window_t *window,
                         const char *name);

void _twin_toplevel_queue_paint(twin_widget_t *widget);

void _twin_toplevel_queue_layout(twin_widget_t *widget);

twin_dispatch_result_t _twin_button_dispatch(twin_widget_t *widget,
                                             twin_event_t *event);

void _twin_button_init(twin_button_t *button,
                       twin_box_t *parent,
                       const char *value,
                       twin_argb32_t foreground,
                       twin_fixed_t font_size,
                       twin_style_t font_style,
                       twin_dispatch_proc_t dispatch);

/* utility */

#define min(x, y)            \
    ({                       \
        typeof(x) _x = (x);  \
        typeof(y) _y = (y);  \
        (void) (&_x == &_y); \
        _x < _y ? _x : _y;   \
    })

#define max(x, y)            \
    ({                       \
        typeof(x) _x = (x);  \
        typeof(y) _y = (y);  \
        (void) (&_x == &_y); \
        _x > _y ? _x : _y;   \
    })
#ifdef _MSC_VER
#include <intrin.h>
static inline int twin_clz(uint32_t v)
{
    uint32_t leading_zero = 0;
    if (_BitScanReverse(&leading_zero, v))
        return 31 - leading_zero;
    return 32; /* undefined behavior */
}
#elif defined(__GNUC__) || defined(__clang__)
static inline int twin_clz(uint32_t v)
{
    return __builtin_clz(v);
}
#else /* generic implementation */
static inline int twin_clz(uint32_t v)
{
    /* http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn */
    static const uint8_t mul_debruijn[] = {
        0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31,
    };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return mul_debruijn[(uint32_t) (v * 0x07C4ACDDU) >> 27];
}
#endif

/* Pattern Matching for C macros.
 * https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
 */

/* In Visual Studio, __VA_ARGS__ is treated as a separate parameter. */
#define FIX_VC_BUG(x) x

/* catenate */
#define PRIMITIVE_CAT(a, ...) FIX_VC_BUG(a##__VA_ARGS__)

#define IIF(c) PRIMITIVE_CAT(IIF_, c)
/* run the 2nd parameter */
#define IIF_0(t, ...) __VA_ARGS__
/* run the 1st parameter */
#define IIF_1(t, ...) t

#endif /* _TWIN_PRIVATE_H_ */
