/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Screen-wide window management services, on top of whichever policy is
 * built (wm-none.c, wm-desktop.c or wm-mobile.c)
 */

#include "twin_private.h"

void _twin_screen_relayout(twin_screen_t *screen)
{
    if (screen->closing)
        return;
    /* Configure handlers run here: keep destroyed windows readable */
    _twin_screen_busy(screen);
    twin_screen_disable_update(screen);
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w->pixmap)
            _twin_window_relayout(w);
    _twin_wm_refresh(screen);
    twin_screen_enable_update(screen);
    _twin_screen_idle(screen);
}

void twin_screen_set_bars(twin_screen_t *screen,
                          twin_window_t *top,
                          twin_window_t *bottom)
{
#if !TWIN_WM_BARS
    /* Only the mobile window manager has system bars: elsewhere the
     * windows are simply hidden */
    (void) screen;
    if (top)
        twin_window_hide(top);
    if (bottom)
        twin_window_hide(bottom);
#else
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
#endif
}

void twin_screen_set_inset(twin_screen_t *screen, twin_coord_t bottom)
{
#if TWIN_WM_BARS
    if (bottom < 0)
        bottom = 0;
    if (bottom == screen->inset_bottom)
        return;
    screen->inset_bottom = bottom;
    _twin_screen_relayout(screen);
#else
    /* Application windows cover the screen or are placed by the user */
    (void) screen;
    (void) bottom;
#endif
}

twin_rect_t twin_screen_work_area(const twin_screen_t *screen)
{
    twin_rect_t area = {0, screen->width, 0, screen->height};
#if TWIN_WM_BARS
    _twin_wm_work_area(screen, &area);
    area.bottom -= screen->inset_bottom;
    if (area.bottom < area.top)
        area.bottom = area.top;
#endif
    return area;
}

void twin_window_activate(twin_window_t *window)
{
    if (!window->mapped || _twin_window_is_bar(window) ||
        window->screen->closing)
        return;
    /* Every policy raises and focuses a window it maps */
    _twin_wm_activate(window);
}

void twin_window_back(twin_window_t *window)
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
