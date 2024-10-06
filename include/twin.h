/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_H_
#define _TWIN_H_

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t twin_a8_t;
typedef uint16_t twin_a16_t;
typedef uint16_t twin_rgb16_t;
typedef uint32_t twin_argb32_t;
typedef uint32_t twin_ucs4_t;
typedef int16_t twin_coord_t;
typedef int16_t twin_count_t;
typedef int16_t twin_keysym_t;
typedef uint8_t twin_js_number_t;
typedef int16_t twin_js_value_t;
typedef int32_t twin_area_t;
typedef int32_t twin_time_t;
typedef int16_t twin_stretch_t;
typedef int32_t twin_fixed_t; /* 16.16 format */

typedef enum { TWIN_A8, TWIN_RGB16, TWIN_ARGB32 } twin_format_t;

#define twin_bytes_per_pixel(format) (1 << (twin_coord_t) (format))

/*
 * Angles
 */
typedef int16_t twin_angle_t; /* -2048 .. 2048 for -180 .. 180 */

#define TWIN_ANGLE_360 4096
#define TWIN_ANGLE_180 (TWIN_ANGLE_360 >> 1)
#define TWIN_ANGLE_90 (TWIN_ANGLE_360 >> 2)
#define TWIN_ANGLE_45 (TWIN_ANGLE_360 >> 3)
#define TWIN_ANGLE_22_5 (TWIN_ANGLE_360 >> 4)
#define TWIN_ANGLE_11_25 (TWIN_ANGLE_360 >> 5)

#define TWIN_ANGLE_270 (TWIN_ANGLE_180 + TWIN_ANGLE_90)
#define TWIN_ANGLE_0 (0)

#define twin_degrees_to_angle(d) \
    ((twin_angle_t) ((((int32_t) (d)) * TWIN_ANGLE_360 / 360)))

/*
 * A rectangle
 */
typedef struct _twin_rect {
    twin_coord_t left, right, top, bottom;
} twin_rect_t;

/*
 * Place matrices in structures so they can be easily copied
 */
typedef struct _twin_matrix {
    twin_fixed_t m[3][2];
} twin_matrix_t;

typedef union _twin_pointer {
    void *v;
    uint8_t *b;
    twin_a8_t *a8;
    twin_rgb16_t *rgb16;
    twin_argb32_t *argb32;
} twin_pointer_t;

typedef struct _twin_window twin_window_t;
typedef struct _twin_screen twin_screen_t;
typedef struct _twin_pixmap twin_pixmap_t;
typedef struct _twin_animation twin_animation_t;

/*
 * Events
 */
typedef enum _twin_event_kind {
    /* Mouse */
    TwinEventButtonDown = 0x0001,
    TwinEventButtonUp = 0x0002,
    TwinEventMotion = 0x0003,
    TwinEventEnter = 0x0004,
    TwinEventLeave = 0x0005,

    /* keyboard */
    TwinEventKeyDown = 0x0101,
    TwinEventKeyUp = 0x0102,
    TwinEventUcs4 = 0x0103,

    /* Focus */
    TwinEventActivate = 0x0201,
    TwinEventDeactivate = 0x0202,

    /* Joystick */
    TwinEventJoyButton = 0x0401,
    TwinEventJoyAxis = 0x0402,

    /* Widgets */
    TwinEventPaint = 0x1001,
    TwinEventQueryGeometry = 0x1002,
    TwinEventConfigure = 0x1003,
    TwinEventDestroy = 0x1004,
} twin_event_kind_t;

typedef struct _twin_event {
    twin_event_kind_t kind;
    union {
        struct {
            twin_coord_t x, y;
            twin_coord_t screen_x, screen_y;
            twin_count_t button;
        } pointer;
        struct {
            twin_keysym_t key;
        } key;
        struct {
            twin_js_number_t control;
            twin_js_value_t value;
        } js;
        struct {
            twin_ucs4_t ucs4;
        } ucs4;
        struct {
            twin_rect_t extents;
        } configure;
    } u;
} twin_event_t;

