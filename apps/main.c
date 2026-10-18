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

#if defined(CONFIG_DEMO_CELLPHONE)
#include "phone.h"
#endif

#define WIDTH 640
#define HEIGHT 480

#define ASSET_PATH "assets/"

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
    twin_pixmap_t *raw_background = twin_pixmap_from_file(path, TWIN_ARGB32);
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

static twin_wm_mode_t scene;

static void cleanup(void)
{
#if defined(CONFIG_DEMO_CELLPHONE)
    if (scene == TwinWmMobile)
        phone_shutdown();
#endif
    twin_destroy(tx);
    tx = NULL;
}

static void sigint_handler(int sig)
{
    (void) sig;
    cleanup();
    exit(1);
}

/*
 * Scene: which window manager the demo starts under.  Kconfig picks the
 * default; MADO_SCENE=mobile|desktop|none overrides it at run time.
 */

static twin_wm_mode_t pick_scene(void)
{
    const char *env = getenv("MADO_SCENE");
    if (env) {
        if (!strcmp(env, "mobile"))
            return TwinWmMobile;
        if (!strcmp(env, "desktop"))
            return TwinWmDesktop;
        if (!strcmp(env, "none"))
            return TwinWmNone;
    }
#if defined(CONFIG_DEMO_SCENE_MOBILE)
    return TwinWmMobile;
#elif defined(CONFIG_DEMO_SCENE_DESKTOP)
    return TwinWmDesktop;
#else
    return TwinWmNone;
#endif
}

static void init_desktop_apps(twin_screen_t *screen)
{
    (void) screen;
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
}

/* Without a window manager every window covers the screen, so overlapping
 * demos would hide each other: start only the first one.
 */
static void init_single_app(twin_screen_t *screen)
{
    (void) screen;
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
}

/* Initialize demo applications based on build configuration.
 * Shared by both native and WebAssembly builds.
 */
static void init_demo_apps(twin_context_t *ctx)
{
    twin_screen_t *screen = twin_context_screen(ctx);

#if defined(CONFIG_DEMO_CELLPHONE)
    if (scene == TwinWmMobile) {
        if (phone_boot(screen))
            return;
        fprintf(stderr,
                "cellphone: out of memory, showing the classic demos\n");
    }
#endif
    if (!twin_screen_set_wm(screen, scene))
        scene = twin_screen_get_wm(screen);
    if (scene == TwinWmDesktop) {
#if defined(CONFIG_WM_DESKTOP)
        twin_screen_set_background(
            screen, load_background(screen, ASSET_PATH "/tux.png"));
#endif
        init_desktop_apps(screen);
    } else {
        init_single_app(screen);
    }
}

int main(void)
{
    int width = WIDTH, height = HEIGHT;

    scene = pick_scene();
#if defined(CONFIG_DEMO_CELLPHONE)
    if (scene == TwinWmMobile) {
        width = PHONE_W;
        height = PHONE_H;
    }
#else
    if (scene == TwinWmMobile)
        scene = TwinWmDesktop;
#endif

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

#if defined(CONFIG_CURSOR)
    int hx, hy;
    twin_pixmap_t *cursor = twin_make_cursor(&hx, &hy);
    if (cursor)
        twin_screen_set_cursor(twin_context_screen(tx), cursor, hx, hy);
#endif

    /* Start application with unified API (handles native and WebAssembly) */
    twin_run(tx, init_demo_apps);

    return 0;
}
