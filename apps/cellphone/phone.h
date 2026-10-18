/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone demo: shell and UI kit
 *
 * Every screen is a "view": one twin window holding the screen's pixels,
 * stacked by the mobile window manager.  Views paint immediate-mode with
 * the helpers below; there is no per-widget object tree.  A screen's state
 * is a plain C struct that embeds ui_view_t as its first member.
 *
 *   typedef struct { ui_view_t view; int count; } counter_t;
 *
 *   static void counter_paint(ui_view_t *v, twin_pixmap_t *px)
 *   {
 *       counter_t *c = (counter_t *) v;
 *       ui_fill(px, ui_view_bounds(v), phone_theme()->bg);
 *       ...
 *   }
 *   static bool counter_event(ui_view_t *v, const ui_event_t *e)
 *   {
 *       if (e->kind == UI_TAP) { ((counter_t *) v)->count++;
 *                                ui_view_invalidate_all(v); return true; }
 *       return false;
 *   }
 *   void app_counter_open(void)
 *   {
 *       counter_t *c = ui_view_new(sizeof(*c), "Counter", counter_paint,
 *                                  counter_event, NULL);
 *       if (c) ui_view_show(&c->view);
 *   }
 *
 * Coordinates handed to paint() and event() are client coordinates: (0, 0)
 * is the top-left of the view.  Drawing is clipped to the damaged region;
 * redraw everything and let the clip (and the coverage cache) make it cheap,
 * or skip work outside ui_view_clip().
 *
 * Memory rules: views free their own state in destroy(); timers and tweens
 * started with ui_view_every()/ui_view_tween() die with the view.  Never
 * keep raw pointers to another view.
 */

#ifndef _APPS_PHONE_H_
#define _APPS_PHONE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <twin.h>

/*
 * Geometry: portrait QVGA
 */
#define PHONE_W 240
#define PHONE_H 320
#define PHONE_STATUS_H 20
#define PHONE_NAV_H 42
#define PHONE_CONTENT_H (PHONE_H - PHONE_STATUS_H - PHONE_NAV_H) /* 258 */

/*
 * PHONE_CONTENT_H is the view height between the mobile bars only: under
 * the other scenes a view is taller or shorter.  Lay out from
 * ui_view_height() of the view at hand.
 */

/* Pixel format of every phone surface */
#if defined(CONFIG_DEMO_CELLPHONE_RGB16)
#define PHONE_FORMAT TWIN_RGB16
#else
#define PHONE_FORMAT TWIN_ARGB32
#endif

#define PHONE_LIST_ROW_H 40
#define PHONE_SECTION_H 22
#define PHONE_HEADER_H 30
#define PHONE_PAD 8

/*
 * Theme
 */
typedef struct {
    const char *name;
    twin_argb32_t primary, primary_dk;
    twin_argb32_t statusbar, statusbar_grad;
    twin_argb32_t navbar, navbar_grad;
    twin_argb32_t bg, bg_grad;
    twin_argb32_t card;
    twin_argb32_t text, text_sec;
    twin_argb32_t separator;
    twin_argb32_t call_green, call_red, missed_red;
    twin_argb32_t sms_sent, sms_recv;
    twin_argb32_t indicator;
    twin_argb32_t on_primary; /* text/icons drawn on primary */
} phone_theme_t;

const phone_theme_t *phone_theme(void);
int phone_theme_count(void);
int phone_theme_index(void);
const phone_theme_t *phone_theme_at(int idx);
/* Switch theme and repaint every view */
void phone_theme_set(int idx);

/*
 * Colors (premultiplied ARGB; all theme colors are opaque)
 */
#define UI_RGB(rgb) ((twin_argb32_t) (0xff000000u | (rgb)))
#define UI_WHITE UI_RGB(0xffffff)
#define UI_BLACK UI_RGB(0x000000)
/* Mix a toward b by t/255 */
twin_argb32_t ui_mix(twin_argb32_t a, twin_argb32_t b, int t);
/* Darken by pct percent (0..100) */
twin_argb32_t ui_darken(twin_argb32_t c, int pct);
/* Scale an opaque color to alpha a (premultiplies) */
twin_argb32_t ui_alpha(twin_argb32_t c, twin_a8_t a);

/*
 * Rectangles (half-open, client coordinates)
 */
