/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _TWIN_PRIVATE_H_
#define _TWIN_PRIVATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "twin.h"

#define twin_bytes_per_pixel(format) (1 << (twin_coord_t) (format))

#define twin_fixed_ceil(f) (((f) + 0xffff) & ~0xffff)
#define twin_fixed_floor(f) ((f) & ~0xffff)

/* Wrap-safe: the difference is taken modulo 2^32 */
#define twin_time_compare(a, op, b) \
    ((twin_time_t) ((uint32_t) (a) - (uint32_t) (b)) op 0)

#define _twin_widget_width(w)                 \
    (((twin_widget_t *) (w))->extents.right - \
     ((twin_widget_t *) (w))->extents.left)

#define _twin_widget_height(w)                 \
    (((twin_widget_t *) (w))->extents.bottom - \
     ((twin_widget_t *) (w))->extents.top)

/*
 * Object layouts.  The public header declares these types opaque;
 * only the library and its backends see their fields.
 */

/**
 * Type-safe pointer union for different pixel formats
 *
 * Provides type-safe access to pixel data in different formats
 * while maintaining memory efficiency through union storage.
 */
typedef union _twin_pointer {
    void *v;               /**< Generic void pointer */
    uint8_t *b;            /**< Raw byte array pointer */
    twin_a8_t *a8;         /**< 8-bit alpha format pointer */
    twin_rgb16_t *rgb16;   /**< 16-bit RGB format pointer */
    twin_argb32_t *argb32; /**< 32-bit ARGB format pointer */
} twin_pointer_t;

/**
 * Animation iterator for frame-by-frame playback
 *
 * Iterator state for stepping through animation frames.
 * Maintains current position and timing information.
 */
typedef struct _twin_animation_iter {
    twin_animation_t *anim;       /**< Animation being iterated */
    twin_count_t current_index;   /**< Current frame index */
    twin_pixmap_t *current_frame; /**< Current frame pixmap */
    twin_time_t current_delay;    /**< Current frame delay time */
} twin_animation_iter_t;

/**
 * Animation structure for frame-based animations
 *
 * Contains all data needed for frame-based animations including
 * timing, frame data, and playback state.
 */
struct _twin_animation {
    twin_pixmap_t **frames;      /**< Array of frame pixmaps */
    twin_count_t n_frames;       /**< Number of frames */
    twin_time_t *frame_delays;   /**< Per-frame timing in milliseconds */
    bool loop;                   /**< Loop animation flag */
    twin_animation_iter_t *iter; /**< Playback iterator */
    twin_coord_t width, height;  /**< Animation dimensions in pixels */
};

/**
 * Procedural drop shadow attached to a pixmap
 *
 * The compositor synthesizes the shadow while scanning out: each edge fades
 * along a smoothstep ramp 2 * blur + 1 pixels wide, a close match to a
 * blurred rectangle.  No pixels are stored: a shadow costs four bytes per
 * pixmap and no blur pass.
 */
typedef struct _twin_shadow {
    int8_t dx, dy;   /**< Offset of the shadow rectangle */
    uint8_t blur;    /**< Blur radius in pixels (0 disables the shadow) */
    twin_a8_t alpha; /**< Peak shadow opacity */
} twin_shadow_t;

/* The pixmap casts a shadow (only with CONFIG_DROP_SHADOW) */
#if defined(CONFIG_DROP_SHADOW)
#define _twin_pixmap_shadowed(p) ((p)->shadow.blur && (p)->shadow.alpha)
#else
#define _twin_pixmap_shadowed(p) ((void) (p), false)
#endif

/**
 * Pixmap structure representing a rectangular array of pixels
 *
 * Core structure representing drawable surfaces with pixels.
 * Can be displayed on screen or used as off-screen render targets.
 */
struct _twin_pixmap {
    /* Screen management */
    struct _twin_screen *screen; /**< Screen displaying this pixmap */
    twin_count_t disable;        /**< Update disable counter */

    /* Display list linkage */
    struct _twin_pixmap *down, *up; /**< Z-order linked list pointers */

    /* Position and layout */
    twin_coord_t x, y;          /**< Screen position */
    twin_format_t format;       /**< Pixel format */
    twin_coord_t width, height; /**< Dimensions in pixels */
    twin_coord_t stride;        /**< Row stride in bytes */
    twin_matrix_t transform;    /**< 2D transformation matrix */

    /* Clipping and origin */
    twin_rect_t clip;                /**< Clipping rectangle */
    twin_coord_t origin_x, origin_y; /**< Drawing origin offset */

    /* Pixel data */
    twin_animation_t *animation; /**< Animation data if animated */
    twin_pointer_t p;            /**< Pixel data pointer (NULL if evicted) */

    /* Compositing attributes, applied by the screen compositor */
    twin_a8_t opacity;       /**< Constant alpha applied on scan-out */
    twin_rect_t opaque_rect; /**< Region (pixmap coords) known to be opaque */
#if defined(CONFIG_DROP_SHADOW)
    twin_shadow_t shadow; /**< Procedural drop shadow */
#endif
    twin_coord_t scan_height; /**< Rows shown from the top on scan-out and
                                   hit by the pointer (normally height) */

    twin_window_t *window; /**< Associated window (if any) */

    /* Transform buffer cache for compositing operations */
    void *xform_cache;       /**< Cached xform buffer */
    size_t xform_cache_size; /**< Cached xform buffer size in bytes */
};

/**
 * Scanline drawing callback called before drawing operation begins
 * @left    : Left edge of drawing region
 * @top     : Top edge of drawing region
 * @right   : Right edge of drawing region
 * @bottom  : Bottom edge of drawing region
 * @closure : User data pointer
 *
 * Backend callback invoked before any scanlines are drawn.
 * Used for setup operations like buffer preparation.
 */
typedef void (*twin_put_begin_t)(twin_coord_t left,
                                 twin_coord_t top,
                                 twin_coord_t right,
                                 twin_coord_t bottom,
                                 void *closure);

/**
 * Scanline drawing callback for outputting pixel data
 * @left    : Left X coordinate of scanline
 * @top     : Y coordinate of scanline
 * @right   : Right X coordinate of scanline
 * @pixels  : Array of ARGB32 pixel data to draw
 * @closure : User data pointer
 *
 * Backend callback invoked for each scanline to be drawn.
 * Pixels array contains (right-left+1) ARGB32 values.
 */
typedef void (*twin_put_span_t)(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_argb32_t *pixels,
                                void *closure);

struct _twin_wm_ops;

/** Maximum number of disjoint damage rectangles tracked per frame */
#define TWIN_DAMAGE_RECTS 8

/**
 * Screen structure managing display and window system
 *
 * Manages the display surface and coordinates drawing operations.
 * Maintains Z-order of pixmaps and handles input event routing.
 */
struct _twin_screen {
    /* Display list management */
    twin_pixmap_t *top, *bottom; /**< Z-order display list */
    twin_pixmap_t *active;       /**< Active pixmap for key events */

    /* Mouse event targeting */
    twin_pixmap_t *target; /**< Current mouse event target */
    bool clicklock;        /**< Mouse click lock state */

