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

#include "twinint.h"
#include <X11/Xutil.h>

static void
_twin_x11_put_span (int		    x,
		    int		    y,
		    int		    width,
		    twin_argb32_t   *pixels,
		    void	    *closure)
{
    twin_x11_t	*tx = closure;
    XImage	*image;
    int		ix = 0;
    int		iw = width;

    image = XCreateImage (tx->dpy, tx->visual, tx->depth, ZPixmap,
			  0, 0, width, 1, 32, 0);

    image->data = malloc (4 * width);

    while (iw--)
    {
	twin_argb32_t	pixel = *pixels++;
	
	if (tx->depth == 16)
	    pixel = twin_argb32_to_rgb16 (pixel);
	XPutPixel (image, ix, 0, pixel);
	ix++;
    }
    XPutImage (tx->dpy, tx->win, tx->gc, image, 0, 0, x, y, width, 1);
    XDestroyImage (image);
}

twin_x11_t *
twin_x11_create (Display *dpy, int width, int height)
{
    twin_x11_t		    *tx;
    int			    scr = DefaultScreen (dpy);
    XSetWindowAttributes    wa;

    tx = malloc (sizeof (twin_x11_t));
    if (!tx)
	return 0;
    tx->dpy = dpy;
    tx->visual = DefaultVisual (dpy, scr);
    tx->depth = DefaultDepth (dpy, scr);

    wa.background_pixmap = None;
    wa.event_mask = (KeyPressMask|
		     KeyReleaseMask|
		     ButtonPressMask|
		     ButtonReleaseMask|
		     PointerMotionMask|
		     ExposureMask|
		     StructureNotifyMask);

    tx->win = XCreateWindow (dpy, RootWindow (dpy, scr),
			     0, 0, width, height, 0,
			     tx->depth, InputOutput,
			     tx->visual, CWBackPixmap|CWEventMask, &wa);
    tx->gc = XCreateGC (dpy, tx->win, 0, 0);
    tx->screen = twin_screen_create (width, height,
				     _twin_x11_put_span, tx);
    XMapWindow (dpy, tx->win);
    return tx;
}

void
twin_x11_destroy (twin_x11_t *tx)
{
    twin_screen_destroy (tx->screen);
    XDestroyWindow (tx->dpy, tx->win);
}

void
twin_x11_damage (twin_x11_t *tx, XExposeEvent *ev)
{
    twin_screen_damage (tx->screen, 
			ev->x, ev->y, ev->x + ev->width, ev->y + ev->height);
}

void
twin_x11_configure (twin_x11_t *tx, XConfigureEvent *ev)
{
    twin_screen_resize (tx->screen, ev->width, ev->height);
}

void
twin_x11_update (twin_x11_t *tx)
{
    twin_screen_update (tx->screen);
}
