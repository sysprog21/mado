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

int main(void)
{
    twin_sdl_t *sdl = twin_sdl_create(WIDTH, HEIGHT);

    twin_screen_set_background(sdl->screen, twin_make_pattern());

    apps_demo_start(sdl->screen, "Demo", 100, 100, 400, 400);
    apps_hello_start(sdl->screen, "Hello, World", 0, 0, 200, 200);
    apps_clock_start(sdl->screen, "Clock", 10, 10, 200, 200);
    apps_calc_start(sdl->screen, "Calculator", 100, 100, 200, 200);
    apps_line_start(sdl->screen, "Line", 0, 0, 200, 200);
    apps_spline_start(sdl->screen, "Spline", 20, 20, 400, 400);

    twin_dispatch();
    return 0;
}
