/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Screen-wide window management services and the 'none' policy
 *
 * The 'none' policy is what a WM_NONE build runs, and what every screen
 * falls back to while it is torn down: Plain windows map where
 * the application asked, Application and Fullscreen windows cover the
 * screen, nothing is decorated, and a button press focuses the window under
 * the pointer.  It costs a few hundred bytes of code and no state, which is
 * what a single-app kiosk wants.
 */

#include "twin_private.h"

static void none_layout(twin_window_t *window,
                        twin_rect_t *geom,
                        twin_rect_t *border)
{
    twin_screen_t *screen = window->screen;
    *border = (twin_rect_t) {0, 0, 0, 0};
    if (window->style == TwinWindowPlain)
        *geom = window->request;
    else
        *geom = (twin_rect_t) {0, screen->width, 0, screen->height};
}

static void none_map(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;

    /* System bars belong to the mobile shell. */
    if (_twin_window_is_bar(window))
        return;
    _twin_window_restore(window);
    if (window->pixmap != screen->top)
        twin_pixmap_show(window->pixmap, screen, screen->top);
    /* The newest window covers the screen, so it takes the keyboard */
    if (screen->active != window->pixmap)
        twin_screen_set_active(screen, window->pixmap);
}

static void none_unmap(twin_window_t *window, bool destroying)
{
    twin_screen_t *screen = window->screen;
    (void) destroying;
    bool had_focus = screen->active && screen->active == window->pixmap;
    twin_pixmap_hide(window->pixmap);
    if (!had_focus)
        return;
    /* The keyboard goes to the topmost window still shown */
    twin_window_t *next = NULL;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w != window && w->mapped && _twin_window_shown(w) &&
            !_twin_window_is_bar(w))
            next = w;
    if (next)
        twin_screen_set_active(screen, next->pixmap);
}

static bool none_dispatch(twin_window_t *window, twin_event_t *event)
{
    if (event->kind == TwinEventButtonDown &&
        window->screen->active != window->pixmap) {
        twin_screen_set_active(window->screen, window->pixmap);
        /* Activation handlers may have destroyed it */
        if (window->dead)
            return true;
    }
    return _twin_window_deliver(window, event);
}

const twin_wm_ops_t _twin_wm_none = {
    .layout = none_layout,
    .map = none_map,
    .unmap = none_unmap,
    .dispatch = none_dispatch,
};

void _twin_screen_relayout(twin_screen_t *screen)
{
    /* Configure handlers run here: keep destroyed windows readable */
    _twin_screen_busy(screen);
    twin_screen_disable_update(screen);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w->pixmap)
            _twin_window_relayout(w);
    /* Bars belong to policies that carve a work area out for them */
    if (!screen->wm->work_area) {
        if (screen->bar_top && screen->bar_top->pixmap)
            twin_pixmap_hide(screen->bar_top->pixmap);
        if (screen->bar_bottom && screen->bar_bottom->pixmap)
            twin_pixmap_hide(screen->bar_bottom->pixmap);
    }
    if (screen->wm->refresh)
        screen->wm->refresh(screen);
    twin_screen_enable_update(screen);
    _twin_screen_idle(screen);
}

void twin_screen_set_bars(twin_screen_t *screen,
                          twin_window_t *top,
                          twin_window_t *bottom)
{
    /* One window cannot be both bars, and bars live on this screen */
    if ((top && top->screen != screen) || (bottom && bottom->screen != screen))
        return;
    if (top && top == bottom)
        bottom = NULL;
    if (top == screen->bar_top && bottom == screen->bar_bottom)
        return;
    twin_window_t *old[2] = {screen->bar_top, screen->bar_bottom};
    screen->bar_top = top;
    screen->bar_bottom = bottom;

    /* Re-place everything: the work area may have changed size.  The
     * policy stays in place (no leave/enter), so this is safe from an
     * event handler.  A bar that is no longer registered is unmapped:
     * left mapped, it would come back as a stray popup. */
    for (int i = 0; i < 2; i++) {
        if (old[i] && old[i] != top && old[i] != bottom)
            twin_window_hide(old[i]);
    }
    _twin_screen_relayout(screen);
}

void twin_screen_set_inset(twin_screen_t *screen, twin_coord_t bottom)
{
    if (bottom < 0)
        bottom = 0;
    if (bottom == screen->inset_bottom)
        return;
    screen->inset_bottom = bottom;
    _twin_screen_relayout(screen);
}

twin_rect_t twin_screen_work_area(const twin_screen_t *screen)
{
    twin_rect_t area = {0, screen->width, 0, screen->height};
    if (screen->wm->work_area)
        screen->wm->work_area(screen, &area);
    area.bottom -= screen->inset_bottom;
    if (area.bottom < area.top)
        area.bottom = area.top;
    return area;
}

void twin_window_activate(twin_window_t *window)
{
    const twin_wm_ops_t *wm = window->screen->wm;
    if (!window->mapped || _twin_window_is_bar(window))
        return;
    /* Every policy raises and focuses a window it maps */
    if (wm->activate)
        wm->activate(window);
    else
        wm->map(window);
}

bool twin_window_back(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;
    twin_event_t ev = {.kind = TwinEventBack};
    /* The handler may destroy the window: keep it readable until we're out */
    _twin_screen_busy(screen);
    bool handled = _twin_window_deliver(window, &ev);
    /* The handler may have destroyed the window instead of handling it */
    if (!handled && !window->dead)
        twin_window_close(window);
    _twin_screen_idle(screen);
    return handled;
}

void twin_screen_focus_next(twin_screen_t *screen)
{
    /* Raising the bottom window each time walks the whole stack */
    twin_window_t *bottom = NULL, *top = NULL;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next) {
        if (_twin_window_managed(w)) {
            if (!bottom)
                bottom = w;
            top = w;
        }
    }
    if (bottom && bottom != top)
        twin_window_activate(bottom);
}
