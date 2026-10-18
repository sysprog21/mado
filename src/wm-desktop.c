/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Desktop window manager policy
 *
 * Overlapping windows with a vector-drawn title bar, click-to-raise,
 * drag-to-move, iconify/restore, close, and a drop shadow under the
 * active window.  The shadow is a compositor attribute, so activating a
 * window costs a damage rectangle rather than a blur pass.
 *
 * Also: double-click the title to maximize to the work area (and back),
 * drop a dragged title on the left or right screen edge to snap to that
 * half, or on the top edge to maximize; drag an unclaimed press in the
 * bottom-right corner of the client to resize.  Windows always keep part
 * of the title bar on screen.
 */

#include "twin_private.h"

#define TWIN_ACTIVE_BG 0xd03b80ae
#define TWIN_INACTIVE_BG 0xffb0b0b0
#define TWIN_FRAME_TEXT 0xffffffff
#define TWIN_ACTIVE_BORDER 0xff606060
#define TWIN_INACTIVE_BORDER 0xff909090
#define TWIN_BW 0
#define TWIN_TITLE_HEIGHT 20
#define TWIN_TITLE_BW ((TWIN_TITLE_HEIGHT + 11) / 12)

#if defined(CONFIG_DROP_SHADOW)
#define SHADOW_ALPHA 0x80
#endif

#define DOUBLE_CLICK_MS 300
#define RESIZE_GRIP 12  /* bottom-right corner of the client that resizes */
#define MIN_CLIENT_W 48 /* smallest client a resize leaves */
#define MIN_CLIENT_H 24
#define TITLE_VISIBLE 32 /* title pixels a drag must leave on screen */
#define SNAP_EDGE 2      /* pointer this close to an edge snaps on release */

typedef struct {
    twin_window_t *drag;   /* window being moved */
    twin_window_t *resize; /* window being resized */
    /* Drag: grab point inside the frame.  Resize: pointer minus client
     * size, so the size is the pointer minus this */
    twin_coord_t button_x, button_y;
    twin_coord_t resize_w, resize_h; /* size to apply on the next frame */
    twin_work_t *resize_work;        /* pending application, at most one */
    twin_window_t *click;            /* last title click, for double-click */
    twin_time_t click_time;
} twin_desktop_t;

#define DESKTOP(screen) ((twin_desktop_t *) (screen)->wm_data)

static void desktop_layout(twin_window_t *window,
                           twin_rect_t *geom,
                           twin_rect_t *border)
{
    twin_screen_t *screen = window->screen;
    const twin_rect_t *r = &window->request;

    *border = (twin_rect_t) {0, 0, 0, 0};
    switch (window->style) {
    case TwinWindowFullscreen:
        *geom = (twin_rect_t) {0, screen->width, 0, screen->height};
        return;
    case TwinWindowApplication:
        border->left = TWIN_BW;
        border->right = TWIN_BW;
        border->top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
        border->bottom = TWIN_BW;
        break;
    case TwinWindowPlain:
    default:
        break;
    }
    /* request holds the frame origin and the client size */
    geom->left = r->left;
    geom->top = r->top;
    geom->right = r->right + border->left + border->right;
    geom->bottom = r->bottom + border->top + border->bottom;
}

/* Title bar geometry shared by painting and hit testing */
typedef struct {
    twin_fixed_t bw, t_h, t_arc_1, t_arc_2;
    twin_fixed_t c_left, c_right, c_top, w_top;
    twin_fixed_t name_height, icon_size, icon_y;
    twin_fixed_t menu_x, text_x, text_y;
    twin_fixed_t close_x, restore_x, iconify_x;
} twin_title_t;

static const char *_title_name(const twin_window_t *window)
{
    return window->name ? window->name : "twin";
}