    /* Cursor management */
    twin_pixmap_t *cursor;         /**< Mouse cursor pixmap */
    twin_coord_t curs_hx, curs_hy; /**< Cursor hotspot offset */
    twin_coord_t curs_x, curs_y;   /**< Current cursor position */

    /* Display properties */
    twin_coord_t width, height;     /**< Screen dimensions */
    twin_pixmap_t *background;      /**< Background pattern */
    twin_argb32_t background_color; /**< Fill when no background pixmap */

    /* Damage tracking: a short list of rectangles, merged when cheap */
    twin_rect_t damage[TWIN_DAMAGE_RECTS]; /**< Pending damage */
    int n_damage;                          /**< Valid entries in damage[] */
    twin_count_t disable;                  /**< Update disable counter */

    /* Backend interface */
    twin_put_begin_t put_begin; /**< Scanline begin callback */
    twin_put_span_t put_span;   /**< Scanline drawing callback */
    void *closure;              /**< Backend user data */

    /* Window manager policy */
    bool closing;           /**< Being destroyed: no policy */
    twin_window_t *windows; /**< Every window, bottom to top */
    twin_window_t *zombies; /**< Destroyed while busy, freed when idle */
    int dispatching;        /**< Busy depth: dispatch, relayout */
#if defined(CONFIG_WM_MOBILE)
    twin_window_t *bar_top;    /**< System bar above the work area */
    twin_window_t *bar_bottom; /**< System bar below the work area */
    twin_coord_t inset_bottom; /**< Reserved above the bottom bar (IME) */
#endif

    twin_screen_stats_t stats; /**< Compositor work counters */

    /* Span buffer cache for screen updates */
    twin_argb32_t *span_cache;     /**< Cached span buffer */
    twin_coord_t span_cache_width; /**< Cached span buffer width */

    /* Scratch arena for per-frame temporary allocations */
    void *scratch_buf;   /**< Scratch buffer (bump allocator) */
    size_t scratch_size; /**< Scratch buffer capacity in bytes */

    /* Reusable path object cache to avoid repeated alloc/free */
#define TWIN_PATH_CACHE_SIZE 4
    struct _twin_path *path_cache[TWIN_PATH_CACHE_SIZE];
    int path_cache_count;

    /* Cached A8 mask pixmap for twin_composite_path -- avoids
     * per-composite malloc for masks larger than the scratch arena. */
    twin_pixmap_t *mask_cache;
    twin_coord_t mask_cache_width;
    twin_coord_t mask_cache_height;

    /* Event processing: event filter callback */
    bool (*event_filter)(twin_screen_t *screen, twin_event_t *event);
};

#define UCS_PAGE_SHIFT 7

#define UCS_PER_PAGE (1 << UCS_PAGE_SHIFT)

typedef struct _twin_charmap {
    uint16_t page;                  /**< Unicode page number */
    uint16_t offsets[UCS_PER_PAGE]; /**< Glyph offsets in outlines, 0: none */
} twin_charmap_t;

#define TWIN_FONT_TYPE_STROKE 1

#define TWIN_FONT_TYPE_TTF 2

struct _twin_font {
    /* Font definition - must be initialized */
    int type;                      /**< Font type (stroke or TTF) */
    const char *name;              /**< Font name */
    const char *style;             /**< Font style name */
    const twin_charmap_t *charmap; /**< Character map array */
    int n_charmap;                 /**< Number of character maps */
    const signed char *outlines;   /**< Glyph outline data */
    signed char ascender;          /**< Font ascender */
    signed char descender;         /**< Font descender */
    signed char height;            /**< Font height */

    /* Runtime caching */
    const twin_charmap_t *cur_page; /**< Currently cached page */
};

/* Font used by paths that did not select one with twin_path_set_font() */
extern twin_font_t *g_twin_font;

/* Built-in default stroke font */
extern twin_font_t twin_Default_Font_Roman;

struct _twin_window {
    /* Window basics */
    twin_screen_t *screen; /**< Parent screen */
    twin_pixmap_t *pixmap; /**< Window pixmap */

    /* Window properties */
    uint8_t style;       /**< twin_window_style_t */
    uint8_t format;      /**< twin_format_t of the window pixmap */
    twin_rect_t request; /**< Requested client geometry (screen) */
    twin_rect_t client;  /**< Client area rectangle (pixmap) */
#define TWIN_WINDOW_DAMAGE_RECTS 4
    twin_rect_t damage[TWIN_WINDOW_DAMAGE_RECTS]; /**< Pending repaint */
    int n_damage;                                 /**< Valid damage[] */

    /* Window state */
    bool active : 1;      /**< Active window flag */
    bool client_grab : 1; /**< Mouse grab state */
    bool draw_queued : 1; /**< Draw pending flag */
    bool opaque : 1;      /**< Client area is fully painted by draw() */
    bool mapped : 1;      /**< Shown by the application */
    bool frame_dirty : 1; /**< Decorations need repainting */
    bool dead : 1;        /**< Destroyed; the struct outlives a busy screen */
#if defined(CONFIG_WM_DESKTOP)
    bool wm_placed : 1;    /**< Policy already chose a position */
    bool wm_maximized : 1; /**< Covers the work area; wm_saved is the request */
    twin_rect_t wm_saved;  /**< Request to restore after maximize */
    char *name;            /**< Title shown in the title bar */
#endif

    struct _twin_work *paint_work; /**< Queued repaint, if any */

    /* Window manager bookkeeping */
    twin_window_t *wm_next;     /**< Next window in the screen's window list */
    twin_window_t *zombie_next; /**< Next in screen->zombies once dead */

    /* Window data */
    void *client_data; /**< User data pointer */

    /* Window callbacks */
    twin_draw_func_t draw;       /**< Draw callback */
    twin_event_func_t event;     /**< Event handler */
    twin_destroy_func_t destroy; /**< Destroy callback */
};

typedef struct _twin_widget_layout {
    twin_coord_t width, height;    /**< Preferred dimensions */
    twin_stretch_t stretch_width;  /**< Horizontal stretch factor */
    twin_stretch_t stretch_height; /**< Vertical stretch factor */
} twin_widget_layout_t;

/**
 * Optional widget attributes, lazily allocated on first use.
 * Widgets that never register a callback pay zero RAM cost for these
 * fields. Allocated on first callback use.
 */
typedef struct _twin_widget_ext {
    twin_widget_proc_t callback; /**< Application callback (optional) */
    void *callback_data;         /**< Callback user data */
} twin_widget_ext_t;

struct _twin_widget {
    /* Widget hierarchy */
    twin_window_t *window; /**< Parent window */
    twin_widget_t *next;   /**< Next sibling widget */
    twin_box_t *parent;    /**< Parent container */

    /* Event handling */
    twin_widget_proc_t handler; /**< Widget event handler (framework) */
    twin_widget_ext_t *ext;     /**< Optional attributes (lazy, may be NULL) */
    twin_rect_t extents;        /**< Current geometry */
    twin_widget_t *copy_geom;   /**< Geometry source widget (optional) */

    /* Widget state */
    bool paint;  /**< Needs painting */
    bool layout; /**< Needs layout */