typedef twin_rect_t ui_rect_t;

static inline ui_rect_t ui_rect(twin_coord_t x,
                                twin_coord_t y,
                                twin_coord_t w,
                                twin_coord_t h)
{
    ui_rect_t r = {x, (twin_coord_t) (x + w), y, (twin_coord_t) (y + h)};
    return r;
}

static inline twin_coord_t ui_w(ui_rect_t r)
{
    return r.right - r.left;
}

static inline twin_coord_t ui_h(ui_rect_t r)
{
    return r.bottom - r.top;
}

static inline bool ui_hit(ui_rect_t r, twin_coord_t x, twin_coord_t y)
{
    return r.left <= x && x < r.right && r.top <= y && y < r.bottom;
}

static inline ui_rect_t ui_inset(ui_rect_t r, twin_coord_t d)
{
    ui_rect_t o = {(twin_coord_t) (r.left + d), (twin_coord_t) (r.right - d),
                   (twin_coord_t) (r.top + d), (twin_coord_t) (r.bottom - d)};
    return o;
}

bool ui_intersects(ui_rect_t a, ui_rect_t b);

static inline bool ui_empty(ui_rect_t r)
{
    return r.left >= r.right || r.top >= r.bottom;
}

/* Smallest rectangle covering both (an empty operand is ignored) */
static inline ui_rect_t ui_rect_union(ui_rect_t a, ui_rect_t b)
{
    if (ui_empty(a))
        return b;
    if (ui_empty(b))
        return a;
    ui_rect_t u = {a.left < b.left ? a.left : b.left,
                   a.right > b.right ? a.right : b.right,
                   a.top < b.top ? a.top : b.top,
                   a.bottom > b.bottom ? a.bottom : b.bottom};
    return u;
}

static inline ui_rect_t ui_rect_intersect(ui_rect_t a, ui_rect_t b)
{
    ui_rect_t r = {a.left > b.left ? a.left : b.left,
                   a.right < b.right ? a.right : b.right,
                   a.top > b.top ? a.top : b.top,
                   a.bottom < b.bottom ? a.bottom : b.bottom};
    return r;
}

/* @inner lies entirely inside @outer */
static inline bool ui_rect_contains(ui_rect_t outer, ui_rect_t inner)
{
    return inner.left >= outer.left && inner.right <= outer.right &&
           inner.top >= outer.top && inner.bottom <= outer.bottom;
}

/*
 * Numbers
 */
static inline twin_fixed_t ui_fx(int v)
{
    return twin_int_to_fixed(v);
}

/* from + (to - from) * t, rounded; t is 0..TWIN_FIXED_ONE (a tween value) */
static inline int ui_lerp(int from, int to, twin_fixed_t t)
{
    return from + twin_fixed_to_int((to - from) * t + TWIN_FIXED_HALF);
}

/*
 * Typography: Montserrat for text, the icon font for TWIN_SYM_* symbols
 */
enum {
    FONT_SM = 12,
    FONT_NORMAL = 14,
    FONT_HEADING = 16,
    FONT_LARGE = 22,
    FONT_CLOCK = 32,
};

typedef enum {
    UI_LEFT,
    UI_CENTER,
    UI_RIGHT,
} ui_align_t;

/* Draw @s with its baseline at @y */
void ui_text(twin_pixmap_t *px,
             int size,
             twin_coord_t x,
             twin_coord_t y,
             ui_align_t align,
             twin_argb32_t color,
             const char *s);

/* Draw @s vertically centered in @r, aligned horizontally (4 px padding) */
void ui_text_in(twin_pixmap_t *px,
                int size,
                ui_rect_t r,
                ui_align_t align,
                twin_argb32_t color,
                const char *s);

/* Like ui_text_in() but truncates with an ellipsis to fit */
void ui_text_fit(twin_pixmap_t *px,
                 int size,
                 ui_rect_t r,
                 ui_align_t align,
                 twin_argb32_t color,
                 const char *s);

twin_coord_t ui_text_width(int size, const char *s);

/*
 * Word-wrap @s into lines no wider than @width, drawing each line at
 * @line_h intervals with the first baseline at @y (px may be NULL to only
 * measure).  Returns the number of lines; *widest receives the widest line.
 */