static void _title_metrics(twin_window_t *window, twin_title_t *t)
{
    t->bw = twin_int_to_fixed(TWIN_TITLE_BW);
    twin_fixed_t bw_2 = t->bw / 2;
    t->w_top = bw_2;
    t->c_left = bw_2;
    t->t_h = twin_int_to_fixed(window->client.top) - t->bw;
    t->t_arc_1 = t->t_h / 3;
    t->t_arc_2 = t->t_h * 2 / 3;
    t->c_right = twin_int_to_fixed(window->client.right) - bw_2;
    t->c_top = twin_int_to_fixed(window->client.top) - bw_2;
    t->name_height = t->t_h - t->bw - bw_2;
    t->icon_size = t->name_height * 8 / 10;
    t->icon_y = (twin_int_to_fixed(window->client.top) - t->icon_size) / 2;
    t->menu_x = t->t_arc_2;
    t->text_x = t->menu_x + t->icon_size + t->bw;
    t->text_y = t->icon_y + t->icon_size;

    twin_path_t *path = _twin_path_acquire(window->screen);
    twin_fixed_t text_width = 0;
    if (path) {
        twin_path_set_font_size(path, t->name_height);
        twin_path_set_font_style(path, TwinStyleOblique | TwinStyleUnhinted);
        text_width = twin_width_utf8(path, _title_name(window));
        _twin_path_release(window->screen, path);
    }
    twin_fixed_t title_right = t->text_x + text_width + t->bw + t->icon_size +
                               t->bw + t->icon_size + t->bw + t->icon_size +
                               t->t_arc_2;
    if (title_right < t->c_right)
        t->c_right = title_right;
    t->close_x = t->c_right - t->t_arc_2 - t->icon_size;
    t->restore_x = t->close_x - t->bw - t->icon_size;
    t->iconify_x = t->restore_x - t->bw - t->icon_size;
}

static void _draw_icon(twin_pixmap_t *pixmap,
                       twin_icon_t icon,
                       twin_fixed_t x,
                       twin_fixed_t y,
                       twin_fixed_t size)
{
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_translate(&m, x, y);
    twin_matrix_scale(&m, size, size);
    twin_icon_draw(pixmap, icon, m);
}

static void desktop_decorate(twin_window_t *window)
{
    twin_pixmap_t *pixmap = window->pixmap;
    twin_title_t t;

    if (window->style != TwinWindowApplication)
        return;
    _title_metrics(window, &t);

    twin_pixmap_reset_clip(pixmap);
    pixmap->origin_x = pixmap->origin_y = 0;
    twin_fill(pixmap, 0x00000000, TWIN_SOURCE, 0, 0, pixmap->width,
              window->client.top);

    twin_path_t *path = _twin_path_acquire(window->screen);
    if (!path)
        return;

    /* Tab-shaped title bar */
    twin_path_move(path, t.c_left, t.c_top);
    twin_path_draw(path, t.c_right, t.c_top);
    twin_path_curve(path, t.c_right, t.w_top + t.t_arc_1, t.c_right - t.t_arc_1,
                    t.w_top, t.c_right - t.t_h, t.w_top);
    twin_path_draw(path, t.c_left + t.t_h, t.w_top);
    twin_path_curve(path, t.c_left + t.t_arc_1, t.w_top, t.c_left,
                    t.w_top + t.t_arc_1, t.c_left, t.c_top);
    twin_path_close(path);
    twin_paint_path(pixmap, window->active ? TWIN_ACTIVE_BG : TWIN_INACTIVE_BG,
                    path);
    twin_paint_stroke(
        pixmap, window->active ? TWIN_ACTIVE_BORDER : TWIN_INACTIVE_BORDER,
        path, t.bw);
    twin_path_empty(path);

    /* Name, clipped between the menu icon and the buttons */
    twin_pixmap_clip(pixmap, twin_fixed_to_int(twin_fixed_floor(t.menu_x)), 0,
                     twin_fixed_to_int(twin_fixed_ceil(t.c_right - t.t_arc_2)),
                     window->client.top);
    twin_pixmap_origin_to_clip(pixmap);
    twin_path_set_font_size(path, t.name_height);
    twin_path_set_font_style(path, TwinStyleOblique | TwinStyleUnhinted);
    twin_path_move(path, t.text_x - twin_fixed_floor(t.menu_x), t.text_y);
    twin_path_utf8(path, _title_name(window));
    twin_paint_path(pixmap, TWIN_FRAME_TEXT, path);
    _twin_path_release(window->screen, path);

    twin_pixmap_reset_clip(pixmap);
    pixmap->origin_x = pixmap->origin_y = 0;
    _draw_icon(pixmap, TwinIconMenu, t.menu_x, t.icon_y, t.icon_size);
    _draw_icon(pixmap, TwinIconIconify, t.iconify_x, t.icon_y, t.icon_size);
    _draw_icon(pixmap, TwinIconRestore, t.restore_x, t.icon_y, t.icon_size);
    _draw_icon(pixmap, TwinIconClose, t.close_x, t.icon_y, t.icon_size);

    twin_pixmap_damage(pixmap, 0, 0, pixmap->width, window->client.top);
}

