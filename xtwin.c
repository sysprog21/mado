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

#include <X11/Xlib.h>
#include "twin.h"
#include <string.h>
#include <stdio.h>

#define D(x) twin_double_to_fixed(x)

int
main (int argc, char **argv)
{
    Display	    *dpy = XOpenDisplay (0);
    twin_x11_t	    *x11 = twin_x11_create (dpy, 512, 512);
    twin_pixmap_t   *red = twin_pixmap_create (TWIN_ARGB32, 512, 512);
    twin_pixmap_t   *blue = twin_pixmap_create (TWIN_ARGB32, 100, 100);
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, 512, 512);
    twin_operand_t  source, mask;
    twin_path_t	    *path;
    twin_path_t	    *pen;
    twin_path_t	    *stroke;
    XEvent	    ev, motion;
    twin_bool_t	    had_motion;
    int		    x, y;
    twin_fixed_t    fx, fy;
    int		    g;

    pen = twin_path_create ();
    twin_path_circle (pen, D (0.5));
#define OFF TWIN_FIXED_HALF
#if 0
    twin_path_move (pen, D(-1), D(-1));
    twin_path_draw (pen, D(1), D(-1));
    twin_path_draw (pen, D(1), D(1));
    twin_path_draw (pen, D(-1), D(1));
#endif
    
    twin_fill (red, 0x00000000, TWIN_SOURCE, 0, 0, 512, 512);
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 512, 512);

    path = twin_path_create ();
#if 0
    stroke = twin_path_create();

#define HEIGHT	twin_int_to_fixed(16)
#define LEFT	(twin_int_to_fixed (3) + OFF)
    
    fx = LEFT;
    fy = HEIGHT + OFF;
    
    twin_path_move (stroke, fx, fy);
    
    for (g = 0; g < 4000; g++)
/*    for (g = 1000; g < 2000; g++) */
/*    for (g = 2000; g < 2500; g++) */
/*     for (g = 2500; g < 3000; g++) */
/*    for (g = 3000; g < 4000; g++) */
    /* really chunky looking */
/*    for (g = 4000; g < 4327; g++) */
/*  #define WIDTH	twin_int_to_fixed(60)
    #define HEIGHT	twin_int_to_fixed(80)
 */
    {
	if (twin_has_glyph (g))
	{
	    if (fx + twin_glyph_width (g) > twin_int_to_fixed (500))
		twin_path_move (stroke, fx = LEFT, fy += HEIGHT);
	    twin_path_glyph (stroke, g);
	    fx += twin_glyph_width (g);
	}
    }

    twin_path_convolve (path, stroke, pen);

    twin_path_destroy (stroke);
#else
    fx = D(3);
    fy = D(8);
    for (g = 6; g < 36; g++)
    {
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path, D(g), D(g),
			"the quick brown fox jumps over the lazy dog.");
	twin_path_utf8 (path, D(g), D(g),
			"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
	fy += D(g);
    }
#endif
    twin_path_fill (alpha, path);
    
    twin_path_destroy (path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    512, 512);

    twin_fill (blue, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);

#if 1
    path = twin_path_create ();

    stroke = twin_path_create ();
    
    twin_path_move (stroke, D (10), D (40));
    twin_path_draw (stroke, D (40), D (40));
    twin_path_draw (stroke, D (10), D (10));
    twin_path_move (stroke, D (10), D (50));
    twin_path_draw (stroke, D (40), D (50));

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (stroke);

    twin_path_fill (alpha, path);
    twin_path_destroy (path);

    path = twin_path_create ();

    twin_path_move (path, D (50), D (50));
    twin_path_curve (path, D (70), D (70), D (80), D (70), D (100), D (50));

    twin_path_fill (alpha, path);

    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff0000ff;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
#endif

    twin_pixmap_move (red, 0, 0);
    twin_pixmap_move (blue, 100, 100);
    twin_pixmap_show (red, x11->screen, 0);
    twin_pixmap_show (blue, x11->screen, 0);
    had_motion = TWIN_FALSE;
    for (;;)
    {
	if (had_motion)
	{
	    x = motion.xmotion.x - 50;
	    y = motion.xmotion.y - 50;
	    if (motion.xmotion.state & Button1Mask)
		twin_pixmap_move (red, x, y);
	    if (motion.xmotion.state & Button3Mask)
		twin_pixmap_move (blue, x, y);
	    had_motion = TWIN_FALSE;
	}
	if (twin_screen_damaged (x11->screen))
	    twin_x11_update (x11);
	XSync (dpy, False);
	do {
	    XNextEvent (dpy, &ev);
	    switch (ev.type) {
	    case Expose:
		twin_x11_damage (x11, &ev.xexpose);
		break;
	    case ButtonPress:
		if (ev.xbutton.button == 2)
		{
		    if (red->higher == 0)
			twin_pixmap_show (red, x11->screen, 0);
		    else
			twin_pixmap_show (blue, x11->screen, 0);
		}
		break;
	    case MotionNotify:
		had_motion = TWIN_TRUE;
		motion = ev;
		break;
	    }
	} while (QLength (dpy));
    }
}

