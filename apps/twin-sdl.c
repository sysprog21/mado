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
#include "apps_text.h"

#define WIDTH 1000
#define HEIGHT 1000

int main(void)
{
    twin_sdl_t *sdl = twin_sdl_create(WIDTH, HEIGHT);

    apps_demo_start(sdl->screen, "Demo", 100, 100, 400, 400);
    apps_text_start(sdl->screen, "Gettysburg Address", 0, 0, 300, 300);
    apps_hello_start(sdl->screen, "Hello, World", 0, 0, 200, 200);
    apps_clock_start(sdl->screen, "Clock", 10, 10, 200, 200);
    apps_calc_start(sdl->screen, "Calculator", 100, 100, 200, 200);
    apps_line_start(sdl->screen, "Demo Line", 0, 0, 200, 200);
    apps_spline_start(sdl->screen, "Demo Spline", 20, 20, 400, 400);

    twin_dispatch();
    return 0;
}