int ui_text_wrap(twin_pixmap_t *px,
                 int size,
                 twin_coord_t x,
                 twin_coord_t y,
                 twin_coord_t width,
                 twin_coord_t line_h,
                 twin_argb32_t color,
                 const char *s,
                 twin_coord_t *widest);

/* Baseline that vertically centers a line of @size text in [top, top+h) */
twin_coord_t ui_baseline(int size, twin_coord_t top, twin_coord_t h);

/* Draw a TWIN_SYM_* symbol centered at (cx, cy) */
void ui_icon(twin_pixmap_t *px,
             const char *sym,
             int size,
             twin_coord_t cx,
             twin_coord_t cy,
             twin_argb32_t color);

/*
 * Shapes (anti-aliased, cached by the coverage cache)
 */
void ui_fill(twin_pixmap_t *px, ui_rect_t r, twin_argb32_t color);
void ui_rrect(twin_pixmap_t *px, ui_rect_t r, int radius, twin_argb32_t color);
void ui_rrect_grad(twin_pixmap_t *px,
                   ui_rect_t r,
                   int radius,
                   twin_argb32_t top,
                   twin_argb32_t bottom);
void ui_rrect_stroke(twin_pixmap_t *px,
                     ui_rect_t r,
                     int radius,
                     int width,
                     twin_argb32_t color);
void ui_circle(twin_pixmap_t *px,
               twin_coord_t cx,
               twin_coord_t cy,
               int radius,
               twin_argb32_t color);
void ui_ring(twin_pixmap_t *px,
             twin_coord_t cx,
             twin_coord_t cy,
             int radius,
             int width,
             twin_argb32_t color);
void ui_line(twin_pixmap_t *px,
             twin_coord_t x0,
             twin_coord_t y0,
             twin_coord_t x1,
             twin_coord_t y1,
             int width,
             twin_argb32_t color);
/* Vertical gradient over a rectangle (no rounding) */
void ui_vgrad(twin_pixmap_t *px,
              ui_rect_t r,
              twin_argb32_t top,
              twin_argb32_t bottom);

/* Scratch path for custom shapes; release it after painting */
/* Create / free the kit's shared paths; phone_boot() calls these */
bool ui_init(void);
void ui_fini(void);

twin_path_t *ui_path(void);
void ui_path_done(twin_path_t *path);

/*
 * Views
 */
typedef enum {
    UI_PRESS,   /* finger down at (x, y) */
    UI_MOVE,    /* finger moved before the drag threshold */
    UI_DRAG,    /* dragging: (x, y) current, (dx, dy) since the press */
    UI_RELEASE, /* finger up after a drag; vx, vy in px/s */
    UI_TAP,     /* finger up without dragging */
    UI_CANCEL,  /* press abandoned (view lost the pointer) */
    UI_KEY,     /* hardware key: a character, UI_KEY_ENTER or _BACKSPACE */
    UI_SHOW,    /* the view became the top screen (focused) */
    UI_HIDE,    /* the view stopped being the top screen */
    UI_RESIZE,  /* the client area changed size (after the layout callback) */
} ui_event_kind_t;

typedef struct {
    ui_event_kind_t kind;
    twin_coord_t x, y;   /* current position */
    twin_coord_t x0, y0; /* where the press started */
    twin_coord_t dx, dy; /* displacement since the press */
    int vx, vy;          /* release velocity, px/s */
    twin_ucs4_t ucs4;    /* UI_KEY */
} ui_event_t;

typedef struct ui_view ui_view_t;
typedef void (*ui_paint_t)(ui_view_t *view, twin_pixmap_t *px);
typedef bool (*ui_handler_t)(ui_view_t *view, const ui_event_t *event);
typedef void (*ui_destroy_t)(ui_view_t *view);
typedef void (*ui_layout_t)(ui_view_t *view);
typedef twin_time_t (*ui_timer_t)(ui_view_t *view);
typedef bool (*ui_tween_t)(ui_view_t *view, twin_fixed_t value);

#define UI_VIEW_TIMERS 4

/* Kit internal: a timer or tween bound to a view (the callback's closure) */
typedef struct {
    ui_view_t *view;
    void *handle; /* twin_timeout_t or twin_tween_t, NULL when free */
    union {
        ui_timer_t timer;
        ui_tween_t tween;
    } fn;
} ui_slot_t;