    /* Widget appearance */
    twin_argb32_t background;       /**< Background color */
    twin_widget_layout_t preferred; /**< Preferred dimensions */
    twin_shape_t shape;             /**< Widget shape */
    twin_fixed_t radius;            /**< Corner radius */
};

struct _twin_box {
    twin_widget_t widget;       /**< Base widget */
    twin_box_dir_t dir;         /**< Layout direction */
    twin_widget_t *children;    /**< Child widget list */
    twin_widget_t *button_down; /**< Button press target */
};

struct _twin_toplevel {
    twin_box_t box;                 /**< Base box container */
    struct _twin_work *paint_work;  /**< Queued repaint, if any */
    struct _twin_work *layout_work; /**< Queued layout, if any */
};

struct _twin_label {
    twin_widget_t widget;     /**< Base widget */
    char *label;              /**< Label text */
    twin_argb32_t foreground; /**< Text color */
    twin_fixed_t font_size;   /**< Font size */
    twin_style_t font_style;  /**< Font style */
    twin_point_t offset;      /**< Text offset */
    twin_align_t align;       /**< Text alignment */
};

struct _twin_button {
    twin_label_t label; /**< Base label widget */
    bool pressed;       /**< Button pressed state */
    bool active;        /**< Button active state */
};

struct _twin_context {
    twin_screen_t *screen; /**< Screen context */
    void *priv;            /**< Private backend data */
};

/**
 * Structure representing a custom widget with user data.
 * Encapsulates a base widget and allows attaching custom data.
 */
struct _twin_custom_widget {
    twin_widget_t *widget; /**< Base widget functionality */
    void *data;            /**< User-defined data */
};

/*
 * Backend interface: a backend creates the screen, feeds it events and
 * scans it out.  Applications only see the twin_create()/twin_run() layer.
 */

/**
 * Create screen for window system display
 * @width     : Screen width in pixels
 * @height    : Screen height in pixels
 * @put_begin : Backend callback for drawing preparation
 * @put_span  : Backend callback for scanline output
 * @closure   : User data passed to backend callbacks
 *
 * Return Newly created screen, or NULL on failure
 */
twin_screen_t *twin_screen_create(twin_coord_t width,
                                  twin_coord_t height,
                                  twin_put_begin_t put_begin,
                                  twin_put_span_t put_span,
                                  void *closure);

/**
 * Destroy screen and free resources
 * @screen : Screen to destroy
 *
 * Cleans up screen resources including all associated pixmaps.
 */
void twin_screen_destroy(twin_screen_t *screen);

void twin_screen_damage(twin_screen_t *screen,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_screen_resize(twin_screen_t *screen,
                        twin_coord_t width,
                        twin_coord_t height);

bool twin_screen_damaged(twin_screen_t *screen);

void twin_screen_update(twin_screen_t *screen);

bool twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event);

/* Portability: _Alignof is C11; older toolchains may need __alignof__ */
#if !defined(_Alignof) && !defined(__cplusplus)
#if defined(__GNUC__) || defined(__clang__)
#define _Alignof(t) __alignof__(t)
#elif defined(_MSC_VER)
#define _Alignof(t) __alignof(t)
#endif
#endif

/*
 * Scratch arena -- bump allocator for per-frame temporary allocations.
 * Eliminates malloc/free in the rendering hot path.  Reset once per
 * screen update; individual allocations are never freed.  Falls back
 * to malloc when the arena is exhausted.
 */
typedef struct _twin_scratch {
    uint8_t *buf;
    size_t size;
    size_t used;
    size_t high_water; /* peak usage for diagnostics */
} twin_scratch_t;

static inline void twin_scratch_init(twin_scratch_t *s, void *buf, size_t sz)
{
    s->buf = buf;
    s->size = sz;
    s->used = 0;
    s->high_water = 0;
}

static inline size_t twin_scratch_save(twin_scratch_t *s)
{
    return s->used;
}

static inline void twin_scratch_restore(twin_scratch_t *s, size_t saved)
{
    s->used = saved;
}

static inline void *twin_scratch_alloc(twin_scratch_t *s,
                                       size_t n,
                                       size_t align)
{
    if (!align || (align & (align - 1)))
        return NULL;
    size_t off = (s->used + align - 1) & ~(align - 1);
    /* Overflow-safe bounds check */
    if (n > s->size || off > s->size - n)
        return NULL;
    s->used = off + n;
    if (s->used > s->high_water)
        s->high_water = s->used;
    return s->buf + off;
}

/* FIXME: Both twin_private.h and log.h are private header files. They should
 * be moved to src/ directory.
 */
#include "log.h"

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
 * twin_xfixed_t - A fixed-point type in the Q31.32 format.
 *
 *        Hex                 Binary
 * Max 0x7fffffff    0111 1111 1111 1111  1111 1111 1111 1111
 * Min 0x80000000    1000 0000 0000 0000  0000 0000 0000 0000
 *             Decimal                     Actual
 * Max 9,223,372,036,854,775,807       1,073,741,823.9999999997
 * Min -9,223,372,036,854,775,808     -1,073,741,824
 *
 * All of the above tables are based on two's complement representation.
 */
typedef int16_t twin_sfixed_t;
typedef int32_t twin_dfixed_t;
typedef int8_t twin_gfixed_t;
typedef int64_t twin_xfixed_t;

#define twin_sfixed_trunc(f) ((f) >> 4)
#define twin_sfixed_ceil(f) (((f) + 0xf) & ~0xf)

#define twin_int_to_sfixed(i) ((twin_sfixed_t) ((i) * 16))

#define twin_sfixed_to_fixed(s) (((twin_fixed_t) (s)) * 4096)
#define twin_fixed_to_sfixed(f) ((twin_sfixed_t) ((f) >> 12))


#define twin_xfixed_to_fixed(x) ((twin_fixed_t) ((x) >> 16))
#define twin_fixed_to_xfixed(f) (((twin_xfixed_t) (f)) * 65536)

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
    (twin_sfixed_t)((((int32_t) (a)) * ((int32_t) (b))) >> 4)
#define twin_sfixed_div(a, b) \
    (twin_sfixed_t)((((int32_t) (a)) << 4) / ((int32_t) (b)))

#define twin_dfixed_mul(a, b) \
    (twin_dfixed_t)((((int64_t) (a)) * ((int64_t) (b))) >> 8)

/* 64-bit fixed-point multiplication and division
 * These require 128-bit intermediate results for full precision.
 *
 * For 64-bit platforms with __int128_t support, use native 128-bit arithmetic.
 * For 32-bit platforms (RISC-V 32, ARM32, i386), use software implementation.
 */
#if defined(__SIZEOF_INT128__) ||               \
    (defined(__GNUC__) && !defined(__i386__) && \
     (defined(__x86_64__) || defined(__aarch64__)))
/* Native 128-bit integer support (x86-64, AArch64, etc.) */
#define twin_xfixed_mul(a, b) \
    (twin_xfixed_t)((((__int128_t) (a)) * ((__int128_t) (b))) >> 32)
#define twin_xfixed_div(a, b) \
    (twin_xfixed_t)((((__int128_t) (a)) << 32) / ((__int128_t) (b)))
