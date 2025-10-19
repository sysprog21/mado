/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _TWIN_H_
#define _TWIN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Core data types for twin window system
 *
 * These types provide platform-independent representations for
 * graphics coordinates, colors, and timing values optimized for
 * embedded systems with minimal memory footprint.
 */
typedef uint8_t twin_a8_t;        /* 8-bit alpha channel value */
typedef uint16_t twin_a16_t;      /* 16-bit alpha channel value */
typedef uint16_t twin_rgb16_t;    /* 16-bit RGB color (5-6-5 format) */
typedef uint32_t twin_argb32_t;   /* 32-bit ARGB color with alpha */
typedef uint32_t twin_ucs4_t;     /* Unicode UCS-4 character code */
typedef int16_t twin_coord_t;     /* Screen/pixmap coordinate (-32K to 32K) */
typedef int16_t twin_count_t;     /* Count of items/elements */
typedef int16_t twin_keysym_t;    /* Keyboard symbol identifier */
typedef uint8_t twin_js_number_t; /* Joystick controller number */
typedef int16_t twin_js_value_t;  /* Joystick axis/button value */
typedef int32_t twin_area_t;      /* Area measurement in pixels */
typedef int32_t twin_time_t;      /* Time value in milliseconds */
typedef int16_t twin_stretch_t;   /* Widget stretch factor for layout */
typedef int32_t twin_fixed_t;     /* Fixed-point number (16.16 format) */

/**
 * Pixel format enumeration
 *
 * Defines supported pixel formats for pixmaps and framebuffers.
 * Each format has different memory requirements and color capabilities.
 */
typedef enum {
    TWIN_A8 /**< 8-bit alpha-only (grayscale) */,
    TWIN_RGB16 /**< 16-bit RGB (5-6-5 format, no alpha) */,
    TWIN_ARGB32 /**< 32-bit ARGB with full alpha channel */
} twin_format_t;

#define twin_bytes_per_pixel(format) (1 << (twin_coord_t) (format))

/*
 * Angles
 */

/**
 * Angle representation for rotations and trigonometry
 * Range: -2048 to 2048 representing -180 to 180 degrees
 *
 * Fixed-point angle system optimized for embedded systems without FPU.
 * One full rotation (360 degrees) equals TWIN_ANGLE_360 units.
 */
typedef int16_t twin_angle_t;

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

/**
 * Rectangle definition with inclusive coordinates
 *
 * Represents rectangular regions for clipping, damage tracking,
 * and geometric operations. Coordinates are inclusive on all edges.
 */
typedef struct _twin_rect {
    twin_coord_t left, right; /**< Left/right edge X coordinate */
    twin_coord_t top, bottom; /**< Top/bottom edge Y coordinate */
} twin_rect_t;

/**
 * 2D transformation matrix for graphics operations
 *
 * Represents affine transformations (translate, rotate, scale, shear).
 * Matrix format: [m00 m01] [m10 m11] [m20 m21] where m20,m21 are translation.
 * Uses fixed-point arithmetic for embedded systems without floating-point
 * units.
 */
typedef struct _twin_matrix {
    twin_fixed_t m[3][2]; /**< 3x2 transformation matrix */
} twin_matrix_t;

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

typedef struct _twin_window twin_window_t;
typedef struct _twin_screen twin_screen_t;
typedef struct _twin_pixmap twin_pixmap_t;
typedef struct _twin_animation twin_animation_t;

/**
 * Event type enumeration for input and system events
 *
 * Defines all event types that can be processed by the window system.
 * Events are categorized by input source and functionality.
 */
typedef enum _twin_event_kind {
    /* Mouse */
    TwinEventButtonDown = 0x0001 /**< Mouse button pressed */,
    TwinEventButtonUp = 0x0002 /**< Mouse button released */,
    TwinEventMotion = 0x0003 /**< Mouse movement */,
    TwinEventEnter = 0x0004 /**< Mouse entered widget/window */,
    TwinEventLeave = 0x0005 /**< Mouse left widget/window */,

    /* keyboard */
    TwinEventKeyDown = 0x0101 /**< Keyboard key pressed */,
    TwinEventKeyUp = 0x0102 /**< Keyboard key released */,
    TwinEventUcs4 = 0x0103 /**< Unicode character input */,

    /* Focus */
    TwinEventActivate = 0x0201 /**< Window/widget activated */,
    TwinEventDeactivate = 0x0202 /**< Window/widget deactivated */,

    /* Joystick */
    TwinEventJoyButton = 0x0401 /**< Joystick button event */,
    TwinEventJoyAxis = 0x0402 /**< Joystick axis movement */,

    /* Widgets */
    TwinEventPaint = 0x1001 /**< Widget needs painting */,
    TwinEventQueryGeometry = 0x1002 /**< Widget geometry query */,
    TwinEventConfigure = 0x1003 /**< Widget configuration change */,
    TwinEventDestroy = 0x1004 /**< Widget destruction */
} twin_event_kind_t;