/* Iconified: only the title bar rows are shown (see scan_height) */
static bool _iconified(const twin_window_t *window)
{
    return window->pixmap->scan_height < window->pixmap->height;
}

static void _set_active(twin_window_t *window, bool active)
{
    if (window->active == active)
        return;
    window->active = active;
#if defined(CONFIG_DROP_SHADOW)
    if (active)
        twin_pixmap_set_shadow(window->pixmap, CONFIG_HORIZONTAL_OFFSET,
                               CONFIG_VERTICAL_OFFSET, CONFIG_SHADOW_BLUR,
                               SHADOW_ALPHA);
    else
        twin_pixmap_set_shadow(window->pixmap, 0, 0, 0, 0);
#endif
    if (window->style == TwinWindowApplication) {
        window->frame_dirty = true;
        twin_window_draw(window);
    }
}

/* Topmost window that can take the keyboard, other than @skip */
static twin_window_t *_focus_candidate(twin_screen_t *screen,
                                       const twin_window_t *skip)
{
    twin_window_t *best = NULL;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w != skip && _twin_window_managed(w) && _twin_window_shown(w) &&
            !_iconified(w))
            best = w;
    }
    return best;
}

/* Give @window (NULL: nobody) the keyboard and deactivate every other */
static void _focus(twin_screen_t *screen, twin_window_t *window)
{
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w != window && w->pixmap)
            _set_active(w, false);
    }
    if (window)
        _set_active(window, true);
    twin_pixmap_t *p = window ? window->pixmap : NULL;
    if (screen->active != p)
        twin_screen_set_active(screen, p);
}

/* Activate @window; an iconified window shows no content, so the keyboard
 * goes to the topmost window that does */
static void _activate(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;
    _focus(screen,
           _iconified(window) ? _focus_candidate(screen, window) : window);
}

/* Give an application window a spot of its own the first time it shows:
 * cascade off any earlier window at the same origin (screens built for a
 * phone all ask for the same spot) and keep the frame on screen */
static void _clamp_to_screen(twin_window_t *w);

static void _offset(twin_rect_t *r, twin_coord_t dx, twin_coord_t dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}

static void _place_new(twin_window_t *w)
{
    twin_screen_t *screen = w->screen;
    w->wm_placed = true;
    if (w->style != TwinWindowApplication)
        return;
    twin_rect_t base = w->request;
    twin_coord_t step = TWIN_TITLE_HEIGHT + 4;
    for (int k = 0; k < 16; k++) {
        w->request = base;
        _offset(&w->request, k * step, k * step);
        _clamp_to_screen(w);
        bool clash = false;
        for (twin_window_t *o = screen->windows; o; o = o->wm_next)
            if (o != w && o->wm_placed && o->style == TwinWindowApplication &&
                o->request.left == w->request.left &&
                o->request.top == w->request.top)
                clash = true;
        if (!clash)
            return;
    }
    w->request = base;
    _clamp_to_screen(w);
}

