/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "twin_sdl.h"

#include "apps_calc.h"
#include "apps_clock.h"
#include "apps_demo.h"
#include "apps_hello.h"
#include "apps_line.h"
#include "apps_spline.h"

#define WIDTH 640
#define HEIGHT 480

#define ASSET_PATH "assets/"

/**
 * Load the background pixmap from storage.
 * @filename: File name of a PNG background.
 *
 * Return a default pattern if the load of @filename fails.
 */
static twin_pixmap_t *load_background(twin_screen_t *screen,
                                      const char *filename)
{
    twin_pixmap_t *raw_background = twin_png_to_pixmap(filename, TWIN_ARGB32);
    if (!raw_background) /* Fallback to a default pattern */
        return twin_make_pattern();

    if (screen->height == raw_background->height &&
        screen->width == raw_background->width)
        return raw_background;

    /* Scale as needed. */
    twin_pixmap_t *scaled_background =
        twin_pixmap_create(TWIN_ARGB32, screen->width, screen->height);
    if (!scaled_background) {
        twin_pixmap_destroy(raw_background);
        return twin_make_pattern();
    }
    twin_fixed_t sx, sy;
    sx = twin_fixed_div(twin_int_to_fixed(raw_background->width),
                        twin_int_to_fixed(screen->width));
    sy = twin_fixed_div(twin_int_to_fixed(raw_background->height),
                        twin_int_to_fixed(screen->height));

    twin_matrix_scale(&raw_background->transform, sx, sy);
    twin_operand_t srcop = {
        .source_kind = TWIN_PIXMAP,
        .u.pixmap = raw_background,
    };
    twin_composite(scaled_background, 0, 0, &srcop, 0, 0, NULL, 0, 0,
                   TWIN_SOURCE, screen->width, screen->height);

    twin_pixmap_destroy(raw_background);

    return scaled_background;
}

int main(void)
{
    twin_sdl_t *sdl = twin_sdl_create(WIDTH, HEIGHT);

    twin_screen_set_background(
        sdl->screen, load_background(sdl->screen, ASSET_PATH "/tux.png"));

    apps_demo_start(sdl->screen, "Demo", 100, 100, 400, 400);
    apps_hello_start(sdl->screen, "Hello, World", 0, 0, 200, 200);
    apps_clock_start(sdl->screen, "Clock", 10, 10, 200, 200);
    apps_calc_start(sdl->screen, "Calculator", 100, 100, 200, 200);
    apps_line_start(sdl->screen, "Line", 0, 0, 200, 200);
    apps_spline_start(sdl->screen, "Spline", 20, 20, 400, 400);

    twin_dispatch();
    return 0;
}
