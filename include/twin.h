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
typedef uint8_t twin_a8_t;      /* 8-bit alpha channel value */
typedef uint16_t twin_a16_t;    /* 16-bit alpha channel value */
typedef uint16_t twin_rgb16_t;  /* 16-bit RGB color (5-6-5 format) */
typedef uint32_t twin_argb32_t; /* 32-bit ARGB color with alpha */
typedef uint32_t twin_ucs4_t;   /* Unicode UCS-4 character code */
typedef int16_t twin_coord_t;   /* Screen/pixmap coordinate (-32K to 32K) */
typedef int16_t twin_count_t;   /* Count of items/elements */
typedef int16_t twin_keysym_t;  /* Keyboard symbol identifier */
typedef int32_t twin_area_t;    /* Area measurement in pixels */
typedef int32_t twin_time_t;    /* Time value in milliseconds */
typedef int16_t twin_stretch_t; /* Widget stretch factor for layout */
typedef int32_t twin_fixed_t;   /* Fixed-point number (16.16 format) */

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

typedef struct _twin_window twin_window_t;
typedef struct _twin_screen twin_screen_t;
typedef struct _twin_pixmap twin_pixmap_t;
typedef struct _twin_animation twin_animation_t;

/** Button signal types (used in unified event system) */
typedef enum _twin_button_signal {
    TwinButtonSignalDown /**< Sent when button pressed */,
    TwinButtonSignalUp /**< Sent when button released inside widget */
} twin_button_signal_t;

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

    /* Focus and window management */
    TwinEventActivate = 0x0201 /**< Window/widget activated */,
    TwinEventDeactivate = 0x0202 /**< Window/widget deactivated */,
    TwinEventClose = 0x0203 /**< Window manager asks the window to close */,
    TwinEventBack = 0x0204 /**< Back requested: unhandled, the window closes */,

    /* Widgets */
    TwinEventPaint = 0x1001 /**< Widget needs painting */,
    TwinEventQueryGeometry = 0x1002 /**< Widget geometry query */,
    TwinEventConfigure = 0x1003 /**< Widget configuration change */,
    TwinEventDestroy = 0x1004 /**< Widget destruction */,

    /* Button signals (unified with event system) */
    TwinEventButtonSignalDown = 0x2001 /**< Button pressed signal */,
    TwinEventButtonSignalUp = 0x2002 /**< Button released signal */
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
            twin_ucs4_t ucs4; /**< Unicode character */
        } ucs4;               /**< Unicode character event data */
        struct {
            twin_rect_t extents; /**< New widget geometry */
        } configure;             /**< Widget configuration event data */
        struct {
            twin_button_signal_t signal; /**< Button signal type */
        } button_signal;                 /**< Button signal event data */
    } u;                                 /**< Event-specific data union */
} twin_event_t;

/**
 * Window manager policy
 *
 * The window manager is a runtime-switchable policy layered over the
 * compositor.  At most one of the desktop and mobile policies is built
 * (CONFIG_WM_DESKTOP or CONFIG_WM_MOBILE); twin_screen_set_wm() refuses the
 * other.  TwinWmNone is always available: it adds no decorations, covers
 * the screen with application windows, and gives the keyboard to the
 * newest or the last pressed window.
 */
typedef enum _twin_wm_mode {
    TwinWmNone /**< No window management: windows map as requested */,
    TwinWmDesktop /**< Overlapping, decorated, draggable windows */,
    TwinWmMobile /**< Full-screen app stack with transitions */,
} twin_wm_mode_t;

/**
 * Compositor counters
 *
 * Monotonic counters describing how much work the compositor performed.
 * They are deterministic for a given scene and input sequence, which lets
 * headless tests gate on work done rather than wall-clock time.
 */
typedef struct _twin_screen_stats {
    uint32_t frames;          /**< twin_screen_update calls that drew */
    uint32_t rects;           /**< Damage rectangles scanned out */
    uint64_t pixels;          /**< Pixels sent to the backend */
    uint64_t layer_pixels;    /**< Pixels composited from layers */
    uint64_t occluded_pixels; /**< Layer pixels skipped by occlusion */
    uint64_t shadow_pixels;   /**< Pixels touched by procedural shadows */
} twin_screen_stats_t;

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

/* 'double' is a no-no in any shipping code, but useful during development */
#define twin_double_to_fixed(d) ((twin_fixed_t) ((d) * 65536))