typedef struct _twin_event_queue {
    struct _twin_event_queue *next;
    twin_event_t event;
} twin_event_queue_t;

typedef struct _twin_animation_iter {
    twin_animation_t *anim;
    twin_count_t current_index;
    twin_pixmap_t *current_frame;
    twin_time_t current_delay;
} twin_animation_iter_t;

typedef struct _twin_animation {
    /* Array of pixmaps representing each frame of the animation */
    twin_pixmap_t **frames;
    /* Number of frames in the animation */
    twin_count_t n_frames;
    /* Delay between frames in milliseconds */
    twin_time_t *frame_delays;
    /* Whether the animation should loop */
    bool loop;
    twin_animation_iter_t *iter;
    twin_coord_t width;  /* pixels */
    twin_coord_t height; /* pixels */
} twin_animation_t;

/*
 * A rectangular array of pixels
 */
typedef struct _twin_pixmap {
    /*
     * Screen showing these pixels
     */
    struct _twin_screen *screen;
    twin_count_t disable;
    /*
     * List of displayed pixmaps
     */
    struct _twin_pixmap *down, *up;
    /*
     * Screen position
     */
    twin_coord_t x, y;
    /*
     * Pixmap layout
     */
    twin_format_t format;
    twin_coord_t width;  /* pixels */
    twin_coord_t height; /* pixels */
    twin_coord_t stride; /* bytes */
    twin_matrix_t transform;

    /*
     * Clipping - a single rectangle in pixmap coordinates.
     * Drawing is done clipped by this rectangle and relative
     * to origin_x, origin_y
     */
    twin_rect_t clip;
    twin_coord_t origin_x;
    twin_coord_t origin_y;

    /*
     * Pixels
     */
    twin_animation_t *animation;
    twin_pointer_t p;
    /*
     * When representing a window, this point
     * refers to the window object
     */
    twin_window_t *window;
} twin_pixmap_t;

/*
 * twin_put_begin_t: called before data are drawn to the screen
 * twin_put_span_t: called for each scanline drawn
 */
typedef void (*twin_put_begin_t)(twin_coord_t left,
                                 twin_coord_t top,
                                 twin_coord_t right,
                                 twin_coord_t bottom,
                                 void *closure);
typedef void (*twin_put_span_t)(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_argb32_t *pixels,
                                void *closure);

/*
 * A screen
 */
struct _twin_screen {
    /*
     * List of displayed pixmaps
     */
    twin_pixmap_t *top, *bottom;

    /*
     * One of them receives all key events
     */
    twin_pixmap_t *active;

    /*
     * this pixmap is target of mouse events
     */
    twin_pixmap_t *target;
    bool clicklock;

    /*
     * mouse image (optional)
     */
    twin_pixmap_t *cursor;
    twin_coord_t curs_hx;
    twin_coord_t curs_hy;
    twin_coord_t curs_x;
    twin_coord_t curs_y;

    /*
     * Output size
     */
    twin_coord_t width, height;

    /*
     * Background pattern
     */
    twin_pixmap_t *background;

    /*
     * Damage
     */
    twin_rect_t damage;
    void (*damaged)(void *);
    void *damaged_closure;
    twin_count_t disable;

    /*
     * Repaint function
     */
    twin_put_begin_t put_begin;
    twin_put_span_t put_span;
    void *closure;

    /*
     * Window manager stuff
     */
    twin_coord_t button_x, button_y;

    /*
     * Event filter
     */
    bool (*event_filter)(twin_screen_t *screen, twin_event_t *event);
};

/*
 * A source operand
 */
typedef enum { TWIN_SOLID, TWIN_PIXMAP } twin_source_t;

typedef struct _twin_operand {
    twin_source_t source_kind;
    union {
        twin_pixmap_t *pixmap;
        twin_argb32_t argb;
    } u;
} twin_operand_t;

typedef enum { TWIN_OVER, TWIN_SOURCE } twin_operator_t;

