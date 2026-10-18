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
static const apps_entry_t *shell; /* the shell the scene runs, if any */

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

/*
 * Scene: which window manager the demo starts under.  Kconfig picks the
 * default; MADO_SCENE=mobile|desktop|none overrides it at run time when
 * that window manager is built.
 */

static twin_wm_mode_t default_scene(void)
{
#if defined(CONFIG_DEMO_SCENE_MOBILE)
    return TwinWmMobile;
#elif defined(CONFIG_DEMO_SCENE_DESKTOP)
    return TwinWmDesktop;
#else
    return TwinWmNone;
#endif
}

static twin_wm_mode_t pick_scene(void)
{
    const char *env = getenv("MADO_SCENE");
    if (!env)
        return default_scene();
#if defined(CONFIG_WM_MOBILE)
    if (!strcmp(env, "mobile"))
        return TwinWmMobile;
#endif
#if defined(CONFIG_WM_DESKTOP)
    if (!strcmp(env, "desktop"))
        return TwinWmDesktop;
#endif
    if (!strcmp(env, "none"))
        return TwinWmNone;
    fprintf(stderr, "MADO_SCENE=%s is not built, using the default scene\n",
            env);
    return default_scene();
}

/* A shell that runs the whole scene (the phone under the mobile policy) */
static const apps_entry_t *scene_shell(twin_wm_mode_t mode)
{
    for (int i = 0; i < apps_count(); i++) {
        const apps_entry_t *app = apps_at(i);
        if (app->shell && apps_runs_under(app, mode))
            return app;
    }
    return NULL;
}

/* Start the shell of the scene, or else every app that runs under it,
 * last to first so that the first registry entries end up on top.
 * Without a window manager every window covers the screen, so overlapping
 * apps would hide each other: start only the first one.
 * Shared by both native and WebAssembly builds.
 */
static void init_demo_apps(twin_context_t *ctx)
{
    twin_screen_t *screen = twin_context_screen(ctx);

    if (!twin_screen_set_wm(screen, scene))
        scene = twin_screen_get_wm(screen);
    const apps_entry_t *sh = scene_shell(scene);
    if (sh) {
        if (sh->start(screen, 0, 0, sh->w, sh->h)) {
            shell = sh;
            return;
        }
        fprintf(stderr, "%s: out of memory, starting the apps instead\n",
                sh->name);
    }
#if defined(CONFIG_WM_DESKTOP)
    if (scene == TwinWmDesktop)
        twin_screen_set_background(
            screen, load_background(screen, ASSET_PATH "/tux.png"));
#endif
    if (scene == TwinWmNone) {
        for (int i = 0; i < apps_count(); i++) {
            const apps_entry_t *app = apps_at(i);
            if (!app->shell && apps_runs_under(app, scene) &&
                app->start(screen, app->x, app->y, app->w, app->h))
                break;
        }
        return;
    }
    for (int i = apps_count() - 1; i >= 0; i--) {
        const apps_entry_t *app = apps_at(i);
        if (!app->shell && apps_runs_under(app, scene))
            app->start(screen, app->x, app->y, app->w, app->h);
    }
}

int main(void)
{
    int width = WIDTH, height = HEIGHT;

    scene = pick_scene();
    const apps_entry_t *sh = scene_shell(scene);
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
