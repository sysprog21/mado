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

#define TWIN_ACTIVE_BG	    0xff3b80ae
#define TWIN_INACTIVE_BG    0xff808080
#define TWIN_FRAME_TEXT	    0xffffffff
#define TWIN_ACTIVE_BORDER  0xff000000
#define TWIN_BW		    3
#define TWIN_TITLE_HEIGHT   18

twin_window_t *
twin_window_create (twin_screen_t	*screen,
		    twin_format_t	format,
		    twin_window_style_t style,
		    twin_coord_t	x,
		    twin_coord_t	y,
		    twin_coord_t	width,
		    twin_coord_t	height)
{
    twin_window_t   *window = malloc (sizeof (twin_window_t));

    if (!window) return NULL;
    window->screen = screen;
    window->pixmap = twin_pixmap_create (format, width, height);
    window->pixmap->window = window;
    twin_pixmap_move (window->pixmap, x, y);
    window->style = style;
    window->client.left = TWIN_BW;
    window->client.top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
    window->client.right = width - TWIN_BW;
    window->client.bottom = height - TWIN_BW;
    window->damage.left = window->damage.right = 0;
    window->damage.top = window->damage.bottom = 0;
    window->client_data = 0;
    window->name = 0;
    
    window->draw = 0;
    window->event = 0;
    window->destroy = 0;
    return window;
}

void
twin_window_destroy (twin_window_t *window)
{
    twin_window_hide (window);
    twin_pixmap_destroy (window->pixmap);
    if (window->name) free (window->name);
    free (window);
}

void
twin_window_show (twin_window_t *window)
{
    if (window->pixmap != window->screen->top)
	twin_pixmap_show (window->pixmap, window->screen, window->screen->top);
}

void
twin_window_hide (twin_window_t *window)
{
    twin_pixmap_hide (window->pixmap);
}

void
twin_window_configure (twin_window_t	    *window,
		       twin_window_style_t  style,
		       twin_coord_t	    x,
		       twin_coord_t	    y,
		       twin_coord_t	    width,
		       twin_coord_t	    height)
{
    twin_bool_t	need_repaint = TWIN_FALSE;
    
    twin_pixmap_disable_update (window->pixmap);
    if (style != window->style)
    {
	window->style = style;
	need_repaint = TWIN_TRUE;
    }
    if (width != window->pixmap->width || height != window->pixmap->height)
    {
	twin_pixmap_t	*old = window->pixmap;
	int		i;

	window->pixmap = twin_pixmap_create (old->format, width, height);
	window->pixmap->window = window;
	twin_pixmap_move (window->pixmap, x, y);
	if (old->screen)
	    twin_pixmap_show (window->pixmap, window->screen, old);
	for (i = 0; i < old->disable; i++)
	    twin_pixmap_disable_update (window->pixmap);
	twin_pixmap_destroy (old);
    }
    if (x != window->pixmap->x || y != window->pixmap->y)
	twin_pixmap_move (window->pixmap, x, y);
    if (need_repaint)
	twin_window_draw (window);
    twin_pixmap_enable_update (window->pixmap);
}

void
twin_window_style_size (twin_window_style_t style,
			twin_rect_t	    *size)
{
    switch (style) {
    case WindowPlain:
	size->left = size->right = size->top = size->bottom = 0;
	break;
    case WindowApplication:
	size->left = TWIN_BW;
	size->right = TWIN_BW;
	size->top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
	size->bottom = TWIN_BW;
	break;
    }
}

void
twin_window_set_name (twin_window_t	*window,
		      const char	*name)
{
    if (window->name) free (window->name);
    window->name = malloc (strlen (name) + 1);
    if (window->name) strcpy (window->name, name);
    twin_window_draw (window);
}

static void
twin_window_frame (twin_window_t *window)
{
    twin_coord_t	bw = window->client.left;
    twin_path_t		*path;
    twin_coord_t	name_height;
    twin_text_metrics_t	m;
    twin_pixmap_t	*pixmap = window->pixmap;

    /* border */
     
    twin_fill (pixmap, TWIN_ACTIVE_BORDER,
	       TWIN_SOURCE,
	       0, 0, pixmap->width, bw);
    twin_fill (pixmap, TWIN_ACTIVE_BORDER,
	       TWIN_SOURCE,
	       0, bw, bw, pixmap->height - bw);
    twin_fill (pixmap, TWIN_ACTIVE_BORDER,
	       TWIN_SOURCE,
	       window->client.right, bw, pixmap->width, pixmap->height - bw);
    twin_fill (pixmap, TWIN_ACTIVE_BORDER,
	       TWIN_SOURCE,
	       window->client.left, window->client.top - bw,
	       window->client.right, window->client.top);
    twin_fill (pixmap, TWIN_ACTIVE_BORDER,
	       TWIN_SOURCE,
	       0, window->client.bottom,
	       pixmap->width, pixmap->height);
    
    /* name background */
    twin_fill (pixmap, TWIN_ACTIVE_BG,
	       TWIN_SOURCE,
	       bw, bw, pixmap->width - bw, window->client.top - bw);
	       
    /* name */
    if (window->name)
    {
	path = twin_path_create ();
	name_height = window->client.top - bw * 4;
	if (name_height < 1) 
	    name_height = 1;
	twin_path_translate (path, twin_int_to_fixed (bw*2),
			     twin_int_to_fixed (bw*2));
	twin_path_set_font_size (path, twin_int_to_fixed (name_height));
	twin_path_set_font_style (path, TWIN_TEXT_OBLIQUE);
	twin_text_metrics_utf8 (path, window->name, &m);
	twin_path_move (path, 0, m.font_ascent);
	twin_path_utf8 (path, window->name);
	twin_paint_path (pixmap, TWIN_FRAME_TEXT, path);
	twin_path_destroy (path);
    }
}

void
twin_window_draw (twin_window_t *window)
{
    switch (window->style) {
    case WindowPlain:
	break;
    case WindowApplication:
	twin_window_frame (window);
	break;
    }
    if (window->draw)
	(*window->draw) (window);
}

twin_bool_t
twin_window_dispatch (twin_window_t *window, twin_event_t *event)
{
    if (window->event && (*window->event) (window, event))
	return TWIN_TRUE;
    switch (event->kind) {
    case EventButtonDown:
	twin_window_show (window);
	window->screen->button_x = event->u.pointer.x;
	window->screen->button_y = event->u.pointer.y;
	return TWIN_TRUE;
    case EventButtonUp:
	window->screen->button_x = -1;
	window->screen->button_y = -1;
    case EventMotion:
	if (window->screen->button_x >= 0)
	{
	    twin_coord_t    x, y;

	    x = event->u.pointer.screen_x - window->screen->button_x;
	    y = event->u.pointer.screen_y - window->screen->button_y;
	    twin_window_configure (window, 
				   window->style,
				   x, y,
				   window->pixmap->width,
				   window->pixmap->height);
	}
	return TWIN_TRUE;
    default:
	break;
    }
    return TWIN_FALSE;
}