/**
 * Event structure containing event data
 *
 * Universal event structure that carries all types of input events.
 * The union contains different data structures depending on event type.
 */
typedef struct _twin_event {
    twin_event_kind_t kind; /**< Event type */
    union {
        struct {
            twin_coord_t x, y;               /**< Widget-relative coordinates */
            twin_coord_t screen_x, screen_y; /**< Screen-absolute coordinates */
            twin_count_t button;             /**< Mouse button number */
        } pointer;                           /**< Pointer/mouse event data */
        struct {
            twin_keysym_t key; /**< Keyboard key symbol */
        } key;                 /**< Keyboard event data */
        struct {
            twin_js_number_t control; /**< Joystick controller ID */
            twin_js_value_t value;    /**< Axis value or button state */
        } js;                         /**< Joystick event data */
        struct {
            twin_ucs4_t ucs4; /**< Unicode character */
        } ucs4;               /**< Unicode character event data */
        struct {
            twin_rect_t extents; /**< New widget geometry */
        } configure;             /**< Widget configuration event data */
    } u;                         /**< Event-specific data union */
} twin_event_t;

/**
 * Event queue node for event processing
 *
 * Linked list node used to queue events for asynchronous processing.
 * Events are processed in FIFO order.
 */
typedef struct _twin_event_queue {
    struct _twin_event_queue *next; /**< Next event in queue */
    twin_event_t event;             /**< Event data */
} twin_event_queue_t;

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
typedef struct _twin_animation {
    twin_pixmap_t **frames;      /**< Array of frame pixmaps */
    twin_count_t n_frames;       /**< Number of frames */
    twin_time_t *frame_delays;   /**< Per-frame timing in milliseconds */
    bool loop;                   /**< Loop animation flag */
    twin_animation_iter_t *iter; /**< Playback iterator */
    twin_coord_t width, height;  /**< Animation dimensions in pixels */
} twin_animation_t;

/**
 * Pixmap structure representing a rectangular array of pixels
 *
 * Core structure representing drawable surfaces with pixels.
 * Can be displayed on screen or used as off-screen render targets.
 */
typedef struct _twin_pixmap {
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
    twin_pointer_t p;            /**< Pixel data pointer */

#if defined(CONFIG_DROP_SHADOW)
    bool shadow; /**< Drop shadow for active windows */
#endif

    twin_window_t *window; /**< Associated window (if any) */

    /* Transform buffer cache for compositing operations */
    void *xform_cache;       /**< Cached xform buffer */
    size_t xform_cache_size; /**< Cached xform buffer size in bytes */
} twin_pixmap_t;

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
    twin_coord_t width, height; /**< Screen dimensions */
    twin_pixmap_t *background;  /**< Background pattern */

    /* Damage tracking */
    twin_rect_t damage;      /**< Current damage rectangle */
    void (*damaged)(void *); /**< Damage notification callback */
    void *damaged_closure;   /**< Damage callback closure */
    twin_count_t disable;    /**< Update disable counter */

    /* Backend interface */
    twin_put_begin_t put_begin; /**< Scanline begin callback */
    twin_put_span_t put_span;   /**< Scanline drawing callback */
    void *closure;              /**< Backend user data */

    /* Window manager */
    twin_coord_t button_x, button_y; /**< Window button position */

    /* Span buffer cache for screen updates */
    twin_argb32_t *span_cache;     /**< Cached span buffer */
    twin_coord_t span_cache_width; /**< Cached span buffer width */

    /* Event processing: event filter callback */
    bool (*event_filter)(twin_screen_t *screen, twin_event_t *event);
};

/**
 * Source operand type for drawing operations
 */
typedef enum {
    TWIN_SOLID /**< Solid color source */,
    TWIN_PIXMAP /**< Pixmap texture source */
} twin_source_t;

/**
 * Drawing operand containing source data
 *
 * Represents a source for drawing operations, either a solid color
 * or a pixmap texture that can be composited onto destinations.
 */
typedef struct _twin_operand {
    twin_source_t source_kind; /**< Type of source */
    union {
        twin_pixmap_t *pixmap; /**< Source pixmap for texture operations */
        twin_argb32_t argb;    /**< Solid color for fill operations */
    } u;                       /**< Source data union */
} twin_operand_t;

/**
 * Compositing operator for blending operations
 */
typedef enum {
    TWIN_OVER /**< Alpha-blend source over destination */,
    TWIN_SOURCE /**< Replace destination with source (no blending) */
} twin_operator_t;

