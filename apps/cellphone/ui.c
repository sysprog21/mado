/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone UI kit: views, gestures, timers and tweens, scrolling.
 */

#include <stdlib.h>
#include <string.h>

#include "ui-internal.h"

#define DRAG_SLOP 6

/*
 * Views
 */
static void _ui_draw(twin_window_t *window);
static bool _ui_event(twin_window_t *window, twin_event_t *ev);
static void _ui_destroy(twin_window_t *window);

/* Every live view, so ui_view_of() can tell kit windows from the others */
static ui_view_t *g_views;

/* Where views open, and who hears that the view stack changed */
static twin_screen_t *g_screen;
static void (*g_changed)(void);

void ui_attach(twin_screen_t *screen, void (*changed)(void))
{
    g_screen = screen;
    g_changed = changed;
}

static void _changed(void)
{
    if (g_changed)
        g_changed();
}

bool ui_is_app_window(twin_window_t *w)
{
    return twin_window_is_mapped(w) &&
           twin_window_style(w) == TwinWindowApplication;
}

twin_window_t *ui_top_window(void)
{
    twin_window_t *top = NULL;
    if (!g_screen)
        return NULL;
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;
         w = twin_window_next(w))
        if (ui_is_app_window(w))
            top = w;
    return top;
}

bool ui_view_covered(const ui_view_t *view)
{
#if defined(CONFIG_WM_MOBILE)
    /* The mobile policy gives every app window the whole work area, and a
     * fullscreen one (the lock screen) more: either above hides this one */
    for (twin_window_t *w = twin_window_next(view->window); w;
         w = twin_window_next(w))
        if (ui_is_app_window(w) ||
            (twin_window_is_mapped(w) &&
             twin_window_style(w) == TwinWindowFullscreen))
            return true;
    return false;
#else
    /* Overlapping windows: a view below another still shows */
    return !twin_window_is_mapped(view->window);
#endif
}

/* A PHONE_W wide view of @h rows at @x,@y on @screen */
static void *_ui_view_new(twin_screen_t *screen,
                          twin_window_style_t style,
                          twin_coord_t x,
                          twin_coord_t y,
                          twin_coord_t h,
                          size_t size,
                          const char *title,
                          ui_paint_t paint,
                          ui_handler_t event,
                          ui_destroy_t destroy)
{
    if (!screen || size < sizeof(ui_view_t))
        return NULL;
    ui_view_t *v = calloc(1, size);
    if (!v)
        return NULL;

    twin_window_t *w =
        twin_window_create(screen, PHONE_FORMAT, style, x, y, PHONE_W, h);
    if (!w) {
        free(v);
        return NULL;
    }
    v->window = w;
    v->title = title;
    v->paint = paint;
    v->event = event;
    v->destroy = destroy;
    v->app = -1;
    v->next_view = g_views;
    g_views = v;
    twin_window_set_client_data(w, v);
    twin_window_set_callbacks(w, _ui_draw, _ui_event, _ui_destroy);
    twin_window_set_name(w, title);
    twin_window_set_opaque(w, true);
    return v;
}

void *ui_view_new(size_t size,
                  const char *title,
                  ui_paint_t paint,
                  ui_handler_t event,
                  ui_destroy_t destroy)
{
    return _ui_view_new(g_screen, TwinWindowApplication, 0, PHONE_STATUS_H,
                        PHONE_CONTENT_H, size, title, paint, event, destroy);
}

void *ui_view_new_at(twin_screen_t *screen,
                     twin_coord_t x,
                     twin_coord_t y,
                     size_t size,
                     const char *title,
                     ui_paint_t paint,
                     ui_handler_t event,
                     ui_destroy_t destroy)
{
    return _ui_view_new(screen, TwinWindowApplication, x, y, PHONE_CONTENT_H,
                        size, title, paint, event, destroy);
}

void *ui_view_new_fullscreen(size_t size,
                             const char *title,
                             ui_paint_t paint,
                             ui_handler_t event,
                             ui_destroy_t destroy)
{
    return _ui_view_new(g_screen, TwinWindowFullscreen, 0, 0, PHONE_H, size,
                        title, paint, event, destroy);
}

void ui_view_show(ui_view_t *view)
{
    /* Paint first so the slide-in shows real content */
    twin_window_draw(view->window);
    twin_window_show(view->window);
    _changed();
}

void ui_view_close(ui_view_t *view)
{
    twin_window_close(view->window);
}

ui_view_t *ui_view_of(twin_window_t *window)
{
    for (ui_view_t *v = g_views; v && window; v = v->next_view) {
        if (v->window == window)
            return v;
    }
    return NULL;
}

void ui_view_set_layout(ui_view_t *view, ui_layout_t layout)
{
    view->layout = layout;
    if (layout)
        layout(view);
}

