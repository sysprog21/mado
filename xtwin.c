/*
 * $Id$
 *
 * Copyright © 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "twin_x11.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <twin_clock.h>
#include <twin_text.h>
#include <twin_demo.h>
#include <twin_hello.h>
#include <twin_calc.h>

#define WIDTH	512
#define HEIGHT	512

int
main (int argc, char **argv)
{
    Display	    *dpy = XOpenDisplay (0);
    twin_x11_t	    *x11 = twin_x11_create (dpy, WIDTH, HEIGHT);

    twin_screen_set_background (x11->screen, twin_make_pattern ());
#if 0
    twin_demo_start (x11->screen, "Demo", 100, 100, 400, 400);
    twin_text_start (x11->screen,  "Gettysburg Address",
		     0, 0, 300, 300);
    twin_hello_start (x11->screen, "Hello, World",
		      0, 0, 200, 200);
#endif
    twin_clock_start (x11->screen, "Clock", 10, 10, 200, 200);
    twin_calc_start (x11->screen, "Calculator",
		     100, 100, 200, 200);
    twin_dispatch ();
    return 0;
}