#define TWIN_FIXED_ONE (0x10000)
#define TWIN_FIXED_HALF (0x08000)
#define TWIN_FIXED_MAX (0x7fffffff)
#define TWIN_FIXED_MIN (-0x7fffffff)

/* 'double' is a no-no in any shipping code, but useful during development */
#define twin_double_to_fixed(d) ((twin_fixed_t) ((d) * 65536))
#define twin_fixed_to_double(f) ((double) (f) / 65536.0)

#define twin_int_to_fixed(i) ((twin_fixed_t) (i) << 16)
#define twin_fixed_ceil(f) (((f) + 0xffff) & ~0xffff)
#define twin_fixed_floor(f) ((f) & ~0xffff)
#define twin_fixed_to_int(f) ((int) ((f) >> 16))

/**
 * 2D point with fixed-point coordinates
 *
 * Represents precise 2D coordinates using fixed-point arithmetic
 * for sub-pixel accuracy in graphics operations.
 */
typedef struct _twin_point {
    twin_fixed_t x, y; /**< Fixed-point coordinates */
} twin_point_t;

typedef struct _twin_path twin_path_t;

/**
 * Font style enumeration
 */
typedef enum _twin_style {
    TwinStyleRoman = 0 /**< Regular/normal font weight */,
    TwinStyleBold = 1 /**< Bold font weight */,
    TwinStyleOblique = 2 /**< Italic/oblique font style */,
    TwinStyleBoldOblique = 3 /**< Bold italic combination */,
    TwinStyleUnhinted = 4 /**< Disable font hinting */,
} twin_style_t;

/**
 * Line cap style for path stroking
 */
typedef enum _twin_cap {
    TwinCapRound /**< Rounded line endings */,
    TwinCapButt /**< Square line endings flush with path */,
    TwinCapProjecting /**< Square line endings extending beyond path */,
} twin_cap_t;

/**
 * Graphics state for drawing operations
 *
 * Encapsulates the current graphics context state that can be
 * saved and restored during drawing operations.
 */
typedef struct _twin_state {
    twin_matrix_t matrix;    /**< Current transformation */
    twin_fixed_t font_size;  /**< Font size */
    twin_style_t font_style; /**< Font style */
    twin_cap_t cap_style;    /**< Line cap style */
} twin_state_t;

/**
 * Text metrics for glyph and string measurements
 *
 * Provides detailed typography metrics for text layout and positioning.
 * All measurements use fixed-point format for sub-pixel precision.
 */
typedef struct _twin_text_metrics {
    twin_fixed_t left_side_bearing;  /**< Left bearing */
    twin_fixed_t right_side_bearing; /**< Right bearing */
    twin_fixed_t ascent;             /**< Glyph ascent */
    twin_fixed_t descent;            /**< Glyph descent */
    twin_fixed_t width;              /**< Advance width */
    twin_fixed_t font_ascent;        /**< Font ascent */
    twin_fixed_t font_descent;       /**< Font descent */
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
    unsigned int page;                  /**< Unicode page number */
    unsigned int offsets[UCS_PER_PAGE]; /**< Character offsets in page */
} twin_charmap_t;

#define TWIN_FONT_TYPE_STROKE 1
#define TWIN_FONT_TYPE_TTF 2

typedef struct _twin_font {
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
} twin_font_t;

/* FIXME: one global font for now */
extern twin_font_t *g_twin_font;

/* Built-in default stroke font */
extern twin_font_t twin_Default_Font_Roman;

/*
 * Windows
 */

typedef enum _twin_window_style {
    TwinWindowPlain /**< Plain window without decorations */,
    TwinWindowApplication /**< Standard application window */,
    TwinWindowFullScreen /**< Full screen window */,
    TwinWindowDialog /**< Dialog window */,
    TwinWindowAlert /**< Alert/popup window */
} twin_window_style_t;

typedef void (*twin_draw_func_t)(twin_window_t *window);

typedef bool (*twin_event_func_t)(twin_window_t *window, twin_event_t *event);

typedef void (*twin_destroy_func_t)(twin_window_t *window);

struct _twin_window {
    /* Window basics */
    twin_screen_t *screen; /**< Parent screen */
    twin_pixmap_t *pixmap; /**< Window pixmap */

#if defined(CONFIG_DROP_SHADOW)
    twin_coord_t shadow_x, shadow_y; /**< Shadow offset */
#endif

    /* Window properties */
    twin_window_style_t style; /**< Window style */
    twin_rect_t client;        /**< Client area rectangle */
    twin_rect_t damage;        /**< Damage rectangle */

    /* Window state */
    bool active;      /**< Active window flag */
    bool iconify;     /**< Iconified state */
    bool client_grab; /**< Mouse grab state */
    bool want_focus;  /**< Focus request flag */
    bool draw_queued; /**< Draw pending flag */

    /* Window data */
    void *client_data; /**< User data pointer */
    char *name;        /**< Window title */