/* Multiply, not shift: left-shifting a negative value is undefined in C */
#define twin_int_to_fixed(i) ((twin_fixed_t) (i) * 65536)
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
typedef struct _twin_font twin_font_t;

/**
 * Font style enumeration
 */
typedef enum _twin_style {
    TwinStyleRoman = 0 /**< Regular/normal font weight */,
    TwinStyleBold = 1 /**< Bold font weight */,
    TwinStyleOblique = 2 /**< Italic/oblique font style */,
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
    twin_font_t *font;       /**< Font (NULL selects the default) */
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

#if defined(CONFIG_FONT_MONTSERRAT)
/* Montserrat Medium, Latin-1 printable subset, TrueType outlines */
extern twin_font_t twin_font_montserrat;
#endif

#if defined(CONFIG_FONT_ICONS)
/* UI symbol set (Font Awesome 5 Free Solid, CC BY 4.0) */
extern twin_font_t twin_font_icons;

/* UTF-8 strings for twin_text_draw(..., &twin_font_icons, ...) */
#define TWIN_SYM_MUSIC "\xef\x80\x81"      /* U+F001 */
#define TWIN_SYM_USER "\xef\x80\x87"       /* U+F007 */
#define TWIN_SYM_CHECK "\xef\x80\x8c"      /* U+F00C */
#define TWIN_SYM_SIGNAL "\xef\x80\x92"     /* U+F012 */
#define TWIN_SYM_SETTINGS "\xef\x80\x93"   /* U+F013 */
#define TWIN_SYM_HOME "\xef\x80\x95"       /* U+F015 */
#define TWIN_SYM_CLOCK "\xef\x80\x97"      /* U+F017 */
#define TWIN_SYM_REPEAT "\xef\x80\x9e"     /* U+F01E */
#define TWIN_SYM_REFRESH "\xef\x80\xa1"    /* U+F021 */
#define TWIN_SYM_VOLUME "\xef\x80\xa8"     /* U+F028 */
#define TWIN_SYM_CAMERA "\xef\x80\xb0"     /* U+F030 */
#define TWIN_SYM_LIST "\xef\x80\xba"       /* U+F03A */
#define TWIN_SYM_IMAGE "\xef\x80\xbe"      /* U+F03E */
#define TWIN_SYM_PREV "\xef\x81\x88"       /* U+F048 */
#define TWIN_SYM_PLAY "\xef\x81\x8b"       /* U+F04B */
#define TWIN_SYM_PAUSE "\xef\x81\x8c"      /* U+F04C */
#define TWIN_SYM_NEXT "\xef\x81\x91"       /* U+F051 */
#define TWIN_SYM_LEFT "\xef\x81\x93"       /* U+F053 */
#define TWIN_SYM_RIGHT "\xef\x81\x94"      /* U+F054 */
#define TWIN_SYM_PLUS "\xef\x81\xa7"       /* U+F067 */
#define TWIN_SYM_SHUFFLE "\xef\x81\xb4"    /* U+F074 */
#define TWIN_SYM_MESSAGE "\xef\x81\xb5"    /* U+F075 */
#define TWIN_SYM_PHONE "\xef\x82\x95"      /* U+F095 */
#define TWIN_SYM_GLOBE "\xef\x82\xac"      /* U+F0AC */
#define TWIN_SYM_MAIL "\xef\x83\xa0"       /* U+F0E0 */
#define TWIN_SYM_BELL "\xef\x83\xb3"       /* U+F0F3 */
#define TWIN_SYM_KEYBOARD "\xef\x84\x9c"   /* U+F11C */
#define TWIN_SYM_MIC "\xef\x84\xb0"        /* U+F130 */
#define TWIN_SYM_SUN "\xef\x86\x85"        /* U+F185 */
#define TWIN_SYM_SEND "\xef\x87\x98"       /* U+F1D8 */
#define TWIN_SYM_HISTORY "\xef\x87\x9a"    /* U+F1DA */
#define TWIN_SYM_WIFI "\xef\x87\xab"       /* U+F1EB */
#define TWIN_SYM_CALCULATOR "\xef\x87\xac" /* U+F1EC */
#define TWIN_SYM_BATTERY "\xef\x89\x80"    /* U+F240 */
#define TWIN_SYM_CONTACTS "\xef\x8a\xb9"   /* U+F2B9 */
#define TWIN_SYM_MOBILE "\xef\x8f\x8d"     /* U+F3CD */
#define TWIN_SYM_HANGUP "\xef\x8f\x9d"     /* U+F3DD */
#define TWIN_SYM_BACKSPACE "\xef\x95\x9a"  /* U+F55A */
#endif

/*
 * Windows
 */

/**
 * Window styles describe intent; the active window manager decides geometry.
 *
 *  - Plain:       placed exactly as requested, never decorated (bars, popups)
 *  - Application: a managed app window.  Desktop decorates it at the
 *                 requested geometry; mobile stacks it over the work area;
 *                 none maps it over the whole screen.
 *  - Fullscreen:  covers the whole screen, above system bars, in every mode
 */
typedef enum _twin_window_style {
    TwinWindowPlain /**< Plain window without decorations */,
    TwinWindowApplication /**< Standard application window */,
    TwinWindowFullscreen /**< Undecorated window covering the screen */,
} twin_window_style_t;

typedef void (*twin_draw_func_t)(twin_window_t *window);

typedef bool (*twin_event_func_t)(twin_window_t *window, twin_event_t *event);

typedef void (*twin_destroy_func_t)(twin_window_t *window);

/*
 * Timeout and work procs return true to remain in the queue,
 * timeout procs are called every 'delay' ms
 */
typedef twin_time_t (*twin_timeout_proc_t)(twin_time_t now, void *closure);

typedef bool (*twin_work_proc_t)(void *closure);

typedef struct _twin_timeout twin_timeout_t;
typedef struct _twin_work twin_work_t;

/*
 * Widgets
 */

typedef struct _twin_widget twin_widget_t;
typedef struct _twin_box twin_box_t;

typedef enum _twin_box_dir {
    TwinBoxHorz /**< Horizontal layout */,
    TwinBoxVert /**< Vertical layout */
} twin_box_dir_t;

typedef enum _twin_dispatch_result {
    TwinDispatchDone /**< Event processing complete */,
    TwinDispatchContinue /**< Continue event propagation */,
    TwinDispatchReject /**< Cannot handle this event */
} twin_dispatch_result_t;

/**
 * Widget event handler function signature
 *
 * All widget event handlers use this signature with closure support.
 * The closure parameter enables stateful event handling without global state.
 *
 * @widget  : Widget receiving the event
 * @event   : Event to process
 * @closure : User data pointer for stateful handling
 * @return  : Dispatch result (Done, Continue, or Reject)
 *
 * This type is used for both framework-level handlers and application
 * callbacks.
 */
typedef twin_dispatch_result_t (*twin_widget_proc_t)(twin_widget_t *widget,
                                                     twin_event_t *event,
                                                     void *closure);

typedef enum _twin_shape {
    TwinShapeRectangle /**< Rectangular shape */,
    TwinShapeRoundedRectangle /**< Rectangle with rounded corners */,
    TwinShapeLozenge /**< Lozenge/diamond shape */,
    TwinShapeTab /**< Tab shape */,
    TwinShapeEllipse /**< Elliptical shape */
} twin_shape_t;

typedef struct _twin_toplevel twin_toplevel_t;

typedef enum _twin_align {
    TwinAlignLeft /**< Left alignment */,
    TwinAlignCenter /**< Center alignment */,
    TwinAlignRight /**< Right alignment */
} twin_align_t;

typedef struct _twin_label twin_label_t;

typedef struct _twin_button twin_button_t;

typedef struct _twin_context twin_context_t;

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
 * Set application callback for widget events (low-level API)
 * @widget   : Widget to configure
 * @callback : Application callback function
 * @data     : User data passed to callback
 *
 * Allows applications to respond to widget events (e.g., button clicks).
 * The callback receives events after the widget's core handler processes them.
 */
void twin_widget_set_callback(twin_widget_t *widget,
                              twin_widget_proc_t callback,
                              void *data);

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
 * Backend-specific behavior:
 *   - Calls init_callback (timing varies by backend)
 *   - Enters main event loop (native: infinite loop, WASM: browser event loop)
 *   - Processes events, timeouts, and work queue until termination
 *
 * This is the recommended way to start Twin applications.
 */
void twin_run(twin_context_t *ctx, void (*init_callback)(twin_context_t *));

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

#define twin_fixed_mul(a, b) ((twin_fixed_t) (((int64_t) (a) * (b)) >> 16))
#define twin_fixed_div(a, b) ((twin_fixed_t) (((int64_t) (a) * 65536) / (b)))
#define twin_fixed_abs(f) ((f) < 0 ? -(f) : (f))

/**
 * Check if font contains specified Unicode character
 * @font : Font to check
 * @ucs4 : Unicode character code
 *
 * Return True if character is available in font
 */
bool twin_has_ucs4(twin_font_t *font, twin_ucs4_t ucs4);

/**
 * Add UTF-8 string outline to path
 * @path   : Path to add text to
 * @string : UTF-8 encoded string
 *
 * Converts UTF-8 string to path outlines for text rendering.
 */
void twin_path_utf8(twin_path_t *path, const char *string);

/**
 * Get total width of UTF-8 string
 * @path   : Path with current font settings
 * @string : UTF-8 encoded string
 *
 * Return Total string width in fixed-point format
 */
twin_fixed_t twin_width_utf8(twin_path_t *path, const char *string);

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

twin_pixmap_t *twin_pixmap_from_file(const char *path, twin_format_t fmt);

/*
 * Frame-based animations, as loaded from an animated GIF.  The animation
 * belongs to its pixmap (pixmap->animation): twin_pixmap_destroy() frees it
 * along with every frame.
 */

/* Get the number of milliseconds the current frame should be displayed. */
twin_time_t twin_animation_get_current_delay(const twin_animation_t *anim);

/* Get the current frame which should be displayed. */
twin_pixmap_t *twin_animation_get_current_frame(const twin_animation_t *anim);

/* Advances the animation to the next frame. If the animation is looping, it
 * will return to the first frame after the last one. */
void twin_animation_advance_frame(twin_animation_t *anim);

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

void twin_label_set_align(twin_label_t *label, twin_align_t align);

/** The widget a label or button is built on, for the twin_widget_* calls */
twin_widget_t *twin_label_widget(twin_label_t *label);
twin_widget_t *twin_button_widget(twin_button_t *button);

void twin_matrix_identity(twin_matrix_t *m);

void twin_matrix_scale(twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy);

void twin_matrix_rotate(twin_matrix_t *m, twin_angle_t a);

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

void twin_path_draw(twin_path_t *path, twin_fixed_t x, twin_fixed_t y);

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

void twin_path_close(twin_path_t *path);

void twin_path_empty(twin_path_t *path);

void twin_path_bounds(twin_path_t *path, twin_rect_t *rect);

twin_path_t *twin_path_create(void);

void twin_path_destroy(twin_path_t *path);

void twin_path_translate(twin_path_t *path, twin_fixed_t tx, twin_fixed_t ty);

void twin_path_scale(twin_path_t *path, twin_fixed_t sx, twin_fixed_t sy);

void twin_path_rotate(twin_path_t *path, twin_angle_t a);

void twin_path_set_matrix(twin_path_t *path, twin_matrix_t matrix);

void twin_path_set_font_size(twin_path_t *path, twin_fixed_t font_size);

void twin_path_set_font_style(twin_path_t *path, twin_style_t font_style);

void twin_path_set_cap_style(twin_path_t *path, twin_cap_t cap_style);

twin_state_t twin_path_save(twin_path_t *path);

void twin_path_restore(twin_path_t *path, twin_state_t *state);

void twin_paint_path(twin_pixmap_t *dst, twin_argb32_t argb, twin_path_t *path);

void twin_paint_stroke(twin_pixmap_t *dst,
                       twin_argb32_t argb,
                       twin_path_t *stroke,
                       twin_fixed_t pen_width);

twin_pixmap_t *twin_make_pattern(void);

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
 * Destroy pixmap and free resources
 * @pixmap : Pixmap to destroy
 *
 * Frees pixmap and associated pixel buffer (if allocated).
 */
void twin_pixmap_destroy(twin_pixmap_t *pixmap);

twin_coord_t twin_pixmap_width(const twin_pixmap_t *pixmap);
twin_coord_t twin_pixmap_height(const twin_pixmap_t *pixmap);
twin_format_t twin_pixmap_format(const twin_pixmap_t *pixmap);

/**
 * Direct access to the pixel buffer
 * @pixmap : Pixmap to access
 * @stride : Receives the row stride in bytes (may be NULL)
 *
 * Return the first pixel row, or NULL when the pixmap holds no pixels.
 * Meant for off-screen pixmaps: drawing calls track screen damage, direct
 * writes do not.
 */
void *twin_pixmap_pixels(twin_pixmap_t *pixmap, twin_coord_t *stride);

/** Frames of an animated image, or NULL for a still pixmap */
twin_animation_t *twin_pixmap_animation(const twin_pixmap_t *pixmap);

/**
 * Set the transform applied when the pixmap is a composite source
 * @pixmap : Source pixmap
 * @m      : Destination-to-source matrix, NULL for the identity
 */
void twin_pixmap_set_transform(twin_pixmap_t *pixmap, const twin_matrix_t *m);

void twin_pixmap_show(twin_pixmap_t *pixmap,
                      twin_screen_t *screen,
                      twin_pixmap_t *higher);

void twin_pixmap_hide(twin_pixmap_t *pixmap);

void twin_pixmap_enable_update(twin_pixmap_t *pixmap);

void twin_pixmap_disable_update(twin_pixmap_t *pixmap);

void twin_pixmap_clip(twin_pixmap_t *pixmap,
                      twin_coord_t left,
                      twin_coord_t top,
                      twin_coord_t right,
                      twin_coord_t bottom);

twin_rect_t twin_pixmap_get_clip(twin_pixmap_t *pixmap);

twin_rect_t twin_pixmap_save_clip(twin_pixmap_t *pixmap);

void twin_pixmap_restore_clip(twin_pixmap_t *pixmap, twin_rect_t rect);

void twin_pixmap_reset_clip(twin_pixmap_t *pixmap);

void twin_pixmap_move(twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y);

/* Low level: route keys to @pixmap without telling the window manager.
 * Applications raise and focus a window with twin_window_activate(). */
void twin_screen_set_active(twin_screen_t *screen, twin_pixmap_t *pixmap);

void twin_screen_set_background(twin_screen_t *screen, twin_pixmap_t *pixmap);

void twin_screen_set_cursor(twin_screen_t *screen,
                            twin_pixmap_t *pixmap,
                            twin_fixed_t hotspot_x,
                            twin_fixed_t hotspot_y);

twin_coord_t twin_screen_width(const twin_screen_t *screen);
twin_coord_t twin_screen_height(const twin_screen_t *screen);

/** Compositor work counters since the screen was created */
const twin_screen_stats_t *twin_screen_stats(const twin_screen_t *screen);

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

/** The box that holds a toplevel's widgets */
twin_box_t *twin_toplevel_box(twin_toplevel_t *toplevel);

/*
 * trig.c
 */

twin_fixed_t twin_cos(twin_angle_t a);

void twin_sincos(twin_angle_t a, twin_fixed_t *sin, twin_fixed_t *cos);

/*
 * widget.c
 */

void twin_widget_set(twin_widget_t *widget, twin_argb32_t background);

/** Outline the widget background is painted with */
void twin_widget_set_shape(twin_widget_t *widget, twin_shape_t shape);

/** Make @widget request the same preferred size as @source */
void twin_widget_match_size(twin_widget_t *widget, twin_widget_t *source);

/* Request widget repaint */
void twin_widget_queue_paint(twin_widget_t *widget);

/*
 * Custom widget support - allows creating widgets without accessing internals
 *
 * This API provides a clean abstraction for creating custom widgets that
 * can store private data and implement custom behavior without requiring
 * access to twin_private.h or internal widget structures.
 */

typedef struct _twin_custom_widget twin_custom_widget_t;

/**
 * Create a custom widget with user-defined data and event handler.
 *
 * @parent       : Parent box widget to contain this widget
 * @background   : Background color (ARGB32 format)
 * @width        : Preferred width in pixels (0 for flexible)
 * @height       : Preferred height in pixels (0 for flexible)
 * @hstretch     : Horizontal stretch factor for layout
 * @vstretch     : Vertical stretch factor for layout
 * @handler      : Custom event handler function for this widget
 * @data_size    : Size of custom data to allocate (0 for no data)
 * @return       : Newly created custom widget, or NULL on failure
 *
 * The handler function will be called for all events sent to this widget.
 * Custom data (if requested) is zero-initialized and accessible via
 * twin_custom_widget_data().
 */
twin_custom_widget_t *twin_custom_widget_create(twin_box_t *parent,
                                                twin_argb32_t background,
                                                twin_coord_t width,
                                                twin_coord_t height,
                                                twin_stretch_t hstretch,
                                                twin_stretch_t vstretch,
                                                twin_widget_proc_t handler,
                                                size_t data_size);

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

/*
 * Convenience functions for common custom widget operations.
 * These functions provide simplified access to frequently needed operations
 * without requiring direct manipulation of the base widget.
 */

/** Get the current width of a custom widget, in pixels */
twin_coord_t twin_custom_widget_width(twin_custom_widget_t *custom);

/** Get the current height of a custom widget, in pixels */
twin_coord_t twin_custom_widget_height(twin_custom_widget_t *custom);

/** Request that a custom widget be repainted */
void twin_custom_widget_queue_paint(twin_custom_widget_t *custom);

/** Get the drawing pixmap for a custom widget */
twin_pixmap_t *twin_custom_widget_pixmap(twin_custom_widget_t *custom);

/**
 * Create window with decorations and event handling
 * @screen : Screen to display window on
 * @format : Pixel format for window contents
 * @style  : Window style (plain or application)
 * @x      : Initial X position
 * @y      : Initial Y position
 * @width  : Client area width (decorations added internally)
 * @height : Client area height (decorations added internally)
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

void twin_window_set_name(twin_window_t *window, const char *name);

/** The pixmap a window draws into; it includes any decorations */
twin_pixmap_t *twin_window_pixmap(const twin_window_t *window);

/** The client area in pixmap coordinates */
twin_rect_t twin_window_client(const twin_window_t *window);

twin_window_style_t twin_window_style(const twin_window_t *window);

/** True once twin_window_show() was called and until twin_window_hide() */
bool twin_window_is_mapped(const twin_window_t *window);

/**
 * Install the client callbacks
 * @draw    : Repaints the client area (may be NULL)
 * @event   : Receives input and window events (may be NULL)
 * @destroy : Runs before the window is freed (may be NULL)
 */
void twin_window_set_callbacks(twin_window_t *window,
                               twin_draw_func_t draw,
                               twin_event_func_t event,
                               twin_destroy_func_t destroy);

void twin_window_set_client_data(twin_window_t *window, void *data);
void *twin_window_client_data(const twin_window_t *window);

void twin_window_draw(twin_window_t *window);

void twin_window_damage(twin_window_t *window,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom);

void twin_window_queue_paint(twin_window_t *window);

/**
 * Ask a window to close
 * @window : Window to close
 *
 * Delivers TwinEventClose to the window's event handler.  If the handler
 * does not consume it, the window is destroyed.  Window managers use the
 * same path for title-bar close buttons and back gestures, so applications
 * handle closing in one place.  The mobile manager animates the departure
 * from a detached copy of the pixels, so the window's state may be freed
 * immediately.
 */
void twin_window_close(twin_window_t *window);

/**
 * Ask a window to go back
 * @window : Window to ask
 *
 * Delivers TwinEventBack, so an application with its own history can step
 * back inside the window; if the handler does not consume it, the window
 * is closed with twin_window_close().  The mobile manager sends it for the
 * edge swipe and the Escape key.  Return true if the window handled it.
 */
bool twin_window_back(twin_window_t *window);

/**
 * Raise a mapped window and give it the keyboard, through the active
 * window manager (desktop also restores an iconified window; mobile brings
 * the application back to the top of the stack).  Bars are left alone.
 */
void twin_window_activate(twin_window_t *window);

/**
 * Declare that draw() paints every pixel of the client area
 * @window : Window to update
 * @opaque : true when the client area is always fully opaque
 *
 * Lets the compositor skip whatever lies underneath (occlusion culling).
 * RGB16 windows are always opaque.
 */
void twin_window_set_opaque(twin_window_t *window, bool opaque);

/**
 * Scroll part of a window's client area without repainting it
 * @window : Window to scroll
 * @area   : Region in client coordinates
 * @dx, dy : Displacement in pixels
 *
 * Moves the retained pixels inside @area and adds only the exposed band
 * to the window damage, then queues a paint.  Damage still pending inside
 * @area moves with the pixels.  Update the state that paint() reads (the
 * scroll offset) before calling this.  This is how lists scroll at full
 * speed on CPUs without graphics acceleration.
 */
void twin_window_scroll(twin_window_t *window,
                        twin_rect_t area,
                        twin_coord_t dx,
                        twin_coord_t dy);

/*
 * Window manager
 */

/**
 * Select the window manager policy
 * @screen : Screen to manage
 * @mode   : Policy to activate
 *
 * Existing windows are re-placed under the new policy: decorations are
 * added or removed, app windows are stacked or cascaded, and the pixels
 * of windows the policy keeps hidden are released.
 *
 * Safe to call from an event handler: the switch is then applied once the
 * event has been delivered (twin_screen_get_wm() already reports it), and
 * a later request replaces it.
 *
 * Return false if @mode was compiled out, or if the switch could not be
 * queued (out of memory); the current policy then stays.  If the new
 * policy cannot start (out of memory), the screen falls back to TwinWmNone
 * and false is returned.
 */
bool twin_screen_set_wm(twin_screen_t *screen, twin_wm_mode_t mode);

twin_wm_mode_t twin_screen_get_wm(const twin_screen_t *screen);

/**
 * Register system bars (status bar, navigation bar)
 * @screen : Screen
 * @top    : Plain window pinned above the work area, or NULL
 * @bottom : Plain window pinned below the work area, or NULL
 *
 * The mobile manager shows the bars and shrinks the work area between
 * them by the height of each mapped bar; other managers hide them.  The
 * screen does not own the bars: a bar that is replaced is unmapped, and a
 * destroyed bar unregisters itself.  Resizing a bar with
 * twin_window_configure() re-lays out the applications.
 */
void twin_screen_set_bars(twin_screen_t *screen,
                          twin_window_t *top,
                          twin_window_t *bottom);

/**
 * Reserve rows at the bottom of the work area, e.g. for an on-screen
 * keyboard
 * @screen : Screen
 * @bottom : Rows to reserve above the bottom bar (0 releases them)
 *
 * Application windows are re-laid out (they receive TwinEventConfigure).
 */
void twin_screen_set_inset(twin_screen_t *screen, twin_coord_t bottom);

/**
 * Area available to application windows under the active policy
 */
twin_rect_t twin_screen_work_area(const twin_screen_t *screen);

/**
 * Raise and activate the lowest managed window, cycling through all of
 * them on repeated calls (desktop Alt-Tab, mobile app switching)
 */
void twin_screen_focus_next(twin_screen_t *screen);

/**
 * Walk the screen's windows, bottom to top in stacking order:
 *   for (w = twin_screen_first_window(s); w; w = twin_window_next(w))
 * The list includes hidden and system-bar windows.  Destroying @w inside
 * the loop is allowed when the next window was fetched first.
 */
twin_window_t *twin_screen_first_window(const twin_screen_t *screen);
twin_window_t *twin_window_next(const twin_window_t *window);

/*
 * Compositing attributes
 */

/**
 * Set the constant opacity applied when the pixmap is scanned out
 */
void twin_pixmap_set_opacity(twin_pixmap_t *pixmap, twin_a8_t opacity);

/**
 * Attach a procedural drop shadow
 * @pixmap : Pixmap casting the shadow
 * @dx, dy : Offset of the shadow
 * @blur   : Blur radius in pixels, 0 removes the shadow
 * @alpha  : Peak opacity of the shadow
 */
void twin_pixmap_set_shadow(twin_pixmap_t *pixmap,
                            twin_coord_t dx,
                            twin_coord_t dy,
                            twin_coord_t blur,
                            twin_a8_t alpha);

/*
 * Text
 *
 * Text is drawn glyph by glyph from the coverage cache; each glyph is
 * rasterized once per (font, size) and composited from then on.
 */

/** Select the font used by twin_path_utf8() and its measuring calls */
void twin_path_set_font(twin_path_t *path, twin_font_t *font);

/**
 * Draw UTF-8 text
 * @dst   : Destination pixmap (clip and origin apply)
 * @font  : Font, or NULL for g_twin_font
 * @size  : Font size (em height) in pixels, fixed point
 * @x, y  : Pen position of the baseline, fixed point
 * @color : Premultiplied ARGB color
 * @utf8  : Text to draw
 *
 * Return the advance (pen displacement) in fixed point.
 */
twin_fixed_t twin_text_draw(twin_pixmap_t *dst,
                            twin_font_t *font,
                            twin_fixed_t size,
                            twin_fixed_t x,
                            twin_fixed_t y,
                            twin_argb32_t color,
                            const char *utf8);

/** Advance width of @utf8 as twin_text_draw() would lay it out */
twin_fixed_t twin_text_width(twin_font_t *font,
                             twin_fixed_t size,
                             const char *utf8);

/** Font ascent and descent (both positive) at @size */
void twin_font_extents(twin_font_t *font,
                       twin_fixed_t size,
                       twin_fixed_t *ascent,
                       twin_fixed_t *descent);

/*
 * Gradients
 */

/**
 * Fill a path with a two-stop linear gradient
 * @dst    : Destination pixmap
 * @path   : Shape to fill
 * @x0, y0 : Start point (color @c0), path coordinates
 * @x1, y1 : End point (color @c1)
 *
 * Colors are premultiplied ARGB and are clamped beyond the end points.
 * Vertical gradients cost one solid fill per scanline.
 */
void twin_paint_path_linear(twin_pixmap_t *dst,
                            twin_path_t *path,
                            twin_fixed_t x0,
                            twin_fixed_t y0,
                            twin_argb32_t c0,
                            twin_fixed_t x1,
                            twin_fixed_t y1,
                            twin_argb32_t c1);

/*
 * Coverage cache
 *
 * Rasterized coverage masks are kept in one byte-budgeted LRU shared by
 * glyphs and small filled paths.  Paths are keyed by their device-space
 * outline relative to the pixel grid, so a shape drawn again at another
 * integer position hits the cache.  A budget of 0 disables caching.
 */
typedef struct _twin_cache_stats {
    uint32_t hits;      /**< Lookups served from the cache */
    uint32_t misses;    /**< Lookups that had to rasterize */
    uint32_t evictions; /**< Entries dropped to honor the budget */
    uint32_t entries;   /**< Entries currently resident */
    size_t bytes;       /**< Bytes currently resident */
    size_t peak_bytes;  /**< High-water mark of resident bytes */
    size_t budget;      /**< Configured budget in bytes */
} twin_cache_stats_t;

void twin_cache_set_budget(size_t bytes);
void twin_cache_get_stats(twin_cache_stats_t *stats);
void twin_cache_flush(void);

/*
 * Tweens: time-based property animation
 */

typedef enum _twin_ease {
    TwinEaseLinear,
    TwinEaseOutCubic,
    TwinEaseInOutCubic,
    TwinEaseOutBack,
} twin_ease_t;

typedef struct _twin_tween twin_tween_t;

/**
 * Tween step callback
 * @value   : Eased progress, 0 to TWIN_FIXED_ONE (may overshoot for OutBack)
 * @closure : User data
 *
 * Called once per frame; the final call passes TWIN_FIXED_ONE exactly,
 * after which the tween is freed.  Return false to stop early.
 */
typedef bool (*twin_tween_proc_t)(twin_fixed_t value, void *closure);

/**
 * Start a tween
 * @duration : Length in milliseconds (0 finishes on the first frame)
 * @ease     : Easing curve
 * @proc     : Step callback
 * @closure  : User data
 *
 * Progress is derived from elapsed time, so dropped frames shorten the
 * animation instead of slowing it down.
 */
twin_tween_t *twin_tween_start(twin_time_t duration,
                               twin_ease_t ease,
                               twin_tween_proc_t proc,
                               void *closure);

/** Stop a tween without calling its step callback again */
void twin_tween_cancel(twin_tween_t *tween);

/** Jump a tween to its end state (step callback sees TWIN_FIXED_ONE) */
void twin_tween_finish(twin_tween_t *tween);

/** Apply an easing curve to linear progress @t in [0, TWIN_FIXED_ONE] */
twin_fixed_t twin_ease(twin_ease_t ease, twin_fixed_t t);

/** Monotonic time in milliseconds, the clock used by timeouts and tweens */
twin_time_t twin_now(void);

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

twin_pixmap_t *twin_tvg_to_pixmap_budget(const char *filepath,
                                         twin_format_t fmt,
                                         size_t memory_budget);

/**
 * Memory usage statistics (requires CONFIG_MEMORY_STATS)
 */
typedef struct _twin_memory_info {
    size_t current_bytes; /**< Currently allocated heap bytes */
    size_t peak_bytes;    /**< High-water mark */
    size_t total_allocs;  /**< Lifetime allocation count */
    size_t total_frees;   /**< Lifetime free count */
} twin_memory_info_t;

void twin_memory_get_info(twin_memory_info_t *info);
void twin_memory_reset_peak(void);

twin_context_t *twin_create(int width, int height);

/** The screen a context drives */
twin_screen_t *twin_context_screen(const twin_context_t *ctx);

void twin_destroy(twin_context_t *ctx);

#endif /* _TWIN_H_ */
