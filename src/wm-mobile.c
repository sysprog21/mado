/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Mobile window manager policy
 *
 * Application windows form a stack over the work area, the region between
 * the system bars.  Only the top application is visible, so the policy
 * releases the pixels of every application underneath it (eviction) and
 * asks the application to repaint when it comes back.  Steady state costs
 * one work-area surface plus the bars, no matter how deep the stack is.
 *
 * Transitions never repaint applications.  They move retained pixmaps and
 * change their opacity, and the compositor does the rest:
 *
 *  - push:    the new app slides in from the right over the old one, which
 *             drifts left by a third and dims (opacity over black).
 *  - pop:     the reverse.  The departing window's pixmap is adopted when
 *             the application destroys it, so app state can be freed at once.
 *  - dismiss: a full-screen window (lock screen) slides up and away.
 *  - swipe:   dragging from the left edge pops interactively; releasing past
 *             a third of the width asks the top window to go back
 *             (twin_window_back: unhandled, it closes).  A press at the edge
 *             is held until the finger moves: a sideways drag starts the
 *             swipe, anything else (a tap, a vertical drag) is replayed to
 *             the application, so controls along the edge keep working.
 *
 * Escape that the top application does not consume also goes back.
 *
 * Every arrangement ends in mobile_settle(), which derives the visible set
 * from the window list alone.  Transitions only perturb positions and
 * opacity in between, so interrupting one is always safe: finish the tween,
 * settle, start the next.
 */

#include <stdlib.h>

#include "twin_private.h"

#define EDGE_WIDTH 16
/* Sideways travel that turns an edge press into a swipe, and how long a
 * press may rest at the edge before it is handed to the app */
#define EDGE_SLOP 8
#define EDGE_HOLD_MS 150
#define PUSH_MS 240
#define POP_MS 220
#define DISMISS_MS 260
#define CANCEL_MS 160
#define DIM_OPACITY 0x99
#define SLIDE_SHADOW_BLUR 12
#define SLIDE_SHADOW_ALPHA 0x60

typedef enum {
    MOBILE_IDLE,
    MOBILE_PUSH,    /* front slides in over back */
    MOBILE_POP,     /* orphan slides out, back is revealed */
    MOBILE_DISMISS, /* orphan slides up */
    MOBILE_SWIPE,   /* finger drives a pop of front over back */
    MOBILE_CANCEL,  /* swipe released early: front slides home */
} twin_mobile_kind_t;

typedef struct {
    twin_mobile_kind_t kind;
    twin_tween_t *tween;
    twin_fixed_t from;     /* progress the running tween started at */
    twin_fixed_t progress; /* last applied progress */
    twin_window_t *front;  /* window whose pixmap is in front */
    twin_window_t *back;   /* window underneath */
    twin_pixmap_t *orphan; /* pixels of a destroyed window, still leaving */
    twin_coord_t swipe_x0; /* pointer x when the swipe began */
    twin_event_t edge;     /* edge press held until the finger moves */
    bool edge_pending;     /* edge holds a press not yet routed */
    twin_timeout_t *edge_timer; /* hands a resting edge press to the app */
    bool swallow; /* a press was dropped: drop until its release */
} twin_mobile_t;

#define MOBILE(screen) ((twin_mobile_t *) (screen)->wm_data)

static bool _animate(void)
{
    return !(_twin_debug & TWIN_DEBUG_NO_ANIMATION);
}

static void mobile_layout(twin_window_t *window,
                          twin_rect_t *geom,
                          twin_rect_t *border)
{
    twin_screen_t *screen = window->screen;
    twin_coord_t h = window->request.bottom - window->request.top;

    *border = (twin_rect_t) {0, 0, 0, 0};
    if (window == screen->bar_top) {
        *geom = (twin_rect_t) {0, screen->width, 0, h};
    } else if (window == screen->bar_bottom) {
        *geom = (twin_rect_t) {0, screen->width, screen->height - h,
                               screen->height};
    } else if (window->style == TwinWindowApplication) {
        *geom = twin_screen_work_area(screen);
    } else if (window->style == TwinWindowFullscreen) {
        *geom = (twin_rect_t) {0, screen->width, 0, screen->height};
    } else {
        *geom = window->request;
    }
}

static bool _is_app(const twin_window_t *w)
{
    return w->style == TwinWindowApplication && !_twin_window_is_bar(w);
}