    /* Window callbacks */
    twin_draw_func_t draw;       /**< Draw callback */
    twin_event_func_t event;     /**< Event handler */
    twin_destroy_func_t destroy; /**< Destroy callback */
};

/*
 * Icons
 */

typedef enum _twin_icon {
    TwinIconMenu,
    TwinIconIconify,
    TwinIconRestore,
    TwinIconClose,
    TwinIconResize,
} twin_icon_t;

/*
 * Timeout and work procs return true to remain in the queue,
 * timeout procs are called every 'delay' ms
 */
typedef twin_time_t (*twin_timeout_proc_t)(twin_time_t now, void *closure);

typedef bool (*twin_work_proc_t)(void *closure);

#define twin_time_compare(a, op, b) (((a) - (b)) op 0)

typedef struct _twin_timeout twin_timeout_t;
typedef struct _twin_work twin_work_t;

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

typedef enum _twin_box_dir {
    TwinBoxHorz /**< Horizontal layout */,
    TwinBoxVert /**< Vertical layout */
} twin_box_dir_t;

typedef enum _twin_dispatch_result {
    TwinDispatchDone /**< Event processing complete */,
    TwinDispatchContinue /**< Continue event propagation */
} twin_dispatch_result_t;

typedef twin_dispatch_result_t (*twin_dispatch_proc_t)(twin_widget_t *widget,
                                                       twin_event_t *event);

typedef struct _twin_widget_layout {
    twin_coord_t width, height;    /**< Preferred dimensions */
    twin_stretch_t stretch_width;  /**< Horizontal stretch factor */
    twin_stretch_t stretch_height; /**< Vertical stretch factor */
} twin_widget_layout_t;

typedef enum _twin_shape {
    TwinShapeRectangle /**< Rectangular shape */,
    TwinShapeRoundedRectangle /**< Rectangle with rounded corners */,
    TwinShapeLozenge /**< Lozenge/diamond shape */,
    TwinShapeTab /**< Tab shape */,
    TwinShapeEllipse /**< Elliptical shape */
} twin_shape_t;

struct _twin_widget {
    /* Widget hierarchy */
    twin_window_t *window; /**< Parent window */
    twin_widget_t *next;   /**< Next sibling widget */
    twin_box_t *parent;    /**< Parent container */

    /* Widget behavior */
    twin_dispatch_proc_t dispatch; /**< Event dispatch handler */
    twin_rect_t extents;           /**< Current geometry */
    twin_widget_t *copy_geom;      /**< Geometry source widget */

    /* Widget state */
    bool paint;      /**< Needs painting */
    bool layout;     /**< Needs layout */
    bool want_focus; /**< Focus request flag */

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
    twin_widget_t *focus;       /**< Focused child */
};

typedef struct _twin_toplevel {
    twin_box_t box; /**< Base box container */
} twin_toplevel_t;

typedef enum _twin_align {
    TwinAlignLeft /**< Left alignment */,
    TwinAlignCenter /**< Center alignment */,
    TwinAlignRight /**< Right alignment */
} twin_align_t;

typedef struct _twin_label {
    twin_widget_t widget;     /**< Base widget */
    char *label;              /**< Label text */
    twin_argb32_t foreground; /**< Text color */
    twin_fixed_t font_size;   /**< Font size */
    twin_style_t font_style;  /**< Font style */
    twin_point_t offset;      /**< Text offset */
    twin_align_t align;       /**< Text alignment */
} twin_label_t;

typedef enum _twin_button_signal {
    TwinButtonSignalDown /**< Sent when button pressed */,
    TwinButtonSignalUp /**< Sent when button released inside widget */
} twin_button_signal_t;

typedef struct _twin_button twin_button_t;

typedef void (*twin_button_signal_proc_t)(twin_button_t *button,
                                          twin_button_signal_t signal,
                                          void *closure);

struct _twin_button {
    twin_label_t label;               /**< Base label widget */
    bool pressed;                     /**< Button pressed state */
    bool active;                      /**< Button active state */
    twin_button_signal_proc_t signal; /**< Signal callback */
    void *closure;                    /**< Callback closure */
};

typedef enum _twin_scroll_signal {
    TwinScrollSignalUpArrow /**< Up arrow clicked */,
    TwinScrollSignalDownArrow /**< Down arrow clicked */,
    TwinScrollSignalThumb /**< Thumb/slider clicked */,
    TwinScrollSignalAboveThumb /**< Area above thumb clicked */,
    TwinScrollSignalBelowThumb /**< Area below thumb clicked */
} twin_scroll_signal_t;

typedef struct _twin_scroll twin_scroll_t;

typedef void (*twin_scroll_signal_proc_t)(twin_scroll_t *scroll,
                                          twin_scroll_signal_t signal,
                                          void *closure);

