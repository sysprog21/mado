/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "apps.h"

#define WIDTH 640
#define HEIGHT 480

#if defined(CONFIG_WM_DESKTOP)
/*
 * Load the background pixmap from storage.
 * The screen compositor tiles backgrounds smaller than the screen,
 * so scaling is only needed when the source is larger.  When the
 * source fits, use it directly to avoid a full-screen allocation.
 *
 * In No-WM mode the single fullscreen window covers the entire screen,
 * so the background is never visible and loading it wastes RAM.
 */
static twin_pixmap_t *load_background(twin_screen_t *screen, const char *path)
{
    twin_pixmap_t *raw_background = twin_pixmap_from_file(path);
    if (!raw_background)
        return twin_make_pattern();

    /* Source fits on screen -- use directly (tiles if smaller) */
    twin_coord_t sw = twin_screen_width(screen),
                 sh = twin_screen_height(screen);
    twin_coord_t bw = twin_pixmap_width(raw_background);
    twin_coord_t bh = twin_pixmap_height(raw_background);
    if (bw <= sw && bh <= sh)
        return raw_background;

    /* Source is larger -- scale down to screen size */
    twin_pixmap_t *scaled = twin_pixmap_create(TWIN_ARGB32, sw, sh);
    if (!scaled) {
        twin_pixmap_destroy(raw_background);
        return twin_make_pattern();
    }
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_scale(
        &m, twin_fixed_div(twin_int_to_fixed(bw), twin_int_to_fixed(sw)),
        twin_fixed_div(twin_int_to_fixed(bh), twin_int_to_fixed(sh)));
    twin_pixmap_set_transform(raw_background, &m);
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = raw_background,
    };
    twin_composite(scaled, 0, 0, &srcop, 0, 0, NULL, 0, 0, TWIN_SOURCE, sw, sh);
    twin_pixmap_destroy(raw_background);
    return scaled;
}
#endif /* CONFIG_WM_DESKTOP */

static twin_context_t *tx = NULL;

static const apps_entry_t *shell; /* the running shell, if any */

static void cleanup(void)
{
    if (shell && shell->stop)
        shell->stop();
    shell = NULL;
    twin_destroy(tx);
    tx = NULL;
}

static void sigint_handler(int sig)
{
    (void) sig;
    cleanup();
    exit(1);
}

/* A shell that owns the screen (the phone, in a mobile build) */
static const apps_entry_t *find_shell(void)
{
    for (int i = 0; i < apps_count(); i++)
        if (apps_at(i)->boot)
            return apps_at(i);
    return NULL;
}

/* Start the shell, or else every app in registry order.  Without a window
 * manager every window covers the screen, so overlapping apps would hide
 * each other: start only the first one.
 * Shared by both native and WebAssembly builds.
 */
static void init_demo_apps(twin_screen_t *screen)
{
    const apps_entry_t *sh = find_shell();
    if (sh) {
        if (sh->boot(screen)) {
            shell = sh;
            return;
        }
        fprintf(stderr, "%s: out of memory, starting the apps instead\n",
                sh->name);
    }
#if defined(CONFIG_WM_DESKTOP)
    twin_screen_set_background(screen,
                               load_background(screen, ASSET_PATH "tux.png"));
#endif
    for (int i = 0; i < apps_count(); i++) {
        const apps_entry_t *app = apps_at(i);
        if (app->boot)
            continue;
        bool started =
            app->start(screen, app->name, app->x, app->y, app->w, app->h);
#if defined(CONFIG_WM_NONE)
        if (started)
            break;
#else
        (void) started;
#endif
    }
}

int main(void)
{
    int width = WIDTH, height = HEIGHT;

    const apps_entry_t *sh = find_shell();
    if (sh) {
        width = sh->w;
        height = sh->h;
    }

    tx = twin_create(width, height);
    if (!tx)
        return 1;

    /* Register the callback function to release allocated resources when SIGINT
     * is caught or the program is about to exit.
     * This is important for long-time execution of the main loop, from which
     * the user usually exits with input events.
     */
    atexit(cleanup);
    signal(SIGINT, sigint_handler);

    /* Start application with unified API (handles native and WebAssembly) */
    twin_run(tx, init_demo_apps);

    return 0;
}