/* Topmost mapped application window other than @skip */
static twin_window_t *_top_app(twin_screen_t *screen, twin_window_t *skip)
{
    twin_window_t *top = NULL;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w != skip && w->mapped && _is_app(w))
            top = w;
    }
    return top;
}

/* Mapped application directly below @above */
static twin_window_t *_app_below(twin_screen_t *screen, twin_window_t *above)
{
    twin_window_t *below = NULL;
    for (twin_window_t *w = screen->windows; w && w != above; w = w->wm_next) {
        if (w->mapped && _is_app(w))
            below = w;
    }
    return below;
}

static bool _fullscreen_mapped(twin_screen_t *screen)
{
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w->mapped && w->style == TwinWindowFullscreen)
            return true;
    }
    return false;
}

static void _home_position(twin_window_t *window)
{
    twin_rect_t geom, border;
    mobile_layout(window, &geom, &border);
    twin_pixmap_move(window->pixmap, geom.left, geom.top);
}

/* Show @w directly above @lower (NULL: bottom), only if not already there */
static twin_pixmap_t *_place(twin_window_t *w, twin_pixmap_t *lower)
{
    _twin_window_restore(w);
    twin_pixmap_t *p = w->pixmap;
    twin_pixmap_set_opacity(p, 0xff);
    twin_pixmap_set_shadow(p, 0, 0, 0, 0);
    _home_position(w);
    if (!p->screen || p->down != lower)
        twin_pixmap_show(p, w->screen, lower);
    return p;
}

/*
 * Derive the visible arrangement from the window list.
 * Bottom to top: top application, system bars, full-screen windows, plain
 * popups.  Everything else is hidden, and hidden applications are evicted.
 */
static void mobile_settle(twin_screen_t *screen)
{
    twin_window_t *top = _top_app(screen, NULL);
    twin_window_t *focus = top;
    twin_pixmap_t *lower = NULL;

    /* An opaque full-screen window (lock screen) hides the app and the bars
     * completely: release them too, they repaint when uncovered. */
    bool covered = false;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w->mapped && w->style == TwinWindowFullscreen && w->opaque)
            covered = true;

    twin_screen_disable_update(screen);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        /* A window being destroyed may already have lost its pixmap */
        if (!w->pixmap)
            continue;
        bool hidden = (_is_app(w) && (w != top || covered)) ||
                      (covered && _twin_window_is_bar(w));
        if (hidden) {
            twin_pixmap_hide(w->pixmap);
#if defined(CONFIG_WM_MOBILE_EVICT)
            _twin_window_evict(w);
#endif
        } else if (!w->mapped) {
            twin_pixmap_hide(w->pixmap);
        }
    }
    if (top && !covered)
        lower = _place(top, NULL);
    if (screen->bar_top && screen->bar_top->mapped && !covered)
        lower = _place(screen->bar_top, lower);
    if (screen->bar_bottom && screen->bar_bottom->mapped && !covered)
        lower = _place(screen->bar_bottom, lower);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w->mapped && w->style == TwinWindowFullscreen) {
            lower = _place(w, lower);
            focus = w;
        }
    }
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (w->mapped && w->style == TwinWindowPlain && !_twin_window_is_bar(w))
            lower = _place(w, lower);
    }
    if (focus && screen->active != focus->pixmap)
        twin_screen_set_active(screen, focus->pixmap);
    twin_screen_enable_update(screen);
}

/*
 * Apply transition progress @v (0..1).  Positions are relative to the work
 * area; the back window parallaxes by a third and dims while covered.
 */