static void desktop_map(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;

    if (_twin_window_is_bar(window))
        return;
    if (!window->wm_placed) {
        _place_new(window);
        _twin_window_relayout(window); /* no-op unless it moved */
    }
    _twin_window_restore(window);
    _twin_window_raise(window);
    if (window->pixmap != screen->top)
        twin_pixmap_show(window->pixmap, screen, screen->top);
    if (window->style != TwinWindowPlain)
        _activate(window);
}

/* twin_window_activate(): an explicit request also restores */
static void desktop_activate(twin_window_t *window)
{
    _twin_pixmap_set_scan_height(window->pixmap, window->pixmap->height);
    desktop_map(window);
}

static void desktop_unmap(twin_window_t *window, bool destroying)
{
    twin_screen_t *screen = window->screen;
    twin_desktop_t *d = DESKTOP(screen);
    (void) destroying;

    if (d->drag == window)
        d->drag = NULL;
    if (d->resize == window)
        d->resize = NULL;
    if (d->click == window)
        d->click = NULL;
    /* Only the window that had the keyboard hands it on */
    bool had_focus =
        window->active || (screen->active && screen->active == window->pixmap);
    _set_active(window, false);
    twin_pixmap_hide(window->pixmap);
    if (!had_focus)
        return;
    twin_window_t *next = _focus_candidate(screen, window);
    if (next)
        _focus(screen, next);
}

/* Pixel test so clicks on the rounded title corners fall through */
static bool _frame_transparent(twin_pixmap_t *p, twin_coord_t x, twin_coord_t y)
{
    if (_twin_pixmap_is_stub(p))
        return true;
    twin_pointer_t q = twin_pixmap_pointer(p, x - p->x, y - p->y);
    switch (p->format) {
    case TWIN_A8:
        return *q.a8 == 0;
    case TWIN_ARGB32:
        return (*q.argb32 >> 24) == 0;
    default:
        return false;
    }
}

static bool desktop_hit(twin_window_t *window, twin_coord_t x, twin_coord_t y)
{
    twin_pixmap_t *p = window->pixmap;
    if (x < p->x || x >= p->x + p->width || y < p->y || y >= p->y + p->height)
        return false;
    if (window->style != TwinWindowApplication)
        return true;
    if (y < p->y + window->client.top)
        return !_frame_transparent(p, x, y);
    return y < p->y + p->scan_height;
}

/* Frame at (@x, @y) with a @cw x @ch client, re-laid out at once */
static void _set_frame(twin_window_t *window,
                       twin_coord_t x,
                       twin_coord_t y,
                       twin_coord_t cw,
                       twin_coord_t ch)
{
    window->request = (twin_rect_t) {x, x + cw, y, y + ch};
    _twin_window_relayout(window);
}

/* Fill @area (a work-area rectangle) with @window's frame */
static void _fill_area(twin_window_t *window, twin_rect_t area)
{
    if (!window->wm_maximized) {
        window->wm_saved = window->request;
        window->wm_maximized = true;
    }
    _set_frame(window, area.left, area.top, area.right - area.left,
               max((twin_coord_t) (area.bottom - area.top - TWIN_TITLE_HEIGHT),
                   (twin_coord_t) MIN_CLIENT_H));
}

static void _unmaximize(twin_window_t *window)
{
    if (!window->wm_maximized)
        return;
    window->wm_maximized = false;
    twin_rect_t r = window->wm_saved;
    _set_frame(window, r.left, r.top, r.right - r.left, r.bottom - r.top);
}

static void _toggle_maximize(twin_window_t *window)
{
    if (window->wm_maximized)
        _unmaximize(window);
    else
        _fill_area(window, twin_screen_work_area(window->screen));
}