#define TWIN_FIXED_ONE (0x10000)
#define TWIN_FIXED_HALF (0x08000)
#define TWIN_FIXED_MAX (0x7fffffff)
#define TWIN_FIXED_MIN (-0x7fffffff)

/*
 * 'double' is a no-no in any shipping code, but useful during
 * development
 */
#define twin_double_to_fixed(d) ((twin_fixed_t) ((d) * 65536))
#define twin_fixed_to_double(f) ((double) (f) / 65536.0)

#define twin_int_to_fixed(i) ((twin_fixed_t) (i) << 16)
#define twin_fixed_ceil(f) (((f) + 0xffff) & ~0xffff)
#define twin_fixed_floor(f) ((f) & ~0xffff)
#define twin_fixed_to_int(f) ((int) ((f) >> 16))

typedef struct _twin_point {
    twin_fixed_t x, y;
} twin_point_t;

typedef struct _twin_path twin_path_t;

typedef enum _twin_style {
    TwinStyleRoman = 0,
    TwinStyleBold = 1,
    TwinStyleOblique = 2,
    TwinStyleBoldOblique = 3,
    TwinStyleUnhinted = 4,
} twin_style_t;

typedef enum _twin_cap {
    TwinCapRound,
    TwinCapButt,
    TwinCapProjecting,
} twin_cap_t;

typedef struct _twin_state {
    twin_matrix_t matrix;
    twin_fixed_t font_size;
    twin_style_t font_style;
    twin_cap_t cap_style;
} twin_state_t;

/*
 * Text metrics
 */
typedef struct _twin_text_metrics {
    twin_fixed_t left_side_bearing;
    twin_fixed_t right_side_bearing;
    twin_fixed_t ascent;
    twin_fixed_t descent;
    twin_fixed_t width;
    twin_fixed_t font_ascent;
    twin_fixed_t font_descent;
} twin_text_metrics_t;


/*
 * Fonts
 */
#define UCS_PAGE_SHIFT 7
#define UCS_PER_PAGE (1 << UCS_PAGE_SHIFT)

static inline int twin_ucs_page(uint32_t ucs4)
{
    return ucs4 >> UCS_PAGE_SHIFT;
}

static inline int twin_ucs_char_in_page(uint32_t ucs4)
{
    return ucs4 & (UCS_PER_PAGE - 1);
}

typedef struct _twin_charmap {
    unsigned int page;
    unsigned int offsets[UCS_PER_PAGE];
} twin_charmap_t;

#define TWIN_FONT_TYPE_STROKE 1
#define TWIN_FONT_TYPE_TTF 2

typedef struct _twin_font {
    /* those fields have to be initialized */
    int type;
    const char *name;
    const char *style;
    const twin_charmap_t *charmap;
    int n_charmap;
    const signed char *outlines;
    signed char ascender;
    signed char descender;
    signed char height;

    /* those are used at runtime for caching */
    const twin_charmap_t *cur_page;

} twin_font_t;

/* FIXME: one global font for now */
extern twin_font_t *g_twin_font;

/* Built-in default stroke font */
extern twin_font_t twin_Default_Font_Roman;

/*
 * Windows
 */
typedef enum _twin_window_style {
    TwinWindowPlain,
    TwinWindowApplication,
    TwinWindowFullScreen,
    TwinWindowDialog,
    TwinWindowAlert,
} twin_window_style_t;

typedef void (*twin_draw_func_t)(twin_window_t *window);

typedef bool (*twin_event_func_t)(twin_window_t *window, twin_event_t *event);

typedef void (*twin_destroy_func_t)(twin_window_t *window);

struct _twin_window {
    twin_screen_t *screen;
    twin_pixmap_t *pixmap;
    twin_window_style_t style;
    twin_rect_t client;
    twin_rect_t damage;
    bool client_grab;
    bool want_focus;
    bool draw_queued;
    void *client_data;
    char *name;

    twin_draw_func_t draw;
    twin_event_func_t event;
    twin_destroy_func_t destroy;
};