struct ui_view {
    twin_window_t *window;
    const char *title;
    ui_paint_t paint;
    ui_handler_t event;
    ui_destroy_t destroy; /* free resources; the kit frees the struct */
    int app;              /* index into the app registry, or -1 */

    bool pressed; /* a finger is down on the view (read-only) */
    /* Kit internal: gesture tracking */
    bool dragging;
    twin_coord_t px0, py0, plx, ply;
    twin_time_t plt;
    int pvx, pvy;
    ui_layout_t layout; /* recomputes cached geometry; see set_layout */
    /* Kit internal: callbacks bound to the view's lifetime */
    ui_slot_t timers[UI_VIEW_TIMERS], tweens[UI_VIEW_TIMERS];
    bool *alive;               /* cleared on destroy while a callback runs */
    struct ui_view *next_view; /* Kit internal: list of live views */
};

/*
 * Allocate a zeroed view of @size bytes (>= sizeof(ui_view_t)) as an app
 * screen over the work area.  Returns NULL on allocation failure.
 */
void *ui_view_new(size_t size,
                  const char *title,
                  ui_paint_t paint,
                  ui_handler_t event,
                  ui_destroy_t destroy);

/* Same, covering the whole screen above the bars (lock screen) */
void *ui_view_new_fullscreen(size_t size,
                             const char *title,
                             ui_paint_t paint,
                             ui_handler_t event,
                             ui_destroy_t destroy);

/* Push the view on screen (slides in over the current one) */
void ui_view_show(ui_view_t *view);

/* Close the view (slides out); state is freed immediately */
void ui_view_close(ui_view_t *view);

/*
 * Geometry that depends on the view's size (scroll areas and the like) is
 * computed by @layout: now, and again whenever the view is resized.
 */
void ui_view_set_layout(ui_view_t *view, ui_layout_t layout);

/* The UI_KEY character if it is ASCII, else -1 (for keypad apps) */
int ui_key_ascii(const ui_event_t *event);

ui_rect_t ui_view_bounds(const ui_view_t *view);
twin_coord_t ui_view_width(const ui_view_t *view);
twin_coord_t ui_view_height(const ui_view_t *view);

/* Region being repainted, in client coordinates */
ui_rect_t ui_view_clip(ui_view_t *view);

/* Schedule a repaint of @r / of everything */
void ui_view_invalidate(ui_view_t *view, ui_rect_t r);
void ui_view_invalidate_all(ui_view_t *view);

/*
 * Move a pressed highlight: *slot becomes @next (-1 for none) and the old
 * and new items, located by @rect, are repainted.  Empty rects are skipped.
 */
void ui_view_set_pressed(ui_view_t *view,
                         int *slot,
                         int next,
                         ui_rect_t (*rect)(ui_view_t *view, int i));

/* Move pixels inside @r by (dx, dy) and repaint only the exposed band */
void ui_view_scroll(ui_view_t *view, ui_rect_t r, int dx, int dy);

/* View that owns @window, or NULL */
ui_view_t *ui_view_of(twin_window_t *window);

/*
 * Timers and tweens bound to a view: cancelled automatically when the view
 * is destroyed.  A timer callback returns the delay to the next call in ms,
 * or -1 to stop.  Each view holds up to UI_VIEW_TIMERS of each.
 */
bool ui_view_every(ui_view_t *view, twin_time_t delay, ui_timer_t fn);
/* A timer running @fn is set on the view */
bool ui_view_has_timer(ui_view_t *view, ui_timer_t fn);
void ui_view_stop_timers(ui_view_t *view);
void ui_view_stop_tweens(ui_view_t *view);
bool ui_view_tween(ui_view_t *view,
                   twin_time_t duration,
                   twin_ease_t ease,
                   ui_tween_t fn);

/*
 * Common widgets.  All are stateless painters plus hit tests; the caller
 * keeps state (pressed, value, ...) in its view struct.
 */

/* Screen header: title, optional trailing text or symbol */
void ui_header(twin_pixmap_t *px,
               ui_rect_t r,
               const char *title,
               const char *trailing);

/* Section header (list group label) */
void ui_section(twin_pixmap_t *px, ui_rect_t r, const char *label);