static void _apply(twin_screen_t *screen, twin_fixed_t v)
{
    twin_mobile_t *m = MOBILE(screen);
    twin_rect_t work = twin_screen_work_area(screen);
    twin_coord_t width = work.right - work.left;
    twin_coord_t shift = twin_fixed_to_int(
        twin_fixed_mul(twin_int_to_fixed(width), v) + TWIN_FIXED_HALF);
    twin_a8_t dim =
        (twin_a8_t) (DIM_OPACITY + twin_fixed_to_int((0xff - DIM_OPACITY) * v));

    m->progress = v;
    switch (m->kind) {
    case MOBILE_PUSH: {
        /* v = 0: back fully visible; v = 1: front fully in */
        twin_pixmap_t *front = m->front->pixmap, *back = m->back->pixmap;
        twin_pixmap_move(front, work.left + width - shift, work.top);
        twin_pixmap_move(back, work.left - shift / 3, work.top);
        twin_pixmap_set_opacity(
            back,
            (twin_a8_t) (0xff - twin_fixed_to_int((0xff - DIM_OPACITY) * v)));
        break;
    }
    case MOBILE_POP:
    case MOBILE_SWIPE:
    case MOBILE_CANCEL: {
        /* v = 0: front covers back; v = 1: front gone */
        twin_pixmap_t *front =
            m->kind == MOBILE_POP ? m->orphan : m->front->pixmap;
        twin_pixmap_t *back = m->back->pixmap;
        twin_pixmap_move(front, work.left + shift, work.top);
        twin_pixmap_move(back, work.left - (width - shift) / 3, work.top);
        twin_pixmap_set_opacity(back, dim);
        break;
    }
    case MOBILE_DISMISS: {
        twin_coord_t lift = twin_fixed_to_int(
            twin_fixed_mul(twin_int_to_fixed(screen->height), v));
        twin_pixmap_move(m->orphan, 0, -lift);
        break;
    }
    default:
        break;
    }
}

static void _transition_done(twin_screen_t *screen)
{
    twin_mobile_t *m = MOBILE(screen);
    m->tween = NULL;
    m->kind = MOBILE_IDLE;
    m->front = m->back = NULL;
    if (m->orphan) {
        twin_pixmap_destroy(m->orphan);
        m->orphan = NULL;
    }
    mobile_settle(screen);
}

static bool _tween_step(twin_fixed_t value, void *closure)
{
    twin_screen_t *screen = closure;
    twin_mobile_t *m = MOBILE(screen);
    twin_fixed_t v;

    /* Map tween output onto [from, to] */
    if (m->kind == MOBILE_CANCEL)
        v = m->from - twin_fixed_mul(m->from, value);
    else
        v = m->from + twin_fixed_mul(TWIN_FIXED_ONE - m->from, value);
    _apply(screen, v);
    if (value >= TWIN_FIXED_ONE) {
        _transition_done(screen);
        return false;
    }
    return true;
}

static void _start(twin_screen_t *screen,
                   twin_mobile_kind_t kind,
                   twin_fixed_t from,
                   twin_time_t duration)
{
    twin_mobile_t *m = MOBILE(screen);
    m->kind = kind;
    m->from = from;
    /* Shorten the remaining distance proportionally */
    twin_fixed_t remain = kind == MOBILE_CANCEL ? from : TWIN_FIXED_ONE - from;
    duration = (twin_time_t) (((int64_t) duration * remain) >> 16);
    _apply(screen, from);
    m->tween =
        twin_tween_start(duration, TwinEaseOutCubic, _tween_step, screen);
    if (!m->tween)
        _transition_done(screen);
}

/* Complete whatever is running and settle */
static void _finish(twin_screen_t *screen)
{
    twin_mobile_t *m = MOBILE(screen);
    if (m->tween)
        twin_tween_finish(m->tween);
    else if (m->kind != MOBILE_IDLE)
        _transition_done(screen);
}

static void _shadow_on(twin_pixmap_t *p)
{
#if defined(CONFIG_DROP_SHADOW)
    twin_pixmap_set_shadow(p, 0, 0, SLIDE_SHADOW_BLUR, SLIDE_SHADOW_ALPHA);
#else
    (void) p;
#endif
}

static void mobile_map(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;
    twin_mobile_t *m = MOBILE(screen);

    _finish(screen);
    if (_twin_window_is_bar(window)) {
        /* A bar coming back takes its rows out of the work area again */
        _twin_screen_relayout(screen);
        return;
    }
    if (!_is_app(window)) {
        _twin_window_raise(window);
        mobile_settle(screen);
        return;
    }

    /* The window list is the stack; a new window already sits at its end,
     * so "already on top" has to mean "already on screen". */
    twin_window_t *prev = _top_app(screen, window);
    bool was_top =
        _twin_window_shown(window) && _top_app(screen, NULL) == window;
    _twin_window_raise(window);
    if (!_animate() || !prev || prev == window || was_top ||
        !prev->pixmap->screen || _fullscreen_mapped(screen)) {
        mobile_settle(screen);
        return;
    }

    /* Push: bring the new app in above the previous one, under the bars */
    _twin_window_restore(window);
    m->front = window;
    m->back = prev;
    twin_pixmap_show(window->pixmap, screen, prev->pixmap);
    _shadow_on(window->pixmap);
    /* Keys go to the app sliding in, not the one leaving */
    if (screen->active != window->pixmap)
        twin_screen_set_active(screen, window->pixmap);
    _start(screen, MOBILE_PUSH, 0, PUSH_MS);
}