#else
/* Software implementation for 32-bit platforms
 * Uses 64x64 -> 128 multiplication split into 32x32 -> 64 parts
 *
 * For a * b where a and b are 64-bit:
 *   a = ah << 32 | al
 *   b = bh << 32 | bl
 *   a * b = (ah*bh << 64) + (ah*bl << 32) + (al*bh << 32) + (al*bl)
 *
 * After right shift by 32:
 *   result = (ah*bh << 32) + (ah*bl) + (al*bh) + (al*bl >> 32)
 */
static inline twin_xfixed_t _twin_xfixed_mul_32bit(twin_xfixed_t a,
                                                   twin_xfixed_t b)
{
    /* Handle sign */
    int neg = ((a < 0) != (b < 0));
    uint64_t ua = (a < 0) ? -a : a;
    uint64_t ub = (b < 0) ? -b : b;

    /* Split into high and low 32-bit parts */
    uint64_t ah = ua >> 32;
    uint64_t al = ua & 0xFFFFFFFFULL;
    uint64_t bh = ub >> 32;
    uint64_t bl = ub & 0xFFFFFFFFULL;

    /* Compute partial products */
    uint64_t p_hh = ah * bh, p_hl = ah * bl;
    uint64_t p_lh = al * bh, p_ll = al * bl;

    /* Combine with appropriate shifts, handling carry correctly
     * Result = (p_hh << 32) + p_hl + p_lh + (p_ll >> 32)
     * Split middle terms to prevent overflow: add low 32 bits first,
     * then high 32 bits with carries
     */
    uint64_t mid_low =
        (p_hl & 0xFFFFFFFFULL) + (p_lh & 0xFFFFFFFFULL) + (p_ll >> 32);
    uint64_t mid_high = (p_hl >> 32) + (p_lh >> 32) + (mid_low >> 32);
    uint64_t result =
        (p_hh << 32) + (mid_high << 32) + (mid_low & 0xFFFFFFFFULL);

    return neg ? -(int64_t) result : (int64_t) result;
}

static inline twin_xfixed_t _twin_xfixed_div_32bit(twin_xfixed_t a,
                                                   twin_xfixed_t b)
{
    /* Handle sign */
    int neg = ((a < 0) != (b < 0));
    uint64_t ua = (a < 0) ? -a : a;
    uint64_t ub = (b < 0) ? -b : b;

    /* Fixed-point division: (a / b) in Q32.32 = (a << 32) / b in integer
     * arithmetic Since ua is 64-bit, (ua << 32) is 96-bit. Perform long
     * division: Split ua into 32-bit parts: ua = (ah << 32) | al Then (ua <<
     * 32) = (ah << 64) | (al << 32)
     *
     * Step 1: Divide high part (ah << 32) by ub to get high 32 bits of quotient
     * Step 2: Divide ((remainder << 32) | al) by ub to get low 32 bits
     */
    uint64_t ah = ua >> 32;           /* High 32 bits */
    uint64_t al = ua & 0xFFFFFFFFULL; /* Low 32 bits */

    /* Divide (ah << 32) / ub */
    uint64_t ah_shifted = ah << 32;
    uint64_t q_h = ah_shifted / ub;
    uint64_t rem = ah_shifted % ub;

    /* Divide ((rem << 32) | al) / ub */
    uint64_t dividend_low = (rem << 32) | al;
    uint64_t q_l = dividend_low / ub;

    /* Combine quotients: result = (q_h << 32) | q_l */
    uint64_t result = (q_h << 32) | q_l;

    return neg ? -(int64_t) result : (int64_t) result;
}

#define twin_xfixed_mul(a, b) _twin_xfixed_mul_32bit(a, b)
#define twin_xfixed_div(a, b) _twin_xfixed_div_32bit(a, b)
#endif

/*
 * 'double' is a no-no in any shipping code, but useful during
 * development
 */
#define twin_double_to_sfixed(d) ((twin_sfixed_t) ((d) * 16.0))
#define twin_sfixed_to_double(f) ((double) (f) / 16.0)

#define TWIN_SFIXED_ONE (0x10)
#define TWIN_SFIXED_TOLERANCE (TWIN_SFIXED_ONE >> 2)
#define TWIN_SFIXED_MIN (-0x7fff)
#define TWIN_SFIXED_MAX (0x7fff)


#define TWIN_GFIXED_ONE (0x40)

#define TWIN_XFIXED_ONE (0x100000000)
/*
 * Compositing stuff
 */
#define twin_int_mult(a, b, t) \
    ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))
#define twin_get_8(v, i) ((uint16_t) (uint8_t) ((v) >> (i)))
#define twin_sat(t) ((uint8_t) ((t) | (0 - ((t) >> 8))))

/*
 * Two channels at a time: 16-bit lanes hold the red/blue or alpha/green
 * pair.  A lane never overflows (255 * 255 + 0x80 + 0xff < 0x10000), so this
 * rounds exactly like twin_int_mult does per channel.
 */
#define TWIN_LANES 0x00ff00ffu

static inline uint32_t _twin_mul_lanes(uint32_t x, uint32_t m)
{
    uint32_t t = x * m + 0x00800080u;
    return ((t + ((t >> 8) & TWIN_LANES)) >> 8) & TWIN_LANES;
}

/* Saturate each lane to 0xff */
static inline uint32_t _twin_sat_lanes(uint32_t x)
{
    uint32_t carry = (x >> 8) & 0x00010001u;
    return (x | (carry * 0xff)) & TWIN_LANES;
}

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

#ifndef min
#if defined(__GNUC__) || defined(__clang__)
#define min(x, y)            \
    ({                       \
        typeof(x) _x = (x);  \
        typeof(y) _y = (y);  \
        (void) (&_x == &_y); \
        _x < _y ? _x : _y;   \
    })
#else
/* Generic implementation: potential side effects */
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#endif
#ifndef max
#if defined(__GNUC__) || defined(__clang__)
#define max(x, y)            \
    ({                       \
        typeof(x) _x = (x);  \
        typeof(y) _y = (y);  \
        (void) (&_x == &_y); \
        _x > _y ? _x : _y;   \
    })
#else
/* Generic implementation: potential side effects */
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif
#endif

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
    /* Where the clipped destination starts relative to the requested one;
     * the transform maps unclipped destination offsets to the source. */
    twin_coord_t off_x, off_y;
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

/*
 * Inline storage for small paths avoids heap allocation in the common
 * case (rectangles, circles, rounded rects).  Only paths exceeding
 * these limits spill to malloc.
 */
#ifndef TWIN_PATH_INLINE_POINTS
#define TWIN_PATH_INLINE_POINTS 64
#endif
#ifndef TWIN_PATH_INLINE_SUBLEN
#define TWIN_PATH_INLINE_SUBLEN 4
#endif

struct _twin_path {
    twin_spoint_t *points;
    int size_points;
    int npoints;
    int *sublen;
    int size_sublen;
    int nsublen;
    twin_state_t state;
    twin_spoint_t inline_points[TWIN_PATH_INLINE_POINTS];
    int inline_sublen[TWIN_PATH_INLINE_SUBLEN];
};

typedef struct _twin_gpoint {
    twin_gfixed_t x, y;
} twin_gpoint_t;