/* Filled button with a text or symbol label */
typedef enum {
    UI_BTN_PRIMARY, /* primary fill, on_primary text */
    UI_BTN_CARD,    /* card fill, text color */
    UI_BTN_GREEN,   /* call green */
    UI_BTN_RED,     /* call red */
    UI_BTN_FLAT,    /* no fill until pressed */
} ui_button_style_t;

void ui_button(twin_pixmap_t *px,
               ui_rect_t r,
               int radius,
               ui_button_style_t style,
               bool pressed,
               int size,
               const char *label);

/* List row: avatar/icon optional, title, optional subtitle and detail */
void ui_list_row(twin_pixmap_t *px,
                 ui_rect_t r,
                 bool pressed,
                 const char *sym,
                 twin_argb32_t sym_color,
                 const char *title,
                 const char *subtitle,
                 const char *detail);

/* Round avatar with an initial letter */
void ui_avatar(twin_pixmap_t *px,
               twin_coord_t cx,
               twin_coord_t cy,
               int radius,
               char initial,
               twin_argb32_t color);
twin_argb32_t ui_avatar_color(const char *name);

/* Horizontal slider: value 0..1000 */
void ui_slider(twin_pixmap_t *px, ui_rect_t r, int value, bool pressed);
int ui_slider_value_at(ui_rect_t r, twin_coord_t x);
/* Knob center for @value, the inverse of ui_slider_value_at() */
twin_coord_t ui_slider_knob_x(ui_rect_t r, int value);

/* On/off switch */
void ui_switch(twin_pixmap_t *px, ui_rect_t r, bool on);

/* Page indicator dots, current page highlighted */
void ui_dots(twin_pixmap_t *px,
             twin_coord_t cx,
             twin_coord_t cy,
             int count,
             int current);

/* Thin separator line along the bottom of @r */
void ui_separator(twin_pixmap_t *px, ui_rect_t r);

/* Narrow drawing to @r; returns the clip to hand to
 * twin_pixmap_restore_clip() */
twin_rect_t ui_clip_push(twin_pixmap_t *px, ui_rect_t r);

/*
 * Kinetic vertical scrolling.  Keep a ui_scroll_t in the view, call
 * ui_scroll_setup() when content height is known, feed every event to
 * ui_scroll_event() first, and paint content shifted up by s->offset.
 * Scrolling blits existing pixels and repaints only the exposed band.
 */
typedef struct {
    ui_rect_t area;         /* viewport, client coordinates */
    twin_coord_t content_h; /* total content height */
    twin_coord_t offset;    /* current scroll offset, 0..max */
    twin_coord_t grab;      /* offset at the start of a drag */
    int velocity;           /* px/s during a fling */
    twin_time_t last;       /* fling clock */
    bool flinging;
    bool caught; /* this press stopped a fling: its tap is swallowed */
} ui_scroll_t;

void ui_scroll_setup(ui_scroll_t *s, ui_rect_t area, twin_coord_t content_h);
twin_coord_t ui_scroll_max(const ui_scroll_t *s);
/* Returns true when the event was a scroll gesture (do not treat as tap) */
bool ui_scroll_event(ui_view_t *view, ui_scroll_t *s, const ui_event_t *e);
/* Scroll to an absolute offset (clamped), blitting what can be reused */
void ui_scroll_to(ui_view_t *view, ui_scroll_t *s, twin_coord_t offset);
/* Thin indicator on the right edge while content overflows */
void ui_scroll_bar(twin_pixmap_t *px, const ui_scroll_t *s);

/*
 * Single-line text field
 */
typedef struct {
    char text[64];
    int len;
    bool focused;
} ui_field_t;

void ui_field(twin_pixmap_t *px,
              ui_rect_t r,
              const ui_field_t *f,
              const char *placeholder);
void ui_field_set(ui_field_t *f, const char *s);
/* Apply a keyboard key; returns true if the text changed */
bool ui_field_key(ui_field_t *f, int key);

/*
 * Soft keyboard, drawn inside the view along its bottom edge.
 */
enum {
    UI_KEY_NONE = 0,
    UI_KEY_BACKSPACE = 8,
    UI_KEY_ENTER = 13,
    UI_KEY_SHIFT = 0x100,
    UI_KEY_MODE, /* letters <-> digits/symbols */
};

