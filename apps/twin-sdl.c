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
    // Display	    *dpy = XOpenDisplay (0);
    twin_x11_t *x11;

    x11 = twin_x11_create(WIDTH, HEIGHT);

#if 1
    twin_demo_start(x11->screen, "Demo", 100, 100, 400, 400);
#endif
#if 1
    twin_text_start(x11->screen, "Gettysburg Address", 0, 0, 300, 300);
#endif
#if 1
    twin_hello_start(x11->screen, "Hello, World", 0, 0, 200, 200);
#endif
#if 1
    twin_clock_start(x11->screen, "Clock", 10, 10, 200, 200);
#endif
#if 1
    twin_calc_start(x11->screen, "Calculator", 100, 100, 200, 200);
#endif
#if 1
    twin_demoline_start(x11->screen, "Demo Line", 0, 0, 200, 200);
#endif
#if 1
    twin_demospline_start(x11->screen, "Demo Spline", 20, 20, 400, 400);
#endif
    twin_dispatch();
    return 0;
}
