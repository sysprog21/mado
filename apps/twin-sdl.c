/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "twin_sdl.h"

#include "twin_calc.h"
#include "twin_clock.h"
#include "twin_demo.h"
#include "twin_demoline.h"
#include "twin_demospline.h"
#include "twin_hello.h"
#include "twin_text.h"

#define WIDTH 1000
#define HEIGHT 1000

int main(void)
{
    twin_sdl_t *sdl = twin_sdl_create(WIDTH, HEIGHT);

    twin_demo_start(sdl->screen, "Demo", 100, 100, 400, 400);
    twin_text_start(sdl->screen, "Gettysburg Address", 0, 0, 300, 300);
    twin_hello_start(sdl->screen, "Hello, World", 0, 0, 200, 200);
    twin_clock_start(sdl->screen, "Clock", 10, 10, 200, 200);
    twin_calc_start(sdl->screen, "Calculator", 100, 100, 200, 200);
    twin_demoline_start(sdl->screen, "Demo Line", 0, 0, 200, 200);
    twin_demospline_start(sdl->screen, "Demo Spline", 20, 20, 400, 400);

    twin_dispatch();
    return 0;
}