typedef struct {
    bool visible;
    bool shift;
    bool symbols;
    int pressed;     /* index of the pressed key, -1 when none */
    ui_rect_t dirty; /* keys whose look changed in the last event */
} ui_kbd_t;

#define UI_KBD_H 132

void ui_kbd_init(ui_kbd_t *k);
/* Area the keyboard occupies inside a view of the given height */
ui_rect_t ui_kbd_rect(twin_coord_t view_h);
void ui_kbd_draw(twin_pixmap_t *px, const ui_kbd_t *k, ui_rect_t r);
/*
 * Feed a view event; returns the key typed on UI_TAP (character code or
 * UI_KEY_*), UI_KEY_NONE otherwise.  Shift/mode keys are handled here.
 * Afterwards k->dirty covers the keys whose look changed (empty if none):
 * invalidate it rather than the whole keyboard.
 */
int ui_kbd_event(ui_kbd_t *k, ui_rect_t r, const ui_event_t *e);

/* Rectangle of the key producing @code in the current layout */
bool ui_kbd_find(const ui_kbd_t *k, ui_rect_t r, int code, ui_rect_t *out);

/*
 * Shell
 */

/* Boot the phone on @screen: bars, home, lock screen.  False when out of
 * memory before anything was created. */
bool phone_boot(twin_screen_t *screen);
void phone_shutdown(void);
twin_screen_t *phone_screen(void);

/* Navigation */
void phone_open(int app);   /* launch an app from the registry */
void phone_back(void);      /* close the top view */
void phone_home(void);      /* close everything above home */
int phone_depth(void);      /* views above home (0 at home) */
ui_view_t *phone_top(void); /* topmost view, NULL if none */
void phone_lock(void);      /* show the lock screen */
bool phone_locked(void);

/* Ask the status/nav bars to repaint (after navigation or theme change) */
void phone_bars_refresh(void);

/* Home screen wallpaper */
int phone_wallpaper(void);
int phone_wallpaper_count(void);
const char *phone_wallpaper_name(int idx);
void phone_set_wallpaper(int idx);
/* Paint wallpaper @idx into @r; returns the label color that reads on it */
twin_argb32_t phone_paint_wallpaper(twin_pixmap_t *px, ui_rect_t r, int idx);

/* Window manager scene (Settings): TwinWmMobile, TwinWmDesktop, TwinWmNone */
bool phone_set_scene(twin_wm_mode_t mode);

/* Wall clock; tests install a fixed source */
typedef struct {
    int hour, min, sec;
    int wday; /* 0 = Sunday */
    int mday; /* 1..31 */
    int mon;  /* 0 = January */
} phone_clock_t;

void phone_clock(phone_clock_t *c);
void phone_set_clock_source(void (*source)(phone_clock_t *c));
void phone_format_time(char *buf, size_t len); /* "12:03" */
void phone_format_date(char *buf, size_t len); /* "Saturday, May 9" */

/* App registry */
typedef void (*phone_glyph_t)(twin_pixmap_t *px,
                              ui_rect_t plate,
                              twin_argb32_t fg);

typedef struct {
    const char *name;
    const char *sym;      /* TWIN_SYM_* or NULL */
    phone_glyph_t glyph;  /* procedural icon when sym is NULL */
    twin_argb32_t accent; /* icon plate color */
    void (*open)(void);
} phone_app_t;

int phone_app_count(void);
const phone_app_t *phone_app_at(int idx);
int phone_app_find(const char *name);

/* Draw an app's home-screen icon (plate + glyph) into @plate */
void phone_app_icon(twin_pixmap_t *px, int app, ui_rect_t plate, bool pressed);

/*
 * Apps.  Each opens its first screen as a new view.
 */
void app_dialer_open(void);
void app_contacts_open(void);
void app_sms_open(void);
void app_calc_open(void);
void app_music_open(void);
void app_photos_open(void);
void app_camera_open(void);
void app_settings_open(void);
void app_calllog_open(void);
void app_snake_open(void);
void app_pong_open(void);
void app_tetris_open(void);

/* Home and lock screens (shell.c) */
ui_view_t *home_create(void);
ui_view_t *lock_create(void);

/* Dial a number from another app (contacts, call log) */
void app_dialer_call(const char *name, const char *number);

#endif /* _APPS_PHONE_H_ */