/* Keep enough of the title bar on screen to grab it again */
static void _clamp_title(const twin_screen_t *screen,
                         twin_coord_t frame_w,
                         twin_coord_t *x,
                         twin_coord_t *y)
{
    twin_coord_t keep = min((twin_coord_t) TITLE_VISIBLE, frame_w);
    *x = max(*x, (twin_coord_t) (keep - frame_w));
    *x = min(*x, (twin_coord_t) (screen->width - keep));
    *y = max(*y, (twin_coord_t) 0);
    *y = min(*y, (twin_coord_t) (screen->height - TWIN_TITLE_HEIGHT));
}

/* Released a title drag at screen (@x, @y): snap on the screen edges */
static void _drop(twin_window_t *window, twin_coord_t x, twin_coord_t y)
{
    twin_screen_t *screen = window->screen;
    twin_rect_t area = twin_screen_work_area(screen);
    twin_coord_t half = (area.right - area.left) / 2;
    if (y <= area.top + SNAP_EDGE) {
        _fill_area(window, area);
    } else if (x <= SNAP_EDGE) {
        area.right = area.left + half;
        _fill_area(window, area);
    } else if (x >= screen->width - 1 - SNAP_EDGE) {
        area.left = area.right - half;
        _fill_area(window, area);
    }
}

static bool _in_grip(const twin_window_t *window,
                     twin_coord_t x,
                     twin_coord_t y)
{
    return window->style == TwinWindowApplication &&
           x >= window->client.right - RESIZE_GRIP &&
           y >= window->client.bottom - RESIZE_GRIP;
}

static bool _title_press(twin_window_t *window, twin_event_t *event)
{
    twin_screen_t *screen = window->screen;
    twin_desktop_t *d = DESKTOP(screen);
    twin_pixmap_t *pixmap = window->pixmap;
    twin_title_t t;
    twin_coord_t lx = event->u.pointer.x;
    _title_metrics(window, &t);
    if (lx > twin_fixed_to_int(t.close_x) &&
        lx < twin_fixed_to_int(t.close_x + t.icon_size)) {
        twin_window_close(window);
        return true;
    }
    if (lx > twin_fixed_to_int(t.iconify_x) &&
        lx < twin_fixed_to_int(t.restore_x)) {
        _twin_pixmap_set_scan_height(pixmap, window->client.top);
    } else if (lx > twin_fixed_to_int(t.restore_x) &&
               lx < twin_fixed_to_int(t.close_x)) {
        _twin_pixmap_set_scan_height(pixmap, pixmap->height);
    } else {
        twin_time_t now = twin_now();
        if (d->click == window && now - d->click_time <= DOUBLE_CLICK_MS) {
            d->click = NULL;
            _toggle_maximize(window);
            /* Configure handlers may have destroyed it */
            if (!window->dead)
                desktop_map(window);
            return true;
        }
        d->click = window;
        d->click_time = now;
    }
    return false;
}

/* Apply the size a resize drag asked for; at most once per frame, since
 * each new size reallocates the window's pixmap */
static void _resize_apply(twin_desktop_t *d)
{
    twin_window_t *w = d->resize;
    if (d->resize_work)
        twin_clear_work(d->resize_work);
    d->resize_work = NULL;
    if (!w || (d->resize_w == w->client.right - w->client.left &&
               d->resize_h == w->client.bottom - w->client.top))
        return;
    w->wm_maximized = false;
    _set_frame(w, w->request.left, w->request.top, d->resize_w, d->resize_h);
}

static bool _resize_work(void *closure)
{
    twin_desktop_t *d = DESKTOP((twin_screen_t *) closure);
    d->resize_work = NULL; /* the work item ends with this call */
    _resize_apply(d);
    return false;
}