struct _twin_scroll {
    twin_widget_t widget; /**< Base widget */
};

typedef struct _twin_context {
    twin_screen_t *screen; /**< Screen context */
    void *priv;            /**< Private backend data */
} twin_context_t;

/**
 * Create a container widget for organizing child widgets
 * @parent : Parent box to contain this box (NULL for root)
 * @dir    : Layout direction (horizontal or vertical)
 *
 * Return Newly created box widget, or NULL on failure
 */
twin_box_t *twin_box_create(twin_box_t *parent, twin_box_dir_t dir);

/**
 * Create a clickable button widget with text label
 * @parent     : Parent box container
 * @value      : Button text (copied internally)
 * @foreground : Text color in ARGB32 format
 * @font_size  : Text size in fixed-point format
 * @font_style : Text style (bold, italic, etc.)
 *
 * Return Newly created button widget, or NULL on failure
 */
twin_button_t *twin_button_create(twin_box_t *parent,
                                  const char *value,
                                  twin_argb32_t foreground,
                                  twin_fixed_t font_size,
                                  twin_style_t font_style);

/**
 * Create default mouse cursor pixmap
 * @hx : Output hotspot X coordinate
 * @hy : Output hotspot Y coordinate
 *
 * Return Cursor pixmap with arrow design, or NULL on failure
 */
twin_pixmap_t *twin_make_cursor(int *hx, int *hy);

/**
 * Start the Twin application with initialization callback
 * @ctx           : Twin context containing screen and backend data
 * @init_callback : Application initialization function (called once before
 * event loop)
 *
 * Unified application startup function that handles platform differences
 * internally.
 *
 * For native builds:
 *   - Calls init_callback immediately
 *   - Enters infinite event loop via twin_dispatch()
 *
 * For WebAssembly builds:
 *   - Sets up browser-compatible event loop
 *   - Defers init_callback to first iteration (works around SDL timing issues)
 *
 * This is the recommended way to start Twin applications.
 */
void twin_run(twin_context_t *ctx, void (*init_callback)(twin_context_t *));

/**
 * Process pending events and work items
 * @ctx : Twin context containing screen and backend data
 *
 * Main event loop function that processes input events,
 * executes work procedures, and handles timeouts.
 *
 * For native builds, this runs an infinite loop until termination.
 * For WebAssembly builds, use twin_run() instead.
 */
void twin_dispatch(twin_context_t *ctx);

/**
 * Composite source onto destination with optional mask
 * @dst      : Destination pixmap
 * @dst_x    : Destination X offset
 * @dst_y    : Destination Y offset
 * @src      : Source operand (color or pixmap)
 * @src_x    : Source X offset
 * @src_y    : Source Y offset
 * @msk      : Optional mask operand (NULL for no mask)
 * @msk_x    : Mask X offset
 * @msk_y    : Mask Y offset
 * @operator : Compositing operation (OVER or SOURCE)
 * @width    : Width of operation in pixels
 * @height   : Height of operation in pixels
 *
 * Core compositing function for blitting images and drawing operations.
 * Essential for animation playback and image display.
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

/**
 * Fill rectangular region with solid color
 * @dst      : Destination pixmap
 * @pixel    : Fill color in ARGB32 format
 * @operator : Compositing operation (OVER or SOURCE)
 * @left     : Left edge of fill rectangle
 * @top      : Top edge of fill rectangle
 * @right    : Right edge of fill rectangle (inclusive)
 * @bottom   : Bottom edge of fill rectangle (inclusive)
 *
 * High-performance rectangle fill operation for backgrounds
 * and solid color regions.
 */
void twin_fill(twin_pixmap_t *dst,
               twin_argb32_t pixel,
               twin_operator_t operator,
               twin_coord_t left,
               twin_coord_t top,
               twin_coord_t right,
               twin_coord_t bottom);

/**
 * Apply stack blur effect to rectangular region
 * @px     : Pixmap to blur (must be TWIN_ARGB32 format)
 * @radius : Blur radius (larger values create stronger blur)
 * @left   : Left edge of blur region
 * @right  : Right edge of blur region (inclusive)
 * @top    : Top edge of blur region
 * @bottom : Bottom edge of blur region (inclusive)
 *
 * Applies a fast stack blur algorithm to create soft shadows,
 * gaussian-like blur effects, and smooth transitions. The blur
 * only affects the specified rectangular region.
 */
void twin_stack_blur(twin_pixmap_t *px,
                     int radius,
                     twin_coord_t left,
                     twin_coord_t right,
                     twin_coord_t top,
                     twin_coord_t bottom);

/**
 * Add event to processing queue
 * @event : Event to be queued for processing
 *
 * Enqueues events for asynchronous processing by the dispatch loop.
 * Events are processed in FIFO order.
 */