/*
 * Icons
 */
typedef enum _twin_icon {
    TwinIconMenu,
    TwinIconMinimize,
    TwinIconMaximize,
    TwinIconClose,
    TwinIconResize,
} twin_icon_t;

/*
 * Timeout and work procs return true to remain in the queue,
 * timeout procs are called every 'delay' ms
 */
typedef twin_time_t (*twin_timeout_proc_t)(twin_time_t now, void *closure);

typedef bool (*twin_work_proc_t)(void *closure);

typedef enum _twin_file_op { TWIN_READ = 1, TWIN_WRITE = 2 } twin_file_op_t;

typedef bool (*twin_file_proc_t)(int file, twin_file_op_t ops, void *closure);

#define twin_time_compare(a, op, b) (((a) - (b)) op 0)

typedef struct _twin_timeout twin_timeout_t;
typedef struct _twin_work twin_work_t;
typedef struct _twin_file twin_file_t;

/*
 * Widgets
 */
typedef struct _twin_widget twin_widget_t;
typedef struct _twin_box twin_box_t;

#define _twin_widget_width(w)                 \
    (((twin_widget_t *) (w))->extents.right - \
     ((twin_widget_t *) (w))->extents.left)

#define _twin_widget_height(w)                 \
    (((twin_widget_t *) (w))->extents.bottom - \
     ((twin_widget_t *) (w))->extents.top)

typedef enum _twin_box_dir { TwinBoxHorz, TwinBoxVert } twin_box_dir_t;

typedef enum _twin_dispatch_result {
    TwinDispatchDone,
    TwinDispatchContinue
} twin_dispatch_result_t;

typedef twin_dispatch_result_t (*twin_dispatch_proc_t)(twin_widget_t *widget,
                                                       twin_event_t *event);

typedef struct _twin_widget_layout {
    twin_coord_t width;
    twin_coord_t height;
    twin_stretch_t stretch_width;
    twin_stretch_t stretch_height;
} twin_widget_layout_t;

typedef enum _twin_shape {
    TwinShapeRectangle,
    TwinShapeRoundedRectangle,
    TwinShapeLozenge,
    TwinShapeTab,
    TwinShapeEllipse,
} twin_shape_t;

struct _twin_widget {
    twin_window_t *window;
    twin_widget_t *next;
    twin_box_t *parent;
    twin_dispatch_proc_t dispatch;
    twin_rect_t extents; /* current geometry */
    twin_widget_t *copy_geom;
    bool paint;
    bool layout;
    bool want_focus;
    twin_argb32_t background;
    twin_widget_layout_t preferred;
    twin_shape_t shape;
    twin_fixed_t radius;
};

struct _twin_box {
    twin_widget_t widget;
    twin_box_dir_t dir;
    twin_widget_t *children;
    twin_widget_t *button_down;
    twin_widget_t *focus;
};

typedef struct _twin_toplevel {
    twin_box_t box;
} twin_toplevel_t;

typedef enum _twin_align {
    TwinAlignLeft,
    TwinAlignCenter,
    TwinAlignRight
} twin_align_t;

typedef struct _twin_label {
    twin_widget_t widget;
    char *label;
    twin_argb32_t foreground;
    twin_fixed_t font_size;
    twin_style_t font_style;
    twin_point_t offset;
    twin_align_t align;
} twin_label_t;

typedef enum _twin_button_signal {
    TwinButtonSignalDown, /* sent when button pressed */
    TwinButtonSignalUp,   /* send when button released inside widget */
} twin_button_signal_t;

typedef struct _twin_button twin_button_t;

typedef void (*twin_button_signal_proc_t)(twin_button_t *button,
                                          twin_button_signal_t signal,
                                          void *closure);

struct _twin_button {
    twin_label_t label;
    bool pressed;
    bool active;
    twin_button_signal_proc_t signal;
    void *closure;
};