static bool desktop_dispatch(twin_window_t *window, twin_event_t *event)
{
    twin_screen_t *screen = window->screen;
    twin_desktop_t *d = DESKTOP(screen);
    /* Only pointer events carry coordinates */
    bool pointer = event->kind == TwinEventButtonDown ||
                   event->kind == TwinEventButtonUp ||
                   event->kind == TwinEventMotion;
    bool in_client = pointer && window->client.left <= event->u.pointer.x &&
                     event->u.pointer.x < window->client.right &&
                     window->client.top <= event->u.pointer.y &&
                     event->u.pointer.y < window->client.bottom;
    /* Client callbacks may destroy the window (window->dead) */

    switch (event->kind) {
    case TwinEventActivate:
    case TwinEventDeactivate:
        /* Focus follows the WM's own bookkeeping; tell the client only. */
        return _twin_window_deliver(window, event);
    case TwinEventButtonDown:
        if (window->style == TwinWindowApplication &&
            event->u.pointer.y < window->client.top) {
            if (_title_press(window, event))
                return true;
        } else {
            d->click = NULL;
        }
        desktop_map(window); /* raise and activate */
        if (window->dead)
            return true;
        if (in_client && !_iconified(window)) {
            window->client_grab = true;
            if (_twin_window_deliver(window, event) || window->dead)
                return true;
            /* Unclaimed press inside the client drags the window */
            window->client_grab = false;
            if (_in_grip(window, event->u.pointer.x, event->u.pointer.y)) {
                d->resize = window;
                d->button_x = event->u.pointer.screen_x -
                              (window->client.right - window->client.left);
                d->button_y = event->u.pointer.screen_y -
                              (window->client.bottom - window->client.top);
                return true;
            }
        }
        if (window->style == TwinWindowApplication) {
            d->drag = window;
            d->button_x = event->u.pointer.x;
            d->button_y = event->u.pointer.y;
        }
        return true;
    case TwinEventButtonUp:
        if (d->resize == window) {
            _resize_apply(d); /* the final size, at once */
            d->resize = NULL;
            return true;
        }
        if (d->drag == window) {
            d->drag = NULL;
            if (!window->client_grab) {
                _drop(window, event->u.pointer.screen_x,
                      event->u.pointer.screen_y);
                return true;
            }
        }
        if (window->client_grab) {
            window->client_grab = false;
            return _twin_window_deliver(window, event);
        }
        return false;
    case TwinEventMotion:
        if (d->resize == window) {
            d->resize_w =
                max((twin_coord_t) (event->u.pointer.screen_x - d->button_x),
                    (twin_coord_t) MIN_CLIENT_W);
            d->resize_h =
                max((twin_coord_t) (event->u.pointer.screen_y - d->button_y),
                    (twin_coord_t) MIN_CLIENT_H);
            if (!d->resize_work)
                d->resize_work =
                    twin_set_work(_resize_work, TWIN_WORK_LAYOUT, screen);
            if (!d->resize_work)
                _resize_apply(d); /* out of memory: apply right away */
            return true;
        }
        if (d->drag == window && !window->client_grab) {
            /* Dragging a maximized window puts it back to its own size,
             * still under the pointer */
            if (window->wm_maximized) {
                twin_coord_t old_w = window->pixmap->width;
                _unmaximize(window);
                /* Configure handlers may have destroyed it */
                if (window->dead)
                    return true;
                if (d->button_x >= window->pixmap->width)
                    d->button_x = d->button_x * window->pixmap->width / old_w;
            }
            twin_coord_t x = event->u.pointer.screen_x - d->button_x;
            twin_coord_t y = event->u.pointer.screen_y - d->button_y;
            twin_coord_t w = window->request.right - window->request.left;
            twin_coord_t h = window->request.bottom - window->request.top;
            _clamp_title(screen, window->pixmap->width, &x, &y);
            window->request = (twin_rect_t) {x, x + w, y, y + h};
            twin_pixmap_move(window->pixmap, x, y);
            return true;
        }
        if (window->client_grab || in_client)
            return _twin_window_deliver(window, event);
        return false;
    default:
        return _twin_window_deliver(window, event);
    }
}