int ui_key_ascii(const ui_event_t *event)
{
    return event->ucs4 < 0x80 ? (int) event->ucs4 : -1;
}

ui_rect_t ui_view_bounds(const ui_view_t *view)
{
    return ui_rect(0, 0, ui_view_width(view), ui_view_height(view));
}

bool ui_view_titled(const ui_view_t *view)
{
    /* Decorations sit above the client: a title bar pushes it down */
    return twin_window_client(view->window).top > 0;
}

twin_coord_t ui_view_width(const ui_view_t *view)
{
    return ui_w(twin_window_client(view->window));
}

twin_coord_t ui_view_height(const ui_view_t *view)
{
    return ui_h(twin_window_client(view->window));
}

ui_rect_t ui_view_clip(ui_view_t *view)
{
    return twin_pixmap_get_clip(twin_window_pixmap(view->window));
}

void ui_view_invalidate(ui_view_t *view, ui_rect_t r)
{
    twin_window_t *w = view->window;
    twin_rect_t c = twin_window_client(w);
    twin_window_damage(w, r.left + c.left, r.top + c.top, r.right + c.left,
                       r.bottom + c.top);
}

void ui_view_invalidate_all(ui_view_t *view)
{
    ui_view_invalidate(view, ui_view_bounds(view));
}

void ui_view_set_pressed(ui_view_t *view,
                         int *slot,
                         int next,
                         ui_rect_t (*rect)(ui_view_t *view, int i))
{
    if (*slot == next)
        return;
    ui_rect_t r;
    if (*slot >= 0 && !ui_empty(r = rect(view, *slot)))
        ui_view_invalidate(view, r);
    *slot = next;
    if (next >= 0 && !ui_empty(r = rect(view, next)))
        ui_view_invalidate(view, r);
}

void ui_view_scroll(ui_view_t *view, ui_rect_t r, int dx, int dy)
{
    twin_window_scroll(view->window, r, (twin_coord_t) dx, (twin_coord_t) dy);
}

static void _ui_draw(twin_window_t *window)
{
    ui_view_t *v = twin_window_client_data(window);
    twin_pixmap_t *px = twin_window_pixmap(window);
    /* Views are laid out PHONE_W wide: a policy that maximizes windows
     * shows the theme background beside them */
    twin_coord_t w = ui_view_width(v);
    if (w > PHONE_W)
        ui_fill(px, ui_rect(PHONE_W, 0, w - PHONE_W, ui_view_height(v)),
                phone_theme()->bg);
    if (v->paint)
        v->paint(v, px);
}

/* Deliver to the handler; never touch @v afterwards (it may be gone) */
static bool _deliver(ui_view_t *v, ui_event_t *e)
{
    return v->event ? v->event(v, e) : false;
}

static bool _ui_event(twin_window_t *window, twin_event_t *ev)
{
    ui_view_t *v = twin_window_client_data(window);
    ui_event_t e = {0};
    twin_time_t now = twin_now();

    switch (ev->kind) {
    case TwinEventButtonDown:
        v->pressed = true;
        v->dragging = false;
        v->px0 = v->plx = ev->u.pointer.x;
        v->py0 = v->ply = ev->u.pointer.y;
        v->plt = now;
        v->pvx = v->pvy = 0;
        e.kind = UI_PRESS;
        break;
    case TwinEventMotion: {
        if (!v->pressed)
            return false;
        twin_coord_t x = ev->u.pointer.x, y = ev->u.pointer.y;
        twin_time_t dt = now - v->plt;
        if (dt > 0) {
            int vx = (x - v->plx) * 1000 / dt, vy = (y - v->ply) * 1000 / dt;
            v->pvx = (v->pvx + vx) / 2;
            v->pvy = (v->pvy + vy) / 2;
            v->plx = x;
            v->ply = y;
            v->plt = now;
        }
        if (!v->dragging &&
            (abs(x - v->px0) > DRAG_SLOP || abs(y - v->py0) > DRAG_SLOP))
            v->dragging = true;
        e.kind = v->dragging ? UI_DRAG : UI_MOVE;
        break;
    }
    case TwinEventButtonUp:
        if (!v->pressed)
            return false;
        v->pressed = false;
        /* A pause before lifting the finger kills the fling */
        if (now - v->plt > 80)
            v->pvx = v->pvy = 0;
        e.kind = v->dragging ? UI_RELEASE : UI_TAP;
        v->dragging = false;
        break;
    case TwinEventUcs4:
        e.kind = UI_KEY;
        e.ucs4 = ev->u.ucs4.ucs4;
        /* Hardware Return and Delete arrive as the soft keyboard's codes */
        if (e.ucs4 == '\n')
            e.ucs4 = UI_KEY_ENTER;
        else if (e.ucs4 == 127)
            e.ucs4 = UI_KEY_BACKSPACE;
        return _deliver(v, &e);
    case TwinEventConfigure:
        /* Sent only when the client size changed (the work area moved) */
        ui_view_invalidate_all(v);
        if (v->layout)
            v->layout(v);
        e.kind = UI_RESIZE;
        _deliver(v, &e);
        return true;
    case TwinEventActivate:
    case TwinEventDeactivate:
        e.kind = ev->kind == TwinEventActivate ? UI_SHOW : UI_HIDE;
        _deliver(v, &e);
        /* A view revealed by a closing demo window: the stack changed */
        if (ev->kind == TwinEventActivate)
            _changed();
        return true;
    case TwinEventClose:
        return false; /* let the window close */
    default:
        return false;
    }
    e.x = ev->u.pointer.x;
    e.y = ev->u.pointer.y;
    e.x0 = v->px0;
    e.y0 = v->py0;
    e.dx = e.x - v->px0;
    e.dy = e.y - v->py0;
    e.vx = v->pvx;
    e.vy = v->pvy;
    _deliver(v, &e);
    /* Pointer events are always consumed by views */
    return true;
}