/* Compositing function declarations - auto-generated */
#include "composite-decls.h"

/*
 * Geometry helper functions
 */
twin_dfixed_t _twin_distance_to_line_squared(twin_spoint_t *p,
                                             twin_spoint_t *p1,
                                             twin_spoint_t *p2);

/*
 * Polygon stuff
 */

twin_xfixed_t _twin_xfixed_sqrt(twin_xfixed_t a);
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
 * Dispatch stuff
 */
typedef struct _twin_queue {
    struct _twin_queue *next;
    struct _twin_queue *order;
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

void _twin_run_timeout(void);

twin_time_t _twin_timeout_delay(void);

/* How long a backend may sleep waiting for input: not at all while the
 * screen has damage to show, otherwise until the next timeout, at most
 * @cap ms (@cap < 0: no cap, and -1 then means until input arrives) */
twin_time_t _twin_idle_timeout(twin_screen_t *screen, twin_time_t cap);

void _twin_run_work(void);

/* Event dispatch - internal use only
 *
 * twin_dispatch_once() - Single iteration of event loop
 *   Processes timeouts, work queue, and backend events once.
 *
 *   Backend implementations should call this in their .start() function:
 *     while (twin_dispatch_once(ctx))
 *         ;
 *
 *   For WebAssembly, use with emscripten_set_main_loop_arg():
 *     static void main_loop(void *arg) {
 *         twin_dispatch_once((twin_context_t *) arg);
 *     }
 *     emscripten_set_main_loop_arg(main_loop, ctx, 0, 1);
 */
bool twin_dispatch_once(twin_context_t *ctx);

void _twin_box_init(twin_box_t *box,
                    twin_box_t *parent,
                    twin_window_t *window,
                    twin_box_dir_t dir,
                    twin_widget_proc_t dispatch);

/* Flag @box and every widget inside it for repainting, e.g. when the
 * window's pixels were released and have to be drawn again in full */
void _twin_box_mark_paint(twin_box_t *box);

twin_dispatch_result_t _twin_box_dispatch(twin_widget_t *widget,
                                          twin_event_t *event,
                                          void *closure);

void _twin_widget_init(twin_widget_t *widget,
                       twin_box_t *parent,
                       twin_window_t *window,
                       twin_widget_layout_t preferred,
                       twin_widget_proc_t dispatch);

void _twin_widget_paint_shape(twin_widget_t *widget,
                              twin_shape_t shape,
                              twin_coord_t left,
                              twin_coord_t top,
                              twin_coord_t right,
                              twin_coord_t bottom,
                              twin_fixed_t radius);

twin_dispatch_result_t _twin_widget_dispatch(twin_widget_t *widget,
                                             twin_event_t *event,
                                             void *closure);


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
                      twin_widget_proc_t dispatch);

twin_dispatch_result_t _twin_label_dispatch(twin_widget_t *widget,
                                            twin_event_t *event,
                                            void *closure);

twin_dispatch_result_t _twin_toplevel_dispatch(twin_widget_t *widget,
                                               twin_event_t *event,
                                               void *closure);

void _twin_toplevel_init(twin_toplevel_t *toplevel,
                         twin_widget_proc_t dispatch,
                         twin_window_t *window,
                         const char *name);

void _twin_toplevel_queue_paint(twin_widget_t *widget);

void _twin_toplevel_queue_layout(twin_widget_t *widget);

twin_dispatch_result_t _twin_button_dispatch(twin_widget_t *widget,
                                             twin_event_t *event,
                                             void *closure);

void _twin_button_init(twin_button_t *button,
                       twin_box_t *parent,
                       const char *value,
                       twin_argb32_t foreground,
                       twin_fixed_t font_size,
                       twin_style_t font_style,
                       twin_widget_proc_t dispatch);

typedef struct twin_backend {
    /* Initialize the backend */
    twin_context_t *(*init)(int width, int height);

    /* Wait for and dispatch input; false when the user asked to quit.
     * Sleep with _twin_idle_timeout() rather than spin. */
    bool (*poll)(twin_context_t *ctx);

    /* Run the main loop after the application callback, for platforms
     * that own the loop (the browser); NULL: twin_run() loops on
     * twin_dispatch_once() */
    void (*start)(twin_context_t *ctx, void (*init_callback)(twin_context_t *));

    /* Device cleanup when drawing is done */
    void (*exit)(twin_context_t *ctx);
} twin_backend_t;

/* The backend linked into the program */
extern const twin_backend_t g_twin_backend;

/*
 * Window manager policy interface
 *
 * A policy decides window geometry, decorations, stacking, and how pointer
 * events reach clients.  Exactly one is built: wm-none.c (WM_NONE, the
 * minimal one), wm-desktop.c (WM_DESKTOP, classic) or wm-mobile.c
 * (WM_MOBILE, embedded).  It defines the hooks below and window.c,
 * screen.c and wm.c call them directly; a hook a policy has no use for is
 * an inline no-op here, so the call compiles away.  Policies never free a
 * window.
 */
#if defined(CONFIG_WM_DESKTOP) + defined(CONFIG_WM_MOBILE) + \
        defined(CONFIG_WM_NONE) !=                           \
    1
#error \
    "Select exactly one of CONFIG_WM_DESKTOP, CONFIG_WM_MOBILE, CONFIG_WM_NONE"
#endif

#if defined(CONFIG_WM_DESKTOP)
#define TWIN_WM_DECORATES 1 /* title bars: decorate(), hit(), activate() */
#else
#define TWIN_WM_DECORATES 0
#endif
#if defined(CONFIG_WM_MOBILE)
#define TWIN_WM_BARS 1 /* system bars and a work area: filter(), work_area() \
                        */
#else
#define TWIN_WM_BARS 0
#endif

/* Bytes of policy state that follow the screen in its allocation, zeroed;
 * _twin_wm_state() finds them */
extern const size_t _twin_wm_state_size;
#define _twin_wm_state(screen) ((void *) ((twin_screen_t *) (screen) + 1))

/* The screen was created / is going away.  fini() runs before the windows
 * are destroyed and must let go of them: from then on (screen->closing)
 * window.c bypasses the policy. */
void _twin_wm_init(twin_screen_t *screen);
void _twin_wm_fini(twin_screen_t *screen);

/* Compute pixmap geometry (screen coordinates) and decoration border */
void _twin_wm_layout(twin_window_t *window,
                     twin_rect_t *geom,
                     twin_rect_t *border);

/* Application mapped / unmapped a window.  On destroy, the policy may take
 * ownership of window->pixmap (setting it to NULL) to animate the departure
 * after the window object is gone. */
void _twin_wm_map(twin_window_t *window);
void _twin_wm_unmap(twin_window_t *window, bool destroying);

/* Deliver an event targeted at a window */
bool _twin_wm_dispatch(twin_window_t *window, twin_event_t *event);

/* Re-place the stack after a mapped window changed style or opacity, or the
 * system bars, the inset or the screen size changed.  Every window has been
 * re-laid out already. */
#if defined(CONFIG_WM_NONE)
static inline void _twin_wm_refresh(twin_screen_t *screen)
{
    (void) screen;
}
#else
void _twin_wm_refresh(twin_screen_t *screen);
#endif