/* Shift @w's requested frame so it lies on screen where it fits */
static void _clamp_to_screen(twin_window_t *w)
{
    twin_screen_t *screen = w->screen;
    twin_rect_t geom, border;
    desktop_layout(w, &geom, &border);
    twin_coord_t dx = 0, dy = 0;
    if (geom.right > screen->width)
        dx = max((twin_coord_t) (screen->width - geom.right),
                 (twin_coord_t) -geom.left);
    if (geom.bottom > screen->height)
        dy = max((twin_coord_t) (screen->height - geom.bottom),
                 (twin_coord_t) -geom.top);
    /* Never leave the title bar above the screen */
    if (geom.top + dy < 0)
        dy = (twin_coord_t) -geom.top;
    if (geom.left + dx < 0 && geom.right + dx <= 0)
        dx = (twin_coord_t) -geom.left;
    _offset(&w->request, dx, dy);
}

static bool desktop_enter(twin_screen_t *screen)
{
    twin_desktop_t *d = twin_calloc(1, sizeof(*d));
    if (!d)
        return false;
    screen->wm_data = d;

    /* Place every application window afresh, bottom to top (leave cleared
     * wm_placed) */
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        _place_new(w);
        _twin_window_relayout(w);
    }
    /* Map in stacking order; desktop_map raises, so iterate a snapshot. */
    twin_window_t *top = NULL;
    int n = 0;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        n++;
    for (int i = 0; i < n; i++) {
        twin_window_t *w = screen->windows;
        /* Activation handlers may have destroyed windows meanwhile */
        if (!w)
            break;
        if (w->mapped && !_twin_window_is_bar(w)) {
            desktop_map(w);
            if (w->style != TwinWindowPlain)
                top = w;
        } else {
            _twin_window_raise(w);
        }
    }
    if (top)
        _activate(top);
    return true;
}

/* The screen or the work area changed size: refit maximized windows and
 * bring everything else back on screen */
static void desktop_refresh(twin_screen_t *screen)
{
    twin_rect_t area = twin_screen_work_area(screen);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (!w->pixmap || w->style != TwinWindowApplication)
            continue;
        if (w->wm_maximized) {
            /* Snapped halves keep their side, maximized windows refit */
            twin_rect_t r = area;
            twin_coord_t fw = w->pixmap->width;
            twin_coord_t half = (area.right - area.left) / 2;
            if (fw < area.right - area.left) {
                if (w->pixmap->x > area.left)
                    r.left = r.right - half;
                else
                    r.right = r.left + half;
            }
            _fill_area(w, r);
            continue;
        }
        _clamp_to_screen(w);
        _twin_window_relayout(w); /* no-op unless it moved */
    }
}

static void desktop_leave(twin_screen_t *screen)
{
    twin_desktop_t *d = DESKTOP(screen);
    if (d->resize_work)
        twin_clear_work(d->resize_work);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        w->active = false;
        w->client_grab = false;
        /* Placement is redone on the next entry; maximized stays as is */
        w->wm_placed = false;
        /* A window being destroyed may already have lost its pixmap */
        if (w->pixmap)
            twin_pixmap_set_shadow(w->pixmap, 0, 0, 0, 0);
    }
    twin_free(screen->wm_data);
    screen->wm_data = NULL;
}

const twin_wm_ops_t _twin_wm_desktop = {
    .mode = TwinWmDesktop,
    .enter = desktop_enter,
    .leave = desktop_leave,
    .layout = desktop_layout,
    .map = desktop_map,
    .unmap = desktop_unmap,
    .decorate = desktop_decorate,
    .dispatch = desktop_dispatch,
    .hit = desktop_hit,
    .refresh = desktop_refresh,
    .activate = desktop_activate,
};
