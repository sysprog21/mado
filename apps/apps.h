/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Registry of the demo applications that are built.  The demo launcher
 * (main.c), the phone's home screen and the tests all read it, so a new app
 * is listed everywhere by adding one entry.
 *
 * Which window managers an app runs under is settled at build time, like
 * the window manager itself: an app runs under all of them unless its
 * Kconfig option depends on one (DEMO_CELLPHONE depends on WM_MOBILE), and
 * its entry is compiled in only when it can run.
 */

#ifndef _APPS_H_
#define _APPS_H_

#include <stdbool.h>
#include <twin.h>

/* Open the app's windows on @screen at @x,@y, @w x @h titled @name (the
 * policy may place them elsewhere); false when it could not start */
typedef bool apps_start_fn(twin_screen_t *screen,
                           const char *name,
                           int x,
                           int y,
                           int w,
                           int h);

typedef struct apps_entry {
    const char *name; /* window title and launcher label */
    const char *icon; /* TWIN_SYM_* shown by launchers */
    int x, y, w, h;   /* placement requested on a desktop; a shell's screen */
    apps_start_fn *start;
    /* A shell takes over a screen of its own w x h and launches the other
     * apps itself (the phone): it has boot and stop instead of start */
    bool (*boot)(twin_screen_t *screen);
    void (*stop)(void);
} apps_entry_t;

int apps_count(void);
const apps_entry_t *apps_at(int idx);

/* Bundled images and vectors, relative to the working directory */
#define ASSET_PATH "assets/"

apps_start_fn apps_animation_start, apps_calc_start, apps_clock_start,
    apps_image_start, apps_multi_start, apps_spline_start;

#endif /* _APPS_H_ */
