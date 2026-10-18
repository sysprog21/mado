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

typedef struct apps_entry {
    const char *name; /* window title and launcher label */
    const char *icon; /* TWIN_SYM_* shown by launchers */
    /* Takes over the screen and launches the other apps itself (the phone
     * shell): started alone, on a screen of its own w x h */
    bool shell;
    int x, y, w, h; /* placement requested on a desktop */
    /* Open the app's windows on @screen at @x,@y, @w x @h (the policy may
     * place them elsewhere); false when it could not start */
    bool (*start)(twin_screen_t *screen, int x, int y, int w, int h);
    void (*stop)(void); /* tear down a shell; NULL for ordinary apps */
} apps_entry_t;

int apps_count(void);
const apps_entry_t *apps_at(int idx);

#endif /* _APPS_H_ */