static void mobile_unmap(twin_window_t *window, bool destroying)
{
    twin_screen_t *screen = window->screen;
    twin_mobile_t *m = MOBILE(screen);
    twin_fixed_t from = 0;

    if (m->kind == MOBILE_SWIPE && m->front == window && destroying) {
        /* Destroyed mid-swipe: continue from where the finger left it */
        from = m->progress;
        m->kind = MOBILE_IDLE;
        m->front = m->back = NULL;
    } else {
        _finish(screen);
    }

    if (_twin_window_is_bar(window)) {
        /* A hidden bar gives its rows to the applications */
        twin_pixmap_hide(window->pixmap);
        _twin_screen_relayout(screen);
        return;
    }

    bool visible = _twin_window_shown(window);
    twin_window_t *next = _is_app(window) ? _top_app(screen, window) : NULL;
    bool pop = _is_app(window) && next && visible;
    bool dismiss = window->style == TwinWindowFullscreen && visible;

    if (!destroying || !_animate() || !(pop || dismiss)) {
        twin_pixmap_hide(window->pixmap);
        mobile_settle(screen);
        return;
    }

    /* Adopt the pixels so the application can free its state now */
    twin_pixmap_t *orphan = window->pixmap;
    window->pixmap = NULL;
    orphan->window = NULL;
    _twin_screen_forget(screen, orphan);
    m->orphan = orphan;
    mobile_settle(screen);

    /* Settling may have hidden the window we meant to reveal (an opaque
     * full-screen window covers it): nothing to animate then. */
    if (pop && !_twin_window_shown(next)) {
        m->orphan = NULL;
        twin_pixmap_destroy(orphan);
        return;
    }

    if (pop) {
        m->back = next;
        twin_pixmap_show(orphan, screen, next->pixmap);
        _shadow_on(orphan);
        _start(screen, MOBILE_POP, from, POP_MS);
    } else {
        twin_pixmap_show(orphan, screen, screen->top);
        _start(screen, MOBILE_DISMISS, 0, DISMISS_MS);
    }
}

/* Start the interactive pop of the top app; false if there is none below */
static bool _swipe_begin(twin_screen_t *screen, twin_coord_t x0)
{
    twin_mobile_t *m = MOBILE(screen);
    twin_window_t *front = _top_app(screen, NULL);
    twin_window_t *back = front ? _app_below(screen, front) : NULL;
    if (!back || !_twin_window_restore(back))
        return false;
    m->kind = MOBILE_SWIPE;
    m->front = front;
    m->back = back;
    m->swipe_x0 = x0;
    twin_pixmap_show(back->pixmap, screen, NULL);
    _shadow_on(front->pixmap);
    _apply(screen, 0);
    return true;
}

/* Stop holding the edge press: drop it, or route it to the windows as if
 * the filter had never seen it */
static void _edge_end(twin_screen_t *screen, bool deliver)
{
    twin_mobile_t *m = MOBILE(screen);
    if (m->edge_timer)
        twin_clear_timeout(m->edge_timer);
    m->edge_timer = NULL;
    m->edge_pending = false;
    if (!deliver)
        return;
    _twin_screen_busy(screen);
    _twin_screen_route(screen, &m->edge);
    _twin_screen_idle(screen);
}

static twin_time_t _edge_hold_done(twin_time_t now, void *closure)
{
    (void) now;
    twin_screen_t *screen = closure;
    MOBILE(screen)->edge_timer = NULL; /* this timeout ends here */
    _edge_end(screen, true);
    return -1;
}