typedef enum _twin_scroll_signal {
    TwinScrollSignalUpArrow,
    TwinScrollSignalDownArrow,
    TwinScrollSignalThumb,
    TwinScrollSignalAboveThumb,
    TwinScrollSignalBelowThumb,
} twin_scroll_signal_t;

typedef struct _twin_scroll twin_scroll_t;

typedef void (*twin_scroll_signal_proc_t)(twin_scroll_t *scroll,
                                          twin_scroll_signal_t signal,
                                          void *closure);

struct _twin_scroll {
    twin_widget_t widget;
};

/*
 * box.c
 */

twin_box_t *twin_box_create(twin_box_t *parent, twin_box_dir_t dir);

/*
 * button.c
 */

twin_button_t *twin_button_create(twin_box_t *parent,
                                  const char *value,
                                  twin_argb32_t foreground,
                                  twin_fixed_t font_size,
                                  twin_style_t font_style);

/*
 * convolve.c
 */

void twin_path_convolve(twin_path_t *dest,
                        twin_path_t *stroke,
                        twin_path_t *pen);

/*
 * cursor.c
 */

twin_pixmap_t *twin_make_cursor(int *hx, int *hy);

/*
 * dispatch.c
 */

void twin_dispatch(void);

/*
 * draw.c
 */

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
                    twin_coord_t height);

void twin_fill(twin_pixmap_t *dst,
               twin_argb32_t pixel,
               twin_operator_t operator,
               twin_coord_t left,
               twin_coord_t top,
               twin_coord_t right,
               twin_coord_t bottom);

void twin_premultiply_alpha(twin_pixmap_t *px);

void twin_gaussian_blur(twin_pixmap_t *px);

/*
 * event.c
 */

void twin_event_enqueue(const twin_event_t *event);

/*
 * file.c
 */

twin_file_t *twin_set_file(twin_file_proc_t file_proc,
                           int file,
                           twin_file_op_t ops,
                           void *closure);

void twin_clear_file(twin_file_t *file);

/*
 * fixed.c
 */

#define twin_fixed_mul(a, b) ((twin_fixed_t) (((int64_t) (a) * (b)) >> 16))
#define twin_fixed_div(a, b) ((twin_fixed_t) ((((int64_t) (a)) << 16) / b))

twin_fixed_t twin_fixed_sqrt(twin_fixed_t a);

/*
 * font.c
 */

bool twin_has_ucs4(twin_font_t *font, twin_ucs4_t ucs4);

void twin_path_ucs4_stroke(twin_path_t *path, twin_ucs4_t ucs4);

void twin_path_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

void twin_path_utf8(twin_path_t *path, const char *string);

twin_fixed_t twin_width_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

twin_fixed_t twin_width_utf8(twin_path_t *path, const char *string);

void twin_text_metrics_ucs4(twin_path_t *path,
                            twin_ucs4_t ucs4,
                            twin_text_metrics_t *m);

void twin_text_metrics_utf8(twin_path_t *path,
                            const char *string,
                            twin_text_metrics_t *m);

/*
 * hull.c
 */

twin_path_t *twin_path_convex_hull(twin_path_t *path);

/*
 * icon.c
 */

void twin_icon_draw(twin_pixmap_t *pixmap,
                    twin_icon_t icon,
                    twin_matrix_t matrix);

/*
 * image.c
 */

twin_pixmap_t *twin_pixmap_from_file(const char *path, twin_format_t fmt);

/*
 * animation.c
 *
 * Defines the interface for managing frame-based animations.
 * It provides functions to control and manipulate animations such as getting
 * the current frame, advancing the animation, and releasing resources.
 */

/* Get the number of milliseconds the current frame should be displayed. */
twin_time_t twin_animation_get_current_delay(const twin_animation_t *anim);

/* Get the current frame which should be displayed. */
twin_pixmap_t *twin_animation_get_current_frame(const twin_animation_t *anim);

/* Advances the animation to the next frame. If the animation is looping, it
 * will return to the first frame after the last one. */
void twin_animation_advance_frame(twin_animation_t *anim);