#if TWIN_WM_DECORATES
/* Paint decorations into window->pixmap */
void _twin_wm_decorate(twin_window_t *window);
/* Pointer hit test in screen coordinates (others: the pixmap's shown
 * rows, see _twin_window_hit) */
bool _twin_wm_hit(twin_window_t *window, twin_coord_t x, twin_coord_t y);
/* Explicit activation request (twin_window_activate) */
void _twin_wm_activate(twin_window_t *window);
#else
static inline void _twin_wm_decorate(twin_window_t *window)
{
    (void) window;
}
static inline void _twin_wm_activate(twin_window_t *window)
{
    _twin_wm_map(window);
}
#endif

#if TWIN_WM_BARS
/* Gesture hook: sees every pointer event (motion, button down/up) before it
 * is routed to a window; true consumes it.  Other kinds bypass it. */
bool _twin_wm_filter(twin_screen_t *screen, twin_event_t *event);
/* Shrink @area (preset to the whole screen) to what application windows may
 * cover, e.g. minus the system bars */
void _twin_wm_work_area(const twin_screen_t *screen, twin_rect_t *area);
#else
static inline bool _twin_wm_filter(twin_screen_t *screen, twin_event_t *event)
{
    (void) screen;
    (void) event;
    return false;
}
#endif

/* window.c services used by policies */
void _twin_window_relayout(twin_window_t *window);
bool _twin_window_deliver(twin_window_t *window, twin_event_t *event);
void _twin_window_evict(twin_window_t *window);
bool _twin_window_restore(twin_window_t *window);
#if defined(CONFIG_WM_MOBILE)
bool _twin_window_is_bar(const twin_window_t *window);
#else
/* Only the mobile window manager has system bars */
static inline bool _twin_window_is_bar(const twin_window_t *window)
{
    (void) window;
    return false;
}
#endif
/* On screen with its pixels (not hidden, not evicted) */
bool _twin_window_shown(const twin_window_t *window);
/* Mapped window the policy stacks and focuses (not Plain, not a bar) */
bool _twin_window_managed(const twin_window_t *window);
void _twin_window_raise(twin_window_t *window);
/*
 * Client callbacks run while the screen is busy (an event, a relayout).  A
 * window destroyed then is torn down and unlinked at once, but its struct stays
 * readable (window->dead) until the outermost busy section ends, so code
 * holding the pointer can check it.
 */
void _twin_screen_busy(twin_screen_t *screen);
void _twin_screen_idle(twin_screen_t *screen);

/* screen.c: route a pointer event to windows (grab, hover, delivery),
 * skipping the policy's gesture filter */
bool _twin_screen_route(twin_screen_t *screen, twin_event_t *event);

/* wm.c: re-lay out every window after the screen, the bars or the inset
 * changed, then let the policy re-place the stack */
void _twin_screen_relayout(twin_screen_t *screen);

/*
 * Pixmap internals
 */

/* Screen-space rectangle touched by the pixmap, including its shadow */
twin_rect_t _twin_pixmap_extents(const twin_pixmap_t *pixmap);

/* Show only the top @rows rows on scan-out and to the pointer (the rest
 * keeps its pixels); pixmap->height shows everything.  Used by the desktop
 * policy to iconify a window down to its title bar. */
void _twin_pixmap_set_scan_height(twin_pixmap_t *pixmap, twin_coord_t rows);

/* Damage everything the pixmap covers on screen, shadow included */
void _twin_pixmap_damage_extents(twin_pixmap_t *pixmap);

/*
 * Add @r to a rectangle list of capacity @max.  Rectangles merge only when
 * their union costs no more pixels than the pair; a full list folds @r into
 * the entry that grows least.  Shared by screen and window damage.
 */
void _twin_rect_list_add(twin_rect_t *list, int *n, int max, twin_rect_t r);

/* Unlink from the display list without dropping focus/pointer state */
void _twin_pixmap_unlink(twin_pixmap_t *pixmap);

/* Drop the screen's focus and pointer references to @pixmap */
void _twin_screen_forget(twin_screen_t *screen, const twin_pixmap_t *pixmap);

/* Pixmap header with no pixel storage; every drawing operation clips out */
twin_pixmap_t *_twin_pixmap_create_stub(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height);

static inline bool _twin_pixmap_is_stub(const twin_pixmap_t *pixmap)
{
    return pixmap->p.v == NULL;
}

/* Wrap caller-owned pixels in a pixmap header without allocating */
void _twin_pixmap_init(twin_pixmap_t *pixmap,
                       twin_format_t format,
                       twin_coord_t width,
                       twin_coord_t height,
                       twin_coord_t stride,
                       void *pixels);
void _twin_pixmap_init_a8(twin_pixmap_t *pixmap,
                          twin_coord_t width,
                          twin_coord_t height,
                          twin_coord_t stride,
                          void *pixels);

/*
 * Screen compositor primitives (screen-ops.c, renderer independent)
 */
void _twin_screen_blend_argb32(twin_argb32_t *dst,
                               const twin_argb32_t *src,
                               twin_a8_t alpha,
                               int width);
void _twin_screen_blend_rgb16(twin_argb32_t *dst,
                              const twin_rgb16_t *src,
                              twin_a8_t alpha,
                              int width);
void _twin_screen_fill(twin_argb32_t *dst, twin_argb32_t color, int width);

/*
 * Coverage cache (cache.c)
 *
 * Entries hold A8 coverage plus placement.  Lookups bump LRU order; inserts
 * evict least recently used entries until the budget is honored.
 */
typedef struct _twin_cache_entry {
    struct _twin_cache_entry *hnext;       /* hash chain */
    struct _twin_cache_entry *prev, *next; /* LRU list */
    uint64_t key;
    uint32_t size; /* bytes charged to the budget */
    twin_coord_t width, height, stride;
    twin_coord_t left, top; /* placement relative to the key's origin */
    twin_fixed_t advance;   /* glyph advance (glyph entries only) */
    uint8_t data[];         /* stride * height bytes of A8 coverage */
} twin_cache_entry_t;

/* Row stride of A8 coverage: rows start on 4-byte boundaries */
static inline twin_coord_t _twin_a8_stride(twin_coord_t width)
{
    return (twin_coord_t) ((width + 3) & ~3);
}

/* Zeroed entry outside the cache (caller frees); the cache uses it too */
twin_cache_entry_t *_twin_cache_entry_alloc(twin_coord_t width,
                                            twin_coord_t height);
twin_cache_entry_t *_twin_cache_lookup(uint64_t key);
twin_cache_entry_t *_twin_cache_insert(uint64_t key,
                                       twin_coord_t width,
                                       twin_coord_t height);
bool _twin_cache_enabled(void);

/* Largest single entry admitted to the cache, in bytes */
#ifndef CONFIG_CACHE_MAX_ENTRY
#define CONFIG_CACHE_MAX_ENTRY 4096
#endif
#ifndef CONFIG_CACHE_SIZE
#define CONFIG_CACHE_SIZE 24
#endif