/* Edge swipe: drag from the left edge to go back */
static bool mobile_filter(twin_screen_t *screen, twin_event_t *event)
{
    twin_mobile_t *m = MOBILE(screen);
    twin_coord_t x = event->u.pointer.screen_x;
    twin_coord_t y = event->u.pointer.screen_y;
    twin_rect_t work = twin_screen_work_area(screen);
    twin_coord_t width = work.right - work.left;

    /* The rest of a press that was dropped during a transition */
    if (m->swallow) {
        if (event->kind == TwinEventButtonUp)
            m->swallow = false;
        return true;
    }

    if (m->kind == MOBILE_SWIPE) {
        twin_coord_t dx =
            min((twin_coord_t) max((twin_coord_t) (x - m->swipe_x0),
                                   (twin_coord_t) 0),
                width);
        twin_fixed_t v =
            twin_fixed_div(twin_int_to_fixed(dx), twin_int_to_fixed(width));
        _apply(screen, v); /* a release continues the pop from here */
        if (event->kind == TwinEventButtonUp) {
            if (v * 3 > TWIN_FIXED_ONE)
                twin_window_back(m->front);
            /* Still swiping: released early, or the window stayed */
            if (m->kind == MOBILE_SWIPE)
                _start(screen, MOBILE_CANCEL, v, CANCEL_MS);
        }
        return true;
    }

    if (m->edge_pending) {
        twin_coord_t dx = x - m->edge.u.pointer.screen_x;
        twin_coord_t dy = y - m->edge.u.pointer.screen_y;
        if (event->kind == TwinEventMotion) {
            if (dx > EDGE_SLOP && dx > abs(dy)) {
                _edge_end(screen, false);
                if (_swipe_begin(screen, m->edge.u.pointer.screen_x))
                    return mobile_filter(screen, event);
                _edge_end(screen, true);
                return false;
            }
            if (abs(dy) <= EDGE_SLOP && dx >= -EDGE_SLOP)
                return true; /* not decided yet */
        }
        /* A tap, or a drag that is not a swipe: it belongs to the app */
        _edge_end(screen, true);
        return false;
    }

    if (event->kind != TwinEventButtonDown)
        return false;
    /* Ignore presses while a transition runs: they would land on moving UI */
    if (m->tween) {
        m->swallow = true;
        return true;
    }
    if (x >= work.left + EDGE_WIDTH || y < work.top || y >= work.bottom ||
        !_animate() || _fullscreen_mapped(screen))
        return false;
    twin_window_t *front = _top_app(screen, NULL);
    if (!front || !_app_below(screen, front))
        return false;
    m->edge = *event;
    m->edge_pending = true;
    /* A press resting at the edge (a long press) is the app's too */
    m->edge_timer = twin_set_timeout(_edge_hold_done, EDGE_HOLD_MS, screen);
    return true;
}

/* Escape the application leaves alone goes back, like the edge swipe */
static bool mobile_dispatch(twin_window_t *window, twin_event_t *event)
{
    if (_twin_window_deliver(window, event) || window->dead)
        return true;
    if (event->kind == TwinEventUcs4 && event->u.ucs4.ucs4 == 27 &&
        _is_app(window) && _app_below(window->screen, window)) {
        twin_window_back(window);
        return true;
    }
    return false;
}

static bool mobile_enter(twin_screen_t *screen)
{
    twin_mobile_t *m = twin_calloc(1, sizeof(*m));
    if (!m)
        return false;
    screen->wm_data = m;
    screen->background_color = 0xff000000;
    return true;
}

/* A mapped window changed style or opacity: rebuild the arrangement */
static void mobile_refresh(twin_screen_t *screen)
{
    _finish(screen);
    mobile_settle(screen);
}

static void mobile_leave(twin_screen_t *screen)
{
    twin_mobile_t *m = MOBILE(screen);
    if (m->edge_timer)
        twin_clear_timeout(m->edge_timer);
    if (m->tween)
        twin_tween_cancel(m->tween);
    if (m->orphan)
        twin_pixmap_destroy(m->orphan);
    twin_free(m);
    screen->wm_data = NULL;
}

static twin_coord_t _bar_height(const twin_window_t *bar)
{
    if (!bar || !bar->mapped)
        return 0;
    return max((twin_coord_t) 0,
               (twin_coord_t) (bar->request.bottom - bar->request.top));
}

/* Mapped system bars take their requested heights off the top and bottom
 * (twin_screen_work_area() keeps the result from inverting) */
static void mobile_work_area(const twin_screen_t *screen, twin_rect_t *area)
{
    area->top = _bar_height(screen->bar_top);
    area->bottom -= _bar_height(screen->bar_bottom);
}

const twin_wm_ops_t _twin_wm_mobile = {
    .enter = mobile_enter,
    .leave = mobile_leave,
    .layout = mobile_layout,
    .map = mobile_map,
    .unmap = mobile_unmap,
    .dispatch = mobile_dispatch,
    .filter = mobile_filter,
    .refresh = mobile_refresh,
    .work_area = mobile_work_area,
};