#define twin_fixed_mul(a, b) ((twin_fixed_t) (((int64_t) (a) * (b)) >> 16))
#define twin_fixed_div(a, b) ((twin_fixed_t) ((((int64_t) (a)) << 16) / (b)))
#define twin_fixed_abs(f) ((f) < 0 ? -(f) : (f))

/**
 * Calculate square root of fixed-point number
 * @a : Input value in fixed-point format
 *
 * Return Square root in fixed-point format
 */
twin_fixed_t twin_fixed_sqrt(twin_fixed_t a);

/**
 * Check if font contains specified Unicode character
 * @font : Font to check
 * @ucs4 : Unicode character code
 *
 * Return True if character is available in font
 */
bool twin_has_ucs4(twin_font_t *font, twin_ucs4_t ucs4);

/**
 * Add stroked outline of Unicode character to path
 * @path : Path to add character outline to
 * @ucs4 : Unicode character to add
 *
 * Creates outlined/stroked version of character for unfilled text.
 */
void twin_path_ucs4_stroke(twin_path_t *path, twin_ucs4_t ucs4);

/**
 * Add filled outline of Unicode character to path
 * @path : Path to add character to
 * @ucs4 : Unicode character to add
 *
 * Adds character outline to path for filled text rendering.
 */
void twin_path_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

/**
 * Add UTF-8 string outline to path
 * @path   : Path to add text to
 * @string : UTF-8 encoded string
 *
 * Converts UTF-8 string to path outlines for text rendering.
 */
void twin_path_utf8(twin_path_t *path, const char *string);

/**
 * Get advance width of Unicode character
 * @path : Path with current font settings
 * @ucs4 : Unicode character
 *
 * Return Character advance width in fixed-point format
 */
twin_fixed_t twin_width_ucs4(twin_path_t *path, twin_ucs4_t ucs4);

/**
 * Get total width of UTF-8 string
 * @path   : Path with current font settings
 * @string : UTF-8 encoded string
 *
 * Return Total string width in fixed-point format
 */
twin_fixed_t twin_width_utf8(twin_path_t *path, const char *string);

/**
 * Get detailed metrics for Unicode character
 * @path : Path with current font settings
 * @ucs4 : Unicode character
 * @m    : Output metrics structure
 *
 * Fills metrics structure with detailed typography information.
 */
void twin_text_metrics_ucs4(twin_path_t *path,
                            twin_ucs4_t ucs4,
                            twin_text_metrics_t *m);

/**
 * Get detailed metrics for UTF-8 string
 * @path   : Path with current font settings
 * @string : UTF-8 encoded string
 * @m      : Output metrics structure
 *
 * Fills metrics structure with combined string typography information.
 */
void twin_text_metrics_utf8(twin_path_t *path,
                            const char *string,
                            twin_text_metrics_t *m);

void twin_icon_draw(twin_pixmap_t *pixmap,
                    twin_icon_t icon,
                    twin_matrix_t matrix);

twin_pixmap_t *twin_pixmap_from_file(const char *path, twin_format_t fmt);

/*
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
 * frames.
 */
void twin_animation_destroy(twin_animation_t *anim);

twin_animation_iter_t *twin_animation_iter_init(twin_animation_t *anim);

void twin_animation_iter_advance(twin_animation_iter_t *iter);

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

void twin_matrix_identity(twin_matrix_t *m);

bool twin_matrix_is_identity(twin_matrix_t *m);

void twin_matrix_translate(twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty);

void twin_matrix_scale(twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy);

void twin_matrix_rotate(twin_matrix_t *m, twin_angle_t a);

void twin_matrix_multiply(twin_matrix_t *result,
                          const twin_matrix_t *a,
                          const twin_matrix_t *b);

/**
 * Transform coordinates using a transformation matrix.
 *
 * These functions apply a 2D transformation matrix to convert coordinates
 * from one coordinate system to another. Commonly used for rotations,
 * scaling, and translations in graphics operations.
 */

/**
 * Transform the X coordinate using the given matrix.
 * @m      : Transformation matrix to apply
 * @x      : Input X coordinate in fixed-point format
 * @y      : Input Y coordinate in fixed-point format
 * @return : Transformed X coordinate in fixed-point format
 *
 * Applies the matrix transformation to compute the new X coordinate.
 * Both input coordinates are required as 2D transformations can
 * affect both X and Y components.
 */
twin_fixed_t twin_matrix_transform_x(const twin_matrix_t *m,
                                     twin_fixed_t x,
                                     twin_fixed_t y);

/**
 * Transform the Y coordinate using the given matrix.
 * @m      : Transformation matrix to apply
 * @x      : Input X coordinate in fixed-point format
 * @y      : Input Y coordinate in fixed-point format
 * @return : Transformed Y coordinate in fixed-point format
 *
 * Applies the matrix transformation to compute the new Y coordinate.
 * Both input coordinates are required as 2D transformations can
 * affect both X and Y components.
 */
