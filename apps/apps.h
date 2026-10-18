/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Registry of the demo applications that are built, and of the window
 * managers each one runs under.  The demo launcher (main.c), the phone's
 * home screen and the tests all read it, so a new app is listed everywhere
 * by adding one entry.
 */

#ifndef _APPS_H_
#define _APPS_H_

#include <stdbool.h>
#include <twin.h>

/* Window managers an app runs under: one bit per twin_wm_mode_t.  Apps
 * that create ordinary windows run under every policy (APPS_WM_ALL). */
#define APPS_WM(mode) (1u << (mode))
#define APPS_WM_ALL \
    (APPS_WM(TwinWmNone) | APPS_WM(TwinWmDesktop) | APPS_WM(TwinWmMobile))

typedef struct apps_entry {
    const char *name; /* window title and launcher label */
    const char *icon; /* TWIN_SYM_* shown by launchers */
    unsigned wm;      /* APPS_WM() bits of the policies it runs under */
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

static inline bool apps_runs_under(const apps_entry_t *app, twin_wm_mode_t mode)
{
    return app->wm & APPS_WM(mode);
}

#endif /* _APPS_H_ */