static ui_scroll_t *fling_scroll;
static ui_view_t *fling_view;

static void _ui_destroy(twin_window_t *window)
{
    ui_view_t *v = twin_window_client_data(window);
    for (ui_view_t **p = &g_views; *p; p = &(*p)->next_view) {
        if (*p == v) {
            *p = v->next_view;
            break;
        }
    }
    if (v->alive)
        *v->alive = false;
    ui_view_stop_timers(v);
    ui_view_stop_tweens(v);
    if (fling_view == v) {
        fling_view = NULL;
        fling_scroll = NULL;
    }
    if (v->destroy)
        v->destroy(v);
    free(v);
    _changed();
}

/*
 * Timers and tweens.  The closure is the view's slot, so nothing extra is
 * allocated.  A callback may destroy its view: _guard_end() then reports
 * false and the slot (inside the freed view) must not be touched.
 */
static bool *_guard_begin(ui_view_t *v, bool *alive)
{
    bool *outer = v->alive;
    *alive = true;
    v->alive = alive;
    return outer;
}

static bool _guard_end(ui_view_t *v, bool alive, bool *outer)
{
    if (!alive) {
        if (outer)
            *outer = false;
        return false;
    }
    v->alive = outer;
    return true;
}

static twin_time_t _timer(twin_time_t now, void *closure)
{
    ui_slot_t *slot = closure;
    ui_view_t *v = slot->view;
    bool alive, *outer = _guard_begin(v, &alive);
    (void) now;
    /* The callback may stop its timers and start a new one, which can land
     * in this same slot: only clear the slot if it still holds this one */
    void *self = slot->handle;
    twin_time_t next = slot->fn.timer(v);
    if (!_guard_end(v, alive, outer))
        return -1; /* view destroyed: the timeout is already cleared */
    if (slot->handle != self)
        return -1; /* stopped from the callback: already cleared */
    if (next < 0)
        slot->handle = NULL;
    return next;
}

static bool _tween(twin_fixed_t value, void *closure)
{
    ui_slot_t *slot = closure;
    ui_view_t *v = slot->view;
    bool alive, *outer = _guard_begin(v, &alive);
    void *self = slot->handle;
    bool keep = slot->fn.tween(v, value);
    if (!_guard_end(v, alive, outer))
        return false;
    if (slot->handle != self)
        return false; /* cancelled from the callback, slot maybe reused */
    /* Only the final frame is exactly one: overshooting easings (OutBack)
     * pass above it on the way and must still reach the end */
    if (!keep || value == TWIN_FIXED_ONE) {
        slot->handle = NULL;
        return false;
    }
    return true;
}

static ui_slot_t *_free_slot(ui_view_t *view, ui_slot_t *slots)
{
    for (int i = 0; i < UI_VIEW_TIMERS; i++) {
        if (!slots[i].handle) {
            slots[i].view = view;
            return &slots[i];
        }
    }
    return NULL;
}

bool ui_view_every(ui_view_t *view, twin_time_t delay, ui_timer_t fn)
{
    ui_slot_t *slot = _free_slot(view, view->timers);
    if (!slot)
        return false;
    slot->fn.timer = fn;
    slot->handle = twin_set_timeout(_timer, delay, slot);
    return slot->handle != NULL;
}

bool ui_view_has_timer(ui_view_t *view, ui_timer_t fn)
{
    for (int i = 0; i < UI_VIEW_TIMERS; i++)
        if (view->timers[i].handle && view->timers[i].fn.timer == fn)
            return true;
    return false;
}