twin_fixed_t twin_matrix_transform_y(const twin_matrix_t *m,
                                     twin_fixed_t x,
                                     twin_fixed_t y);

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

void twin_path_arc_ellipse(twin_path_t *path,
                           bool large_arc,
                           bool sweep,
                           twin_fixed_t radius_x,
                           twin_fixed_t radius_y,
                           twin_fixed_t cur_x,
                           twin_fixed_t cur_y,
                           twin_fixed_t target_x,
                           twin_fixed_t target_y,
                           twin_angle_t rotation);

void twin_path_arc_circle(twin_path_t *path,
                          bool large_arc,
                          bool sweep,
                          twin_fixed_t radius,
                          twin_fixed_t cur_x,
                          twin_fixed_t cur_y,
                          twin_fixed_t target_x,
                          twin_fixed_t target_y);

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

void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path);


void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width);

twin_pixmap_t *twin_make_pattern(void);

#define twin_pixmap_is_animated(pix) ((pix)->animation != NULL)

/**
 * Create pixmap with allocated pixel buffer
 * @format : Pixel format (A8, RGB16, or ARGB32)
 * @width  : Width in pixels
 * @height : Height in pixels
 *
 * Return Newly created pixmap with allocated pixel buffer, or NULL on failure
 */
twin_pixmap_t *twin_pixmap_create(twin_format_t format,
                                  twin_coord_t width,
                                  twin_coord_t height);

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

/**
 * Destroy pixmap and free resources
 * @pixmap : Pixmap to destroy
 *
 * Frees pixmap and associated pixel buffer (if allocated).
 */
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



void twin_pixmap_move(twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y);


bool twin_pixmap_transparent(twin_pixmap_t *pixmap,
                             twin_coord_t x,
                             twin_coord_t y);

bool twin_pixmap_is_iconified(twin_pixmap_t *pixmap, twin_coord_t y);

bool twin_pixmap_dispatch(twin_pixmap_t *pixmap, twin_event_t *event);

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

void twin_screen_set_active(twin_screen_t *screen, twin_pixmap_t *pixmap);

twin_pixmap_t *twin_screen_get_active(twin_screen_t *screen);

void twin_screen_set_background(twin_screen_t *screen, twin_pixmap_t *pixmap);

twin_pixmap_t *twin_screen_get_background(twin_screen_t *screen);

void twin_screen_set_cursor(twin_screen_t *screen,
                            twin_pixmap_t *pixmap,
                            twin_fixed_t hotspot_x,
                            twin_fixed_t hotspot_y);

bool twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event);



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

void twin_path_quadratic_curve(twin_path_t *path,
                               twin_fixed_t x1,
                               twin_fixed_t y1,
                               twin_fixed_t x2,
                               twin_fixed_t y2);

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

twin_angle_t twin_atan2(twin_fixed_t y, twin_fixed_t x);

twin_angle_t twin_acos(twin_fixed_t x);

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

/* Get widget dimensions */
twin_fixed_t twin_widget_width(twin_widget_t *widget);
twin_fixed_t twin_widget_height(twin_widget_t *widget);

/* Request widget repaint */
void twin_widget_queue_paint(twin_widget_t *widget);

/* Create widget with custom dispatch handler */
twin_widget_t *twin_widget_create_with_dispatch(twin_box_t *parent,
                                                twin_argb32_t background,
                                                twin_coord_t width,
                                                twin_coord_t height,
                                                twin_stretch_t hstretch,
                                                twin_stretch_t vstretch,
                                                twin_dispatch_proc_t dispatch);

/*
 * Custom widget support - allows creating widgets without accessing internals
 *
 * This API provides a clean abstraction for creating custom widgets that
 * can store private data and implement custom behavior without requiring
 * access to twin_private.h or internal widget structures.
 */

/**
 * Structure representing a custom widget with user data.
 * Encapsulates a base widget and allows attaching custom data.
 */
typedef struct {
    twin_widget_t *widget; /**< Base widget functionality */
    void *data;            /**< User-defined data */
} twin_custom_widget_t;

/**
 * Create a custom widget with user-defined data and dispatch handler.
 *
 * @parent       : Parent box widget to contain this widget
 * @background   : Background color (ARGB32 format)
 * @width        : Preferred width in pixels (0 for flexible)
 * @height       : Preferred height in pixels (0 for flexible)
 * @hstretch     : Horizontal stretch factor for layout
 * @vstretch     : Vertical stretch factor for layout
 * @dispatch     : Custom event dispatch function for this widget
 * @data_size    : Size of custom data to allocate (0 for no data)
 * @return       : Newly created custom widget, or NULL on failure
 *
 * The dispatch function will be called for all events sent to this widget.
 * Custom data (if requested) is zero-initialized and accessible via
 * twin_custom_widget_data().
 */
