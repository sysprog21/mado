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

#include "apps_animation.h"
#include "apps_calc.h"
#include "apps_clock.h"
#include "apps_image.h"
#include "apps_multi.h"
#include "apps_spline.h"

#define WIDTH 640
#define HEIGHT 480

#define ASSET_PATH "assets/"

#if defined(CONFIG_WINDOW_MANAGER)
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
    twin_pixmap_t *raw_background = twin_pixmap_from_file(path, TWIN_ARGB32);
    if (!raw_background)
        return twin_make_pattern();

    /* Source fits on screen -- use directly (tiles if smaller) */
    if (raw_background->width <= screen->width &&
        raw_background->height <= screen->height)
        return raw_background;

    /* Source is larger -- scale down to screen size */
    twin_pixmap_t *scaled =
        twin_pixmap_create(TWIN_ARGB32, screen->width, screen->height);
    if (!scaled) {
        twin_pixmap_destroy(raw_background);
        return twin_make_pattern();
    }
    twin_fixed_t sx = twin_fixed_div(twin_int_to_fixed(raw_background->width),
                                     twin_int_to_fixed(screen->width));
    twin_fixed_t sy = twin_fixed_div(twin_int_to_fixed(raw_background->height),
                                     twin_int_to_fixed(screen->height));
    twin_matrix_scale(&raw_background->transform, sx, sy);
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = raw_background,
    };
    twin_composite(scaled, 0, 0, &srcop, 0, 0, NULL, 0, 0, TWIN_SOURCE,
                   screen->width, screen->height);
    twin_pixmap_destroy(raw_background);
    return scaled;
}
#endif /* CONFIG_WINDOW_MANAGER */

static twin_context_t *tx = NULL;

static void cleanup(void)
{
    twin_destroy(tx);
    tx = NULL;
}

static void sigint_handler(int sig)
{
    (void) sig;
    cleanup();
    exit(1);
}

/* Initialize demo applications based on build configuration.
 * Shared by both native and WebAssembly builds.
 */
static void init_demo_apps(twin_context_t *ctx)
{
    twin_screen_t *screen = ctx->screen;
#if defined(CONFIG_WINDOW_MANAGER)
#if defined(CONFIG_DEMO_MULTI)
    apps_multi_start(screen, "Demo", 100, 100, 400, 400);
#endif
#if defined(CONFIG_DEMO_CLOCK)
    apps_clock_start(screen, "Clock", 10, 10, 200, 200);
#endif
#if defined(CONFIG_DEMO_CALCULATOR)
    apps_calc_start(screen, "Calculator", 100, 100, 200, 200);
#endif
#if defined(CONFIG_DEMO_SPLINE)
    apps_spline_start(screen, "Spline", 20, 20, 400, 400);
#endif
#if defined(CONFIG_DEMO_ANIMATION)
    apps_animation_start(screen, "Viewer", ASSET_PATH "nyancat.gif", 20, 20);
#endif
#if defined(CONFIG_DEMO_IMAGE)
    apps_image_start(screen, "Viewer", 20, 20);
#endif
#else
    /*
     * Without the window manager every window is pinned at (0,0), so launching
     * multiple demos leaves later windows permanently covering earlier ones.
     * Start a single demo in a deterministic order instead.
     */
#if defined(CONFIG_DEMO_CLOCK)
    apps_clock_start(screen, "Clock", 10, 10, 200, 200);
#elif defined(CONFIG_DEMO_CALCULATOR)
    apps_calc_start(screen, "Calculator", 100, 100, 200, 200);
#elif defined(CONFIG_DEMO_SPLINE)
    apps_spline_start(screen, "Spline", 20, 20, 400, 400);
#elif defined(CONFIG_DEMO_ANIMATION)
    apps_animation_start(screen, "Viewer", ASSET_PATH "nyancat.gif", 20, 20);
#elif defined(CONFIG_DEMO_IMAGE)
    apps_image_start(screen, "Viewer", 20, 20);
#endif
#endif
    twin_screen_set_active(screen, screen->top);
}

int main(void)
{
    tx = twin_create(WIDTH, HEIGHT);
    if (!tx)
        return 1;

    /* Register the callback function to release allocated resources when SIGINT
     * is caught or the program is about to exit.
     * This is important for long-time execution of the main loop, from which
     * the user usually exits with input events.
     */
    atexit(cleanup);
    signal(SIGINT, sigint_handler);

#if defined(CONFIG_CURSOR)
    int hx, hy;
    twin_pixmap_t *cursor = twin_make_cursor(&hx, &hy);
    if (cursor)
        twin_screen_set_cursor(tx->screen, cursor, hx, hy);
#endif

#if defined(CONFIG_WINDOW_MANAGER)
    twin_screen_set_background(
        tx->screen, load_background(tx->screen, ASSET_PATH "/tux.png"));
#endif

    /* Start application with unified API (handles native and WebAssembly) */
    twin_run(tx, init_demo_apps);

    return 0;
}