void ui_view_stop_timers(ui_view_t *view)
{
    for (int i = 0; i < UI_VIEW_TIMERS; i++) {
        if (view->timers[i].handle)
            twin_clear_timeout(view->timers[i].handle);
        view->timers[i].handle = NULL;
    }
}

void ui_view_stop_tweens(ui_view_t *view)
{
    for (int i = 0; i < UI_VIEW_TIMERS; i++) {
        if (view->tweens[i].handle)
            twin_tween_cancel(view->tweens[i].handle);
        view->tweens[i].handle = NULL;
    }
}

bool ui_view_tween(ui_view_t *view,
                   twin_time_t duration,
                   twin_ease_t ease,
                   ui_tween_t fn)
{
    ui_slot_t *slot = _free_slot(view, view->tweens);
    if (!slot)
        return false;
    slot->fn.tween = fn;
    slot->handle = twin_tween_start(duration, ease, _tween, slot);
    return slot->handle != NULL;
}

/*
 * Scrolling
 */
void ui_scroll_setup(ui_scroll_t *s, ui_rect_t area, twin_coord_t content_h)
{
    s->area = area;
    s->content_h = content_h;
    twin_coord_t m = ui_scroll_max(s);
    if (s->offset > m)
        s->offset = m;
    if (s->offset < 0)
        s->offset = 0;
}

twin_coord_t ui_scroll_max(const ui_scroll_t *s)
{
    twin_coord_t m = s->content_h - ui_h(s->area);
    return m > 0 ? m : 0;
}

void ui_scroll_to(ui_view_t *view, ui_scroll_t *s, twin_coord_t offset)
{
    twin_coord_t m = ui_scroll_max(s);
    if (offset > m)
        offset = m;
    if (offset < 0)
        offset = 0;
    twin_coord_t d = offset - s->offset;
    if (!d)
        return;
    s->offset = offset;
    ui_view_scroll(view, s->area, 0, -d);
    /* The indicator moved with the pixels: repaint its column */
    if (s->content_h > ui_h(s->area))
        ui_view_invalidate(
            view, ui_rect(s->area.right - 6, s->area.top, 6, ui_h(s->area)));
}

static twin_time_t _fling_step(ui_view_t *view)
{
    ui_scroll_t *s = fling_scroll;
    if (!s || fling_view != view || !s->flinging)
        return -1;
    twin_time_t now = twin_now();
    twin_time_t dt = now - s->last;
    s->last = now;
    twin_coord_t before = s->offset;
    ui_scroll_to(view, s, s->offset + s->velocity * dt / 1000);
    /* Exponential friction, ~5% per frame */
    s->velocity = s->velocity * 243 / 256;
    if (abs(s->velocity) < 40 || s->offset == before) {
        s->flinging = false;
        fling_scroll = NULL;
        fling_view = NULL;
        return -1;
    }
    return 16;
}

bool ui_scroll_event(ui_view_t *view, ui_scroll_t *s, const ui_event_t *e)
{
    switch (e->kind) {
    case UI_PRESS:
        s->grab = s->offset;
        s->caught = false;
        if (s->flinging) {
            s->flinging = false;
            /* A press over the list that stops a fling is not a tap: swallow
             * it and the tap it may end in */
            s->caught = ui_hit(s->area, e->x, e->y);
            return s->caught;
        }
        return false;
    case UI_TAP:
        if (!s->caught)
            return false;
        s->caught = false;
        return true;
    case UI_DRAG:
        if (!ui_hit(s->area, e->x0, e->y0) || abs(e->dy) < abs(e->dx))
            return false;
        ui_scroll_to(view, s, s->grab - e->dy);
        return true;
    case UI_RELEASE:
        if (!ui_hit(s->area, e->x0, e->y0) || abs(e->dy) < abs(e->dx))
            return false;
        if (abs(e->vy) > 200 && ui_scroll_max(s) > 0) {
            s->velocity = -e->vy;
            s->last = twin_now();
            s->flinging = true;
            if (fling_scroll && fling_scroll != s)
                fling_scroll->flinging = false;
            fling_scroll = s;
            fling_view = view;
            if (!ui_view_every(view, 16, _fling_step)) {
                s->flinging = false;
                fling_scroll = NULL;
                fling_view = NULL;
            }
        }
        return true;
    default:
        return false;
    }
}

void ui_scroll_bar(twin_pixmap_t *px, const ui_scroll_t *s)
{
    twin_coord_t h = ui_h(s->area);
    twin_coord_t m = ui_scroll_max(s);
    if (m <= 0)
        return;
    twin_coord_t bar = h * h / s->content_h;
    if (bar < 16)
        bar = 16;
    twin_coord_t y = s->area.top + (h - bar) * s->offset / m;
    ui_rrect(px, ui_rect(s->area.right - 4, y, 3, bar), 1,
             ui_alpha(phone_theme()->text_sec, 0x90));
}
