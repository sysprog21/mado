/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * The 'none' window manager (WM_NONE), the minimal one: Plain windows map
 * where the application asked, Application and Fullscreen windows cover
 * the screen, nothing is decorated, and a button press focuses the window
 * under the pointer.  It costs a few hundred bytes of code and no state,
 * which is what a single-app kiosk wants.
 */

#include "twin_private.h"

const size_t _twin_wm_state_size = 0;

void _twin_wm_init(twin_screen_t *screen)
{
    (void) screen;
}

void _twin_wm_fini(twin_screen_t *screen)
{
    (void) screen;
}

void _twin_wm_layout(twin_window_t *window,
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

void _twin_wm_map(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;

    /* System bars belong to the mobile shell. */
    if (_twin_window_is_bar(window))
        return;
    _twin_window_restore(window);
    /* The window list follows the stacking order: focus moves use it */
    _twin_window_raise(window);
    if (window->pixmap != screen->top)
        twin_pixmap_show(window->pixmap, screen, screen->top);
    /* The newest window covers the screen, so it takes the keyboard */
    if (screen->active != window->pixmap)
        twin_screen_set_active(screen, window->pixmap);
}

void _twin_wm_unmap(twin_window_t *window, bool destroying)
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

bool _twin_wm_dispatch(twin_window_t *window, twin_event_t *event)
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
