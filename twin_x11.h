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
#ifndef _TWIN_X11_H_
#define _TWIN_X11_H_

#include "twin.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>

typedef struct _twin_x11 {
    twin_screen_t   *screen;
    Display	    *dpy;
    Window	    win;
    GC		    gc;
    Visual	    *visual;
    int		    depth;
    pthread_t	    damage_thread;
    pthread_cond_t  damage_cond;
    pthread_t	    event_thread;
} twin_x11_t;

/*
 * twin_x11.c 
 */

twin_x11_t *
twin_x11_create (Display *dpy, int width, int height);

void
twin_x11_destroy (twin_x11_t *tx);

void
twin_x11_damage (twin_x11_t *tx, XExposeEvent *ev);

void
twin_x11_configure (twin_x11_t *tx, XConfigureEvent *ev);

void
twin_x11_update (twin_x11_t *tx);

#endif /* _TWIN_X11_H_ */