twin_custom_widget_t *twin_custom_widget_create(twin_box_t *parent,
                                                twin_argb32_t background,
                                                twin_coord_t width,
                                                twin_coord_t height,
                                                twin_stretch_t hstretch,
                                                twin_stretch_t vstretch,
                                                twin_dispatch_proc_t dispatch,
                                                size_t data_size);

/**
 * Get the drawing pixmap from a widget for rendering operations.
 *
 * @widget : Widget to get pixmap from
 * @return : Widget's pixmap for drawing, or NULL if invalid
 */
twin_pixmap_t *twin_widget_pixmap(twin_widget_t *widget);

/**
 * Retrieve the custom widget wrapper from a base widget.
 *
 * @widget : Base widget to look up
 * @return : Associated custom widget, or NULL if not found
 *
 * This function allows event handlers to retrieve their custom widget
 * context when receiving events through the base widget dispatch system.
 */
twin_custom_widget_t *twin_widget_get_custom(twin_widget_t *widget);

/**
 * Get the user data pointer from a custom widget.
 *
 * @custom : Custom widget to get data from
 * @return : Pointer to user data, or NULL if no data allocated
 *
 * Return The user data allocated during twin_custom_widget_create().
 * The returned pointer should be cast to the appropriate data structure type.
 */
void *twin_custom_widget_data(twin_custom_widget_t *custom);

/**
 * Get the base widget from a custom widget.
 *
 * @custom : Custom widget to get base widget from
 * @return : Base widget, or NULL if invalid
 *
 * Provides access to the underlying widget for operations that require
 * the base widget interface.
 */
twin_widget_t *twin_custom_widget_base(twin_custom_widget_t *custom);

/*
 * Convenience functions for common custom widget operations.
 * These functions provide simplified access to frequently needed operations
 * without requiring direct manipulation of the base widget.
 */

/** Get the current width of a custom widget */
twin_fixed_t twin_custom_widget_width(twin_custom_widget_t *custom);

/** Get the current height of a custom widget */
twin_fixed_t twin_custom_widget_height(twin_custom_widget_t *custom);

/** Request that a custom widget be repainted */
void twin_custom_widget_queue_paint(twin_custom_widget_t *custom);

/** Get the drawing pixmap for a custom widget */
twin_pixmap_t *twin_custom_widget_pixmap(twin_custom_widget_t *custom);

/**
 * Create window with decorations and event handling
 * @screen : Screen to display window on
 * @format : Pixel format for window contents
 * @style  : Window style (plain, application, fullscreen, etc.)
 * @x      : Initial X position
 * @y      : Initial Y position
 * @width  : Window width including decorations
 * @height : Window height including decorations
 *
 * Return Newly created window, or NULL on failure
 */
twin_window_t *twin_window_create(twin_screen_t *screen,
                                  twin_format_t format,
                                  twin_window_style_t style,
                                  twin_coord_t x,
                                  twin_coord_t y,
                                  twin_coord_t width,
                                  twin_coord_t height);

/**
 * Destroy window and free resources
 * @window : Window to destroy
 *
 * Removes window from screen and frees all associated resources.
 */
void twin_window_destroy(twin_window_t *window);

void twin_window_show(twin_window_t *window);

void twin_window_hide(twin_window_t *window);

void twin_window_configure(twin_window_t *window,
                           twin_window_style_t style,
                           twin_coord_t x,
                           twin_coord_t y,
                           twin_coord_t width,
                           twin_coord_t height);

/* Check if the coordinates are within the window's range. */
bool twin_window_valid_range(twin_window_t *window,
                             twin_coord_t x,
                             twin_coord_t y);

void twin_window_style_size(twin_window_style_t style, twin_rect_t *size);

void twin_window_set_name(twin_window_t *window, const char *name);

void twin_window_draw(twin_window_t *window);

void twin_window_damage(twin_window_t *window,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_window_queue_paint(twin_window_t *window);

bool twin_window_dispatch(twin_window_t *window, twin_event_t *event);

#define TWIN_WORK_REDISPLAY 0
#define TWIN_WORK_PAINT 1
#define TWIN_WORK_LAYOUT 2

twin_work_t *twin_set_work(twin_work_proc_t work_proc,
                           int priority,
                           void *closure);

void twin_clear_work(twin_work_t *work);

twin_pixmap_t *twin_tvg_to_pixmap_scale(const char *filepath,
                                        twin_format_t fmt,
                                        twin_coord_t w,
                                        twin_coord_t h);

twin_context_t *twin_create(int width, int height);

void twin_destroy(twin_context_t *ctx);

#endif /* _TWIN_H_ */
