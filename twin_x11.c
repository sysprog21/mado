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
#include "twinint.h"

static void
_twin_x11_put_begin (twin_coord_t   left,
		     twin_coord_t   top,
		     twin_coord_t   right,
		     twin_coord_t   bottom,
		     void	    *closure)
{
    twin_x11_t	    *tx = closure;
    twin_coord_t    width = right - left;
    twin_coord_t    height = bottom - top;

    tx->image_y = top;
    tx->image = XCreateImage (tx->dpy, tx->visual, tx->depth, ZPixmap,
			      0, 0, width, height, 32, 0);
    if (tx->image)
    {
	tx->image->data = malloc (4 * width * height);
	if (!tx->image->data)
	{
	    XDestroyImage (tx->image);
	    tx->image = 0;
	}
    }
}

static void
_twin_x11_put_span (twin_coord_t    left,
		    twin_coord_t    top,
		    twin_coord_t    right,
		    twin_argb32_t   *pixels,
		    void	    *closure)
{
    twin_x11_t	    *tx = closure;
    twin_coord_t    width = right - left;
    twin_coord_t    ix;
    twin_coord_t    iy = top - tx->image_y;

    if (!tx->image)
	return;

    for (ix = 0; ix < width; ix++)
    {
	twin_argb32_t	pixel = *pixels++;
	
	if (tx->depth == 16)
	    pixel = twin_argb32_to_rgb16 (pixel);
	XPutPixel (tx->image, ix, iy, pixel);
    }
    if ((top + 1 - tx->image_y) == tx->image->height)
    {
	XPutImage (tx->dpy, tx->win, tx->gc, tx->image, 0, 0, 
		   left, tx->image_y, tx->image->width, tx->image->height);
	XDestroyImage (tx->image);
	tx->image = 0;
    }
}

static twin_bool_t
twin_x11_read_events (int		file,
		      twin_file_op_t	ops,
		      void		*closure)
{
    twin_x11_t		    *tx = closure;

    while (XEventsQueued (tx->dpy, QueuedAfterReading))
    {
	XEvent		ev;
	twin_event_t    tev;

	XNextEvent (tx->dpy, &ev);
	switch (ev.type) {
	case Expose:
	    twin_x11_damage (tx, (XExposeEvent *) &ev);
	    break;
	case DestroyNotify:
	    return 0;
	case ButtonPress:
	case ButtonRelease:
	    tev.u.pointer.screen_x = ev.xbutton.x;
	    tev.u.pointer.screen_y = ev.xbutton.y;
	    tev.u.pointer.button = ((ev.xbutton.state >> 8) |
				    (1 << (ev.xbutton.button-1)));
	    tev.kind = ((ev.type == ButtonPress) ? 
			TwinEventButtonDown : TwinEventButtonUp);
	    twin_screen_dispatch (tx->screen, &tev);
	    break;
	case MotionNotify:
	    tev.u.pointer.screen_x = ev.xmotion.x;
	    tev.u.pointer.screen_y = ev.xmotion.y;
	    tev.kind = TwinEventMotion;
	    tev.u.pointer.button = ev.xbutton.state >> 8;
	    twin_screen_dispatch (tx->screen, &tev);
	    break;
	}
    }
    return TWIN_TRUE;
}

static twin_bool_t
twin_x11_work (void *closure)
{
    twin_x11_t		    *tx = closure;
    
    if (twin_screen_damaged (tx->screen))
    {
	twin_x11_update (tx);
	XFlush (tx->dpy);
    }
    return TWIN_TRUE;
}

twin_x11_t *
twin_x11_create (Display *dpy, int width, int height)
{
    twin_x11_t		    *tx;
    int			    scr = DefaultScreen (dpy);
    XSetWindowAttributes    wa;
    XTextProperty	    wm_name, icon_name;
    XSizeHints		    sizeHints;
    XWMHints		    wmHints;
    XClassHint		    classHints;
    Atom		    wm_delete_window;
    static char		    *argv[] = { "xtwin", 0 };
    static int		    argc = 1;

    tx = malloc (sizeof (twin_x11_t));
    if (!tx)
	return 0;
    tx->dpy = dpy;
    tx->visual = DefaultVisual (dpy, scr);
    tx->depth = DefaultDepth (dpy, scr);

    twin_set_file (twin_x11_read_events,
		      ConnectionNumber (dpy),
		      TWIN_READ,
		      tx);
    
    twin_set_work (twin_x11_work, TWIN_WORK_REDISPLAY, tx);

    wa.background_pixmap = None;
    wa.event_mask = (KeyPressMask|
		     KeyReleaseMask|
		     ButtonPressMask|
		     ButtonReleaseMask|
		     PointerMotionMask|
		     ExposureMask|
		     StructureNotifyMask);

    wm_name.value = (unsigned char *) argv[0];
    wm_name.encoding = XA_STRING;
    wm_name.format = 8;
    wm_name.nitems = strlen (wm_name.value) + 1;
    icon_name = wm_name;

    tx->win = XCreateWindow (dpy, RootWindow (dpy, scr),
			     0, 0, width, height, 0,
			     tx->depth, InputOutput,
			     tx->visual, CWBackPixmap|CWEventMask, &wa);
    sizeHints.flags = 0;
    wmHints.flags = InputHint;
    wmHints.input = True;
    classHints.res_name = argv[0];
    classHints.res_class = argv[0];
    XSetWMProperties (dpy, tx->win,
		      &wm_name, &icon_name,
		      argv, argc,
 		      &sizeHints, &wmHints, 0);
    XSetWMProtocols (dpy, tx->win, &wm_delete_window, 1);

    tx->gc = XCreateGC (dpy, tx->win, 0, 0);
    tx->screen = twin_screen_create (width, height, _twin_x11_put_begin,
				     _twin_x11_put_span, tx);

    XMapWindow (dpy, tx->win);

    return tx;
}

void
twin_x11_destroy (twin_x11_t *tx)
{
    XDestroyWindow (tx->dpy, tx->win);
    tx->win = 0;
    twin_screen_destroy (tx->screen);
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
