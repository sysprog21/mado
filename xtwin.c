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

int
main (int argc, char **argv)
{
    Display	    *dpy = XOpenDisplay (0);
    twin_x11_t	    *x11 = twin_x11_create (dpy, 256, 256);
    twin_pixmap_t   *red = twin_pixmap_create (TWIN_ARGB32, 100, 100);
    twin_pixmap_t   *blue = twin_pixmap_create (TWIN_ARGB32, 100, 100);
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, 100, 100);
    twin_operand_t  source, mask;
    twin_path_t	    *path;
    twin_path_t	    *pen;
    twin_path_t	    *stroke;
    XEvent	    ev;
    int		    x, y;

    twin_fill (red, 0x80800000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill (red, 0xffffff00, TWIN_SOURCE, 25, 25, 50, 50);

    twin_fill (blue, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);

    path = twin_path_create ();

#if 1
    pen = twin_path_create ();
    twin_path_circle (pen, twin_double_to_fixed (6));
    
    stroke = twin_path_create ();
    
    twin_path_move (stroke, twin_double_to_fixed (10), twin_double_to_fixed (50));
    twin_path_draw (stroke, twin_double_to_fixed (30), twin_double_to_fixed (50));
    twin_path_draw (stroke, twin_double_to_fixed (10), twin_double_to_fixed (10));

    twin_path_convolve (path, stroke, pen);
#else
    twin_path_move (path, twin_double_to_fixed (10), twin_double_to_fixed (10));
    twin_path_circle (path, twin_double_to_fixed (10));
#endif

    twin_path_fill (alpha, path);
    twin_path_empty (path);

    twin_path_move (path, 
		    twin_double_to_fixed  (50), twin_double_to_fixed (50));
    twin_path_curve (path,
		     twin_double_to_fixed (70), twin_double_to_fixed (70),
		     twin_double_to_fixed (80), twin_double_to_fixed (70),
		     twin_double_to_fixed (100), twin_double_to_fixed (50));

    twin_path_fill (alpha, path);

    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff0000ff;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
    
    /*
    twin_fill (blue, 0x80000080, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill (blue, 0xff00c000, TWIN_SOURCE, 25, 25, 50, 50);
     */
    twin_pixmap_move (red, 20, 20);
    twin_pixmap_move (blue, 80, 80);
/*    twin_pixmap_show (red, x11->screen, 0); */
    twin_pixmap_show (blue, x11->screen, 0);
    for (;;)
    {
	if (twin_screen_damaged (x11->screen))
	    twin_x11_update (x11);
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
		x = ev.xmotion.x - 50;
		y = ev.xmotion.y - 50;
		if (ev.xmotion.state & Button1Mask)
		    twin_pixmap_move (red, x, y);
		if (ev.xmotion.state & Button3Mask)
		    twin_pixmap_move (blue, x, y);
		break;
	    }
	} while (QLength (dpy));
    }
}

