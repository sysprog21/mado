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

twin_pixmap_t *
twin_pixmap_create (twin_format_t   format,
		    twin_coord_t    width,
		    twin_coord_t    height)
{
    twin_coord_t    stride = twin_bytes_per_pixel (format) * width;
    twin_area_t	    size = sizeof (twin_pixmap_t) + (twin_area_t) stride * height;
    twin_pixmap_t   *pixmap = malloc (size);
    if (!pixmap)
	return 0;
    pixmap->screen = 0;
    pixmap->higher = 0;
    pixmap->x = pixmap->y = 0;
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    pixmap->stride = stride;
    pixmap->disable = 0;
    pixmap->p.v = pixmap + 1;
    return pixmap;
}

void
twin_pixmap_destroy (twin_pixmap_t *pixmap)
{
    if (pixmap->screen)
	twin_pixmap_hide (pixmap);
    free (pixmap);
}

void
twin_pixmap_show (twin_pixmap_t	*pixmap, 
		  twin_screen_t	*screen,
		  twin_pixmap_t	*lower)
{
    twin_pixmap_t   **higherp;
    
    twin_screen_lock (screen);

    if (pixmap->disable)
	twin_screen_disable_update (screen);
    
    if (pixmap->screen)
	twin_pixmap_hide (pixmap);
    
    pixmap->screen = screen;
    if (lower)
	higherp = &lower->higher;
    else
	higherp = &screen->bottom;
    pixmap->higher = *higherp;
    *higherp = pixmap;
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
    twin_screen_unlock (screen);
}

void
twin_pixmap_hide (twin_pixmap_t *pixmap)
{
    twin_screen_t   *screen = pixmap->screen;
    twin_pixmap_t   **higherp;

    if (!screen)
	return;
    twin_screen_lock (screen);
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
    for (higherp = &screen->bottom; *higherp != pixmap; higherp = &(*higherp)->higher)
	;
    *higherp = pixmap->higher;
    pixmap->screen = 0;
    pixmap->higher = 0;
    if (pixmap->disable)
	twin_screen_enable_update (screen);
    twin_screen_unlock (screen);
}

twin_pointer_t
twin_pixmap_pointer (twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    twin_pointer_t  p;

    p.b = (pixmap->p.b + 
	   y * pixmap->stride + 
	   x * twin_bytes_per_pixel(pixmap->format));
    return p;
}

void
twin_pixmap_enable_update (twin_pixmap_t *pixmap)
{
    if (--pixmap->disable == 0)
    {
	if (pixmap->screen)
	    twin_screen_enable_update (pixmap->screen);
    }
}

void
twin_pixmap_disable_update (twin_pixmap_t *pixmap)
{
    if (pixmap->disable++ == 0)
    {
	if (pixmap->screen)
	    twin_screen_disable_update (pixmap->screen);
    }
}

void
twin_pixmap_damage (twin_pixmap_t *pixmap,
		    twin_coord_t x1, twin_coord_t y1,
		    twin_coord_t x2, twin_coord_t y2)
{
    if (pixmap->screen)
	twin_screen_damage (pixmap->screen,
			    x1 + pixmap->x,
			    y1 + pixmap->y,
			    x2 + pixmap->x,
			    y2 + pixmap->y);
}

void
twin_pixmap_lock (twin_pixmap_t *pixmap)
{
    if (pixmap->screen)
	twin_screen_lock (pixmap->screen);
}

void
twin_pixmap_unlock (twin_pixmap_t *pixmap)
{
    if (pixmap->screen)
	twin_screen_unlock (pixmap->screen);
}

void
twin_pixmap_move (twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
    pixmap->x = x;
    pixmap->y = y;
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
}