/* Frees the memory allocated for the animation, including all associated
 * frames. */
void twin_animation_destroy(twin_animation_t *anim);

twin_animation_iter_t *twin_animation_iter_init(twin_animation_t *anim);

void twin_animation_iter_advance(twin_animation_iter_t *iter);

/*
 * label.c
 */

twin_label_t *twin_label_create(twin_box_t *parent,
                                const char *value,
                                twin_argb32_t foreground,
                                twin_fixed_t font_size,
                                twin_style_t font_style);

void twin_label_set(twin_label_t *label,
                    const char *value,
                    twin_argb32_t foreground,
                    twin_fixed_t font_size,
                    twin_style_t font_style);

/*
 * matrix.c
 */

void twin_matrix_identity(twin_matrix_t *m);

bool twin_matrix_is_identity(twin_matrix_t *m);

void twin_matrix_translate(twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty);

void twin_matrix_scale(twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy);

void twin_matrix_rotate(twin_matrix_t *m, twin_angle_t a);

void twin_matrix_multiply(twin_matrix_t *result,
                          const twin_matrix_t *a,
                          const twin_matrix_t *b);

/*
 * path.c
 */

void twin_path_move(twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void twin_path_rmove(twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void twin_path_draw(twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void twin_path_rdraw(twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

void twin_path_circle(twin_path_t *path,
                      twin_fixed_t x,
                      twin_fixed_t y,
                      twin_fixed_t radius);

void twin_path_ellipse(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t x_radius,
                       twin_fixed_t y_radius);
void twin_path_arc(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius,
                   twin_angle_t start,
                   twin_angle_t extent);

void twin_path_rectangle(twin_path_t *path,
                         twin_fixed_t x,
                         twin_fixed_t y,
                         twin_fixed_t w,
                         twin_fixed_t h);

void twin_path_rounded_rectangle(twin_path_t *path,
                                 twin_fixed_t x,
                                 twin_fixed_t y,
                                 twin_fixed_t w,
                                 twin_fixed_t h,
                                 twin_fixed_t x_radius,
                                 twin_fixed_t y_radius);

void twin_path_lozenge(twin_path_t *path,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t w,
                       twin_fixed_t h);

void twin_path_tab(twin_path_t *path,
                   twin_fixed_t x,
                   twin_fixed_t y,
                   twin_fixed_t w,
                   twin_fixed_t h,
                   twin_fixed_t x_radius,
                   twin_fixed_t y_radius);


void twin_path_close(twin_path_t *path);

void twin_path_empty(twin_path_t *path);

void twin_path_bounds(twin_path_t *path, twin_rect_t *rect);

void twin_path_append(twin_path_t *dst, twin_path_t *src);

twin_path_t *twin_path_create(void);

void twin_path_destroy(twin_path_t *path);

void twin_path_identity(twin_path_t *path);

void twin_path_translate(twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty);

void twin_path_scale(twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy);

void twin_path_rotate(twin_path_t *path, twin_angle_t a);

twin_matrix_t twin_path_current_matrix(twin_path_t *path);

void twin_path_set_matrix(twin_path_t *path, twin_matrix_t matrix);

twin_fixed_t twin_path_current_font_size(twin_path_t *path);

void twin_path_set_font_size(twin_path_t *path, twin_fixed_t font_size);

twin_style_t twin_path_current_font_style(twin_path_t *path);

void twin_path_set_font_style(twin_path_t *path, twin_style_t font_style);

void twin_path_set_cap_style(twin_path_t *path, twin_cap_t cap_style);

twin_cap_t twin_path_current_cap_style(twin_path_t *path);

twin_state_t twin_path_save(twin_path_t *path);

void twin_path_restore(twin_path_t *path, twin_state_t *state);

void twin_composite_path(twin_pixmap_t *dst,
                         twin_operand_t *src,
                         twin_coord_t src_x,
                         twin_coord_t src_y,
                         twin_path_t *path,
                         twin_operator_t operator);
void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path);

void twin_composite_stroke(twin_pixmap_t *dst,
                           twin_operand_t *src,
                           twin_coord_t src_x,
                           twin_coord_t src_y,
                           twin_path_t *stroke,
                           twin_fixed_t pen_width,
                           twin_operator_t operator);

void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width);

/*
 * pattern.c
 */

twin_pixmap_t *twin_make_pattern(void);

/*
 * pixmap.c
 */

#define twin_pixmap_is_animated(pix) ((pix)->animation != NULL)

twin_pixmap_t *twin_pixmap_create(twin_format_t format,
                                  twin_coord_t width,
                                  twin_coord_t height);

twin_pixmap_t *twin_pixmap_create_const(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height,
                                        twin_coord_t stride,
                                        twin_pointer_t pixels);

void twin_pixmap_destroy(twin_pixmap_t *pixmap);

void twin_pixmap_show(twin_pixmap_t *pixmap,
                      twin_screen_t *screen,
                      twin_pixmap_t *higher);

void twin_pixmap_hide(twin_pixmap_t *pixmap);

void twin_pixmap_enable_update(twin_pixmap_t *pixmap);

void twin_pixmap_disable_update(twin_pixmap_t *pixmap);

void twin_pixmap_get_origin(twin_pixmap_t *pixmap,
                            twin_coord_t *ox,
                            twin_coord_t *oy);

void twin_pixmap_set_origin(twin_pixmap_t *pixmap,
                            twin_coord_t ox,
                            twin_coord_t oy);

void twin_pixmap_origin_to_clip(twin_pixmap_t *pixmap);

void twin_pixmap_offset(twin_pixmap_t *pixmap,
                        twin_coord_t offx,
                        twin_coord_t offy);


void twin_pixmap_clip(twin_pixmap_t *pixmap,
                      twin_coord_t left,
                      twin_coord_t top,
                      twin_coord_t right,
                      twin_coord_t bottom);

void twin_pixmap_set_clip(twin_pixmap_t *pixmap, twin_rect_t clip);


twin_rect_t twin_pixmap_get_clip(twin_pixmap_t *pixmap);

twin_rect_t twin_pixmap_save_clip(twin_pixmap_t *pixmap);

void twin_pixmap_restore_clip(twin_pixmap_t *pixmap, twin_rect_t rect);

void twin_pixmap_reset_clip(twin_pixmap_t *pixmap);

void twin_pixmap_damage(twin_pixmap_t *pixmap,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_pixmap_lock(twin_pixmap_t *pixmap);

void twin_pixmap_unlock(twin_pixmap_t *pixmap);

void twin_pixmap_move(twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y);

twin_pointer_t twin_pixmap_pointer(twin_pixmap_t *pixmap,
                                   twin_coord_t x,
                                   twin_coord_t y);

bool twin_pixmap_transparent(twin_pixmap_t *pixmap,
                             twin_coord_t x,
                             twin_coord_t y);

bool twin_pixmap_dispatch(twin_pixmap_t *pixmap, twin_event_t *event);

/*
 * poly.c
 */

void twin_fill_path(twin_pixmap_t *pixmap,
                    twin_path_t *path,
                    twin_coord_t dx,
                    twin_coord_t dy);

/*
 * screen.c
 */

twin_screen_t *twin_screen_create(twin_coord_t width,
                                  twin_coord_t height,
                                  twin_put_begin_t put_begin,
                                  twin_put_span_t put_span,
                                  void *closure);

void twin_screen_destroy(twin_screen_t *screen);

void twin_screen_enable_update(twin_screen_t *screen);

void twin_screen_disable_update(twin_screen_t *screen);

void twin_screen_damage(twin_screen_t *screen,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_screen_register_damaged(twin_screen_t *screen,
                                  void (*damaged)(void *),
                                  void *closure);

void twin_screen_resize(twin_screen_t *screen,
                        twin_coord_t width,
                        twin_coord_t height);

bool twin_screen_damaged(twin_screen_t *screen);

void twin_screen_update(twin_screen_t *screen);

void twin_screen_set_active(twin_screen_t *screen, twin_pixmap_t *pixmap);

twin_pixmap_t *twin_screen_get_active(twin_screen_t *screen);

void twin_screen_set_background(twin_screen_t *screen, twin_pixmap_t *pixmap);

twin_pixmap_t *twin_screen_get_background(twin_screen_t *screen);

void twin_screen_set_cursor(twin_screen_t *screen,
                            twin_pixmap_t *pixmap,
                            twin_fixed_t hotspot_x,
                            twin_fixed_t hotspot_y);

bool twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event);

void twin_screen_lock(twin_screen_t *screen);

void twin_screen_unlock(twin_screen_t *screen);


/*
 * spline.c
 */

void twin_path_curve(twin_path_t *path,
                     twin_fixed_t x1,
                     twin_fixed_t y1,
                     twin_fixed_t x2,
                     twin_fixed_t y2,
                     twin_fixed_t x3,
                     twin_fixed_t y3);

/*
 * timeout.c
 */

twin_timeout_t *twin_set_timeout(twin_timeout_proc_t timeout_proc,
                                 twin_time_t delay,
                                 void *closure);

void twin_clear_timeout(twin_timeout_t *timeout);

/*
 * toplevel.c
 */

twin_toplevel_t *twin_toplevel_create(twin_screen_t *screen,
                                      twin_format_t format,
                                      twin_window_style_t style,
                                      twin_coord_t x,
                                      twin_coord_t y,
                                      twin_coord_t width,
                                      twin_coord_t height,
                                      const char *name);

void twin_toplevel_show(twin_toplevel_t *toplevel);

/*
 * trig.c
 */

twin_fixed_t twin_sin(twin_angle_t a);

twin_fixed_t twin_cos(twin_angle_t a);

twin_fixed_t twin_tan(twin_angle_t a);

void twin_sincos(twin_angle_t a, twin_fixed_t *sin, twin_fixed_t *cos);

/*
 * widget.c
 */

twin_widget_t *twin_widget_create(twin_box_t *parent,
                                  twin_argb32_t background,
                                  twin_coord_t width,
                                  twin_coord_t height,
                                  twin_stretch_t hstretch,
                                  twin_stretch_t vstretch);

void twin_widget_set(twin_widget_t *widget, twin_argb32_t background);

/*
 * window.c
 */

twin_window_t *twin_window_create(twin_screen_t *screen,
                                  twin_format_t format,
                                  twin_window_style_t style,
                                  twin_coord_t x,
                                  twin_coord_t y,
                                  twin_coord_t width,
                                  twin_coord_t height);

void twin_window_destroy(twin_window_t *window);

void twin_window_show(twin_window_t *window);

void twin_window_hide(twin_window_t *window);

void twin_window_configure(twin_window_t *window,
                           twin_window_style_t style,
                           twin_coord_t x,
                           twin_coord_t y,
                           twin_coord_t width,
                           twin_coord_t height);

void twin_window_set_name(twin_window_t *window, const char *name);

void twin_window_style_size(twin_window_style_t style, twin_rect_t *size);

void twin_window_draw(twin_window_t *window);

void twin_window_damage(twin_window_t *window,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_window_queue_paint(twin_window_t *window);

bool twin_window_dispatch(twin_window_t *window, twin_event_t *event);

/*
 * work.c
 */

#define TWIN_WORK_REDISPLAY 0
#define TWIN_WORK_PAINT 1
#define TWIN_WORK_LAYOUT 2

twin_work_t *twin_set_work(twin_work_proc_t work_proc,
                           int priority,
                           void *closure);

void twin_clear_work(twin_work_t *work);

/*
 * backend
 */

typedef struct _twin_context {
    twin_screen_t *screen;
    void *priv;
} twin_context_t;

twin_context_t *twin_create(int width, int height);

void twin_destroy(twin_context_t *ctx);

#endif /* _TWIN_H_ */