/* 64-bit FNV-1a */
#define TWIN_HASH_INIT 0xcbf29ce484222325ULL
static inline uint64_t _twin_hash(uint64_t h, const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len--) {
        h ^= *p++;
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* Set up a caller-owned path (inline storage, default state) / release it */
void _twin_path_init(twin_path_t *path);
void _twin_path_fini(twin_path_t *path);

/* Bounds of points [first, npoints), as twin_path_bounds() */
void _twin_path_bounds_from(const twin_path_t *path,
                            int first,
                            twin_rect_t *rect);

int _twin_utf8_to_ucs4(const char *src, twin_ucs4_t *dst);

/*
 * Test and diagnostics hooks (not part of the public API)
 */

#if defined(CONFIG_MEMORY_STATS)
/* Make the next @n tracked allocations fail (out-of-memory tests) */
extern unsigned int _twin_alloc_fail_next;
#endif

/*
 * Library-internal helpers: used across src/, not part of the public API
 */

twin_angle_t twin_acos(twin_fixed_t x);

/**
 * Calculate square root of fixed-point number
 * @a : Input value in fixed-point format
 *
 * Return Square root in fixed-point format
 */
twin_fixed_t twin_fixed_sqrt(twin_fixed_t a);

bool twin_matrix_is_identity(twin_matrix_t *m);

void twin_matrix_multiply(twin_matrix_t *result,
                          const twin_matrix_t *a,
                          const twin_matrix_t *b);

void twin_matrix_translate(twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty);

void twin_path_append(twin_path_t *dst, twin_path_t *src);

void twin_path_arc_circle(twin_path_t *path,
                          bool large_arc,
                          bool sweep,
                          twin_fixed_t radius,
                          twin_fixed_t cur_x,
                          twin_fixed_t cur_y,
                          twin_fixed_t target_x,
                          twin_fixed_t target_y);

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

/**
 * Add filled outline of Unicode character to path
 * @path : Path to add character to
 * @ucs4 : Unicode character to add
 *
 * Adds character outline to path for filled text rendering.
 */
void twin_path_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

/**
 * Get advance width of Unicode character
 * @path : Path with current font settings
 * @ucs4 : Unicode character
 *
 * Return Character advance width in fixed-point format
 */
twin_fixed_t twin_width_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

/**
 * Create pixmap using existing pixel buffer
 * @format : Pixel format
 * @width  : Width in pixels
 * @height : Height in pixels
 * @stride : Row stride in bytes
 * @pixels : Existing pixel buffer (not copied)
 *
 * Return Pixmap wrapper for existing buffer, or NULL on failure
 */
twin_pixmap_t *twin_pixmap_create_const(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height,
                                        twin_coord_t stride,
                                        twin_pointer_t pixels);

void twin_pixmap_damage(twin_pixmap_t *pixmap,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

bool twin_pixmap_dispatch(twin_pixmap_t *pixmap, twin_event_t *event);

void twin_pixmap_get_origin(twin_pixmap_t *pixmap,
                            twin_coord_t *ox,
                            twin_coord_t *oy);

void twin_pixmap_set_clip(twin_pixmap_t *pixmap, twin_rect_t clip);

void twin_pixmap_set_origin(twin_pixmap_t *pixmap,
                            twin_coord_t ox,
                            twin_coord_t oy);

static inline int twin_ucs_page(uint32_t ucs4)
{
    return ucs4 >> UCS_PAGE_SHIFT;
}

static inline int twin_ucs_char_in_page(uint32_t ucs4)
{
    return ucs4 & (UCS_PER_PAGE - 1);
}

/* Animations belong to their pixmap: twin_pixmap_destroy() frees them */
void _twin_animation_destroy(twin_animation_t *anim);
twin_animation_iter_t *_twin_animation_iter_init(twin_animation_t *anim);

/* Title-bar icons of the desktop policy (built with CONFIG_WM_DESKTOP) */
typedef enum _twin_icon {
    TwinIconMenu,
    TwinIconIconify,
    TwinIconRestore,
    TwinIconClose,
    TwinIconResize,
} twin_icon_t;

void twin_icon_draw(twin_pixmap_t *pixmap,
                    twin_icon_t icon,
                    twin_matrix_t matrix);

/* Replace the monotonic clock; NULL restores the system clock */
void _twin_set_time_source(twin_time_t (*now)(void));

/* Debug switches used by differential tests:
 * NO_OCCLUSION composites every layer at every pixel;
 * NO_ANIMATION makes window manager transitions instant;
 * ROW_FALLBACK takes the plain bottom-up path that rows with too many
 * layers use, so it can be compared with the culled one. */
#define TWIN_DEBUG_NO_OCCLUSION 0x1
#define TWIN_DEBUG_NO_ANIMATION 0x2
#define TWIN_DEBUG_ROW_FALLBACK 0x4
extern unsigned _twin_debug;

/* utility */

#ifdef _MSC_VER
#include <intrin.h>
static inline int twin_clz(uint32_t v)
{
    uint32_t leading_zero = 0;
    if (_BitScanReverse(&leading_zero, v))
        return 31 - leading_zero;
    return 32; /* undefined behavior */
}
static inline int twin_clzll(uint64_t v)
{
    uint32_t leading_zero = 0;
    if (_BitScanReverse64(&leading_zero, v))
        return 63 - leading_zero;
    return 64; /* undefined behavior */
}
#elif defined(__GNUC__) || defined(__clang__)
static inline int twin_clz(uint32_t v)
{
    return __builtin_clz(v);
}
static inline int twin_clzll(uint64_t v)
{
    return __builtin_clzll(v);
}
#else /* generic implementation */
static inline int twin_clz(uint32_t v)
{
    /* De Bruijn log2floor + inversion to get count of leading zeros.
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
     */
    static const uint8_t mul_debruijn[] = {
        0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31,
    };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return 31 - (int) mul_debruijn[(uint32_t) (v * 0x07C4ACDDU) >> 27];
}
static inline int twin_clzll(uint64_t v)
{
    /* Split into high/low 32-bit halves and reuse the verified 32-bit
     * de Bruijn CLZ.  Avoids a separate 64-bit lookup table.
     */
    uint32_t hi = (uint32_t) (v >> 32);
    return hi ? twin_clz(hi) : 32 + twin_clz((uint32_t) v);
}
#endif

extern const uint8_t _twin_cursor_default[];

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

/*
 * Internal functions moved from public API
 * These functions are implementation details and should not be used
 * by applications directly.
 */

/* Low-level drawing operations */
void twin_path_convolve(twin_path_t *dest,
                        twin_path_t *stroke,
                        twin_path_t *pen,
                        twin_scratch_t *scratch);


void twin_fill_path(twin_pixmap_t *pixmap,
                    twin_path_t *path,
                    twin_coord_t dx,
                    twin_coord_t dy,
                    twin_scratch_t *scratch);

void twin_composite_path(twin_pixmap_t *dst,
                         twin_operand_t *src,
                         twin_coord_t src_x,
                         twin_coord_t src_y,
                         twin_path_t *path,
                         twin_operator_t operator,
                         twin_scratch_t * scratch);

void twin_composite_stroke(twin_pixmap_t *dst,
                           twin_operand_t *src,
                           twin_coord_t src_x,
                           twin_coord_t src_y,
                           twin_path_t *stroke,
                           twin_fixed_t pen_width,
                           twin_operator_t operator,
                           twin_scratch_t * scratch);

/* Internal window management helpers */
bool _twin_window_hit(twin_window_t *window, twin_coord_t x, twin_coord_t y);
bool _twin_window_dispatch(twin_window_t *window, twin_event_t *event);

/* Internal pixmap operations */
twin_pointer_t twin_pixmap_pointer(twin_pixmap_t *pixmap,
                                   twin_coord_t x,
                                   twin_coord_t y);
void twin_pixmap_reset_xform_cache(twin_pixmap_t *pixmap);
void twin_pixmap_origin_to_clip(twin_pixmap_t *pixmap);

/* Internal screen operations */
void twin_screen_enable_update(twin_screen_t *screen);
void twin_screen_disable_update(twin_screen_t *screen);

/* Reusable path acquisition -- avoids malloc/free per draw call */
twin_path_t *_twin_path_acquire(twin_screen_t *screen);
void _twin_path_release(twin_screen_t *screen, twin_path_t *path);

/* Path convex hull computation */
twin_path_t *twin_path_convex_hull(twin_path_t *path, twin_scratch_t *scratch);

/*
 * Memory pointer validation
 *
 * This defines the minimum valid pointer address for closure validation.
 * Different environments have different memory layouts:
 *
 * - Unix-like systems (Linux/BSD/macOS/Solaris): First 4KB (0x1000) typically
 * unmapped
 * - Windows: First 64KB (0x10000) reserved
 * - Bare-metal: May have valid memory starting at 0x0
 */
#ifndef TWIN_POINTER_MIN_VALID
#if defined(_WIN32) || defined(_WIN64)
#define TWIN_POINTER_MIN_VALID 0x10000 /* Windows: 64KB */
#elif defined(__unix__) || defined(__unix) || defined(unix) ||             \
    (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__) ||     \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__DragonFly__) || defined(__sun) || defined(__HAIKU__) ||      \
    defined(__ANDROID__)
#define TWIN_POINTER_MIN_VALID 0x1000 /* Unix-like: 4KB */
#else
#define TWIN_POINTER_MIN_VALID 0x100 /* Bare-metal/embedded: 256 bytes */
#endif
#endif

/*
 * Validate a pointer for basic sanity
 * Returns true if the pointer appears to be valid
 */
static inline bool twin_pointer_valid(const void *ptr)
{
    return ptr && (uintptr_t) ptr >= TWIN_POINTER_MIN_VALID;
}

/*
 * Closure lifetime management
 *
 * This system tracks closure pointers used in work queues and timeouts
 * to prevent use-after-free bugs when widgets are destroyed while
 * callbacks are still queued.
 */

/* Maximum number of tracked closures */
#define TWIN_MAX_CLOSURES 128

/* Closure tracking entry */
typedef struct _twin_closure_entry {
    void *closure;        /* Closure pointer */
    uint32_t ref_count;   /* Reference count */
    bool marked_for_free; /* Closure is being freed */
} twin_closure_entry_t;

/* Closure tracking table */
typedef struct _twin_closure_tracker {
    twin_closure_entry_t entries[TWIN_MAX_CLOSURES];
    int count;
    /* Mutex would go here for thread safety if needed */
} twin_closure_tracker_t;

/* Global closure tracker instance */
extern twin_closure_tracker_t _twin_closure_tracker;

/* Register a closure with the tracking system */
bool _twin_closure_register(void *closure);

/* Unregister a closure from the tracking system */
void _twin_closure_unregister(void *closure);

/* Increment reference count for a closure */
bool _twin_closure_ref(void *closure);

/* Decrement reference count for a closure */
bool _twin_closure_unref(void *closure);

/* Check if a closure is valid and not marked for deletion */
bool _twin_closure_is_valid(void *closure);

/* Mark a closure as being freed (prevents new references) */
void _twin_closure_mark_for_free(void *closure);

/*
 * Memory allocation wrappers.
 *
 * When CONFIG_MEMORY_STATS is enabled, all allocations are routed
 * through tracking functions that maintain current/peak usage counters.
 * When disabled, the macros expand to raw malloc/free -- zero overhead.
 */
/*
 * Internal memory statistics counters.
 */
typedef struct {
    size_t current_bytes;
    size_t peak_bytes;
    size_t total_allocs;
    size_t total_frees;
    size_t total_bytes;
} twin_memstats_t;

/*
 * Raw allocator backend.  When CONFIG_MEM_TLSF is enabled these route
 * through a fixed-size TLSF pool; otherwise they are trivial wrappers
 * around libc malloc/calloc/realloc/free.
 */
#if defined(CONFIG_MEM_TLSF)
void twin_mem_pool_init(void);
void *twin_raw_malloc(size_t size);
void *twin_raw_calloc(size_t n, size_t size);
void *twin_raw_realloc(void *ptr, size_t size);
void twin_raw_free(void *ptr);
#else
static inline void twin_mem_pool_init(void) {}
static inline void *twin_raw_malloc(size_t sz)
{
    return malloc(sz);
}
static inline void *twin_raw_calloc(size_t n, size_t sz)
{
    return calloc(n, sz);
}
static inline void *twin_raw_realloc(void *p, size_t sz)
{
    return realloc(p, sz);
}
static inline void twin_raw_free(void *p)
{
    free(p);
}
#endif

#if defined(CONFIG_MEMORY_STATS)
void *_twin_malloc(size_t size, const char *file, int line);
void *_twin_calloc(size_t n, size_t size, const char *file, int line);
void *_twin_realloc(void *ptr, size_t size, const char *file, int line);
void _twin_free(void *ptr, const char *file, int line);
#define twin_malloc(sz) _twin_malloc(sz, __FILE__, __LINE__)
#define twin_calloc(n, sz) _twin_calloc(n, sz, __FILE__, __LINE__)
#define twin_realloc(p, sz) _twin_realloc(p, sz, __FILE__, __LINE__)
#define twin_free(p) _twin_free(p, __FILE__, __LINE__)
#else
#define twin_malloc(sz) twin_raw_malloc(sz)
#define twin_calloc(n, sz) twin_raw_calloc(n, sz)
#define twin_realloc(p, sz) twin_raw_realloc(p, sz)
#define twin_free(p) twin_raw_free(p)
#endif

/*
 * Lazy widget extension accessors.
 *
 * The optional fields (callback, callback_data) live in a separately
 * allocated twin_widget_ext_t block. Widgets that never register a
 * callback keep ext == NULL and pay only one pointer.
 */

/* Allocate the extension block if absent.  Returns ext or NULL on OOM. */
static inline twin_widget_ext_t *_twin_widget_ensure_ext(twin_widget_t *w)
{
    if (w->ext)
        return w->ext;
    w->ext = twin_calloc(1, sizeof(twin_widget_ext_t));
    return w->ext;
}

static inline twin_widget_proc_t _twin_widget_callback(const twin_widget_t *w)
{
    return w->ext ? w->ext->callback : NULL;
}

static inline void *_twin_widget_callback_data(const twin_widget_t *w)
{
    return w->ext ? w->ext->callback_data : NULL;
}

#endif /* _TWIN_PRIVATE_H_ */
