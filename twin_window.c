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

#define TWIN_ACTIVE_BG	    0xd03b80ae
#define TWIN_INACTIVE_BG    0xff808080
#define TWIN_FRAME_TEXT	    0xffffffff
#define TWIN_ACTIVE_BORDER  0xff606060
#define TWIN_BW		    0
#define TWIN_TITLE_HEIGHT   20
#define TWIN_RESIZE_SIZE    ((TWIN_TITLE_HEIGHT + 4) / 5)
#define TWIN_TITLE_BW	    ((TWIN_TITLE_HEIGHT + 11) / 12)

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
    twin_coord_t    left, right, top, bottom;

    if (!window) return NULL;
    window->screen = screen;
    window->style = style;
    switch (window->style) {
    case TwinWindowApplication:
	left = TWIN_BW;
	top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
	right = TWIN_BW + TWIN_RESIZE_SIZE;
	bottom = TWIN_BW + TWIN_RESIZE_SIZE;
	break;
    case TwinWindowPlain:
    default:
	left = 0;
	top = 0;
	right = 0;
	bottom = 0;
	break;
    }
    width += left + right;
    height += top + bottom;
    window->client.left = left;
    window->client.right = width - right;
    window->client.top = top;
    window->client.bottom = height - bottom;
    window->pixmap = twin_pixmap_create (format, width, height);
    twin_pixmap_clip (window->pixmap,
		      window->client.left, window->client.top,
		      window->client.right, window->client.bottom);
    window->pixmap->window = window;
    twin_pixmap_move (window->pixmap, x, y);
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
	twin_pixmap_clip (window->pixmap,
			  window->client.left, window->client.top,
			  window->client.right, window->client.bottom);
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
    case TwinWindowPlain:
    default:
	size->left = size->right = size->top = size->bottom = 0;
	break;
    case TwinWindowApplication:
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
    twin_fixed_t	bw = twin_int_to_fixed (TWIN_TITLE_BW);
    twin_path_t		*path;
    twin_fixed_t	bw_2 = bw / 2;
    twin_pixmap_t	*pixmap = window->pixmap;
    twin_fixed_t	w_top = bw_2;
    twin_fixed_t	c_left = bw_2;
    twin_fixed_t	t_h = twin_int_to_fixed (window->client.top) - bw;
    twin_fixed_t	t_arc_1 = t_h / 3;
    twin_fixed_t	t_arc_2 = t_h * 2 / 3;
    twin_fixed_t	c_right = twin_int_to_fixed (window->client.right) - bw_2;
    twin_fixed_t	c_top = twin_int_to_fixed (window->client.top) - bw_2;
    
    twin_fixed_t	name_height = t_h - bw - bw_2;
    twin_fixed_t	icon_size = name_height * 8 / 10;
    twin_fixed_t	icon_y = (twin_int_to_fixed (window->client.top) - 
				  icon_size) / 2;
    twin_fixed_t	menu_x = t_arc_2;
    twin_fixed_t	text_x = menu_x + icon_size + bw;
    twin_fixed_t	text_y = icon_y + icon_size;
    twin_fixed_t	text_width;
    twin_fixed_t	title_right;
    twin_fixed_t	close_x;
    twin_fixed_t	max_x;
    twin_fixed_t	min_x;
    twin_fixed_t	resize_x;
    twin_fixed_t	resize_y;
    const char		*name;
    
    twin_pixmap_reset_clip (pixmap);
    
    twin_fill (pixmap, 0x00000000, TWIN_SOURCE, 
	       0, 0, pixmap->width, window->client.top);

    path = twin_path_create ();

		      
    /* name */
    name = window->name;
    if (!name)
	name = "Sans un nom!";
    twin_path_set_font_size (path, name_height);
    twin_path_set_font_style (path, TWIN_TEXT_OBLIQUE | TWIN_TEXT_UNHINTED);
    text_width = twin_width_utf8 (path, name);
	
    title_right = (text_x + text_width + 
		   bw + icon_size +
		   bw + icon_size +
		   bw + icon_size + 
		   t_arc_2);
    
    if (title_right < c_right)
	c_right = title_right;
    
							  
    close_x = c_right - t_arc_2 - icon_size;
    max_x = close_x - bw - icon_size;
    min_x = max_x - bw - icon_size;
    resize_x = twin_int_to_fixed (window->client.right);
    resize_y = twin_int_to_fixed (window->client.bottom);

    /* border */
     
    twin_path_move (path, c_left, c_top);
    twin_path_draw (path, c_right, c_top);
    twin_path_curve (path,
		     c_right, w_top + t_arc_1,
		     c_right - t_arc_1, w_top,
		     c_right - t_h, w_top);
    twin_path_draw (path, c_left + t_h, w_top);
    twin_path_curve (path,
		     c_left + t_arc_1, w_top,
		     c_left, w_top + t_arc_1,
		     c_left, c_top);
    twin_path_close (path);

    twin_paint_path (pixmap, TWIN_ACTIVE_BG, path);
    
    twin_paint_stroke (pixmap, TWIN_ACTIVE_BORDER, path, bw_2 * 2);

    twin_path_empty (path);
    
    twin_pixmap_clip (pixmap, 
		      twin_fixed_to_int (twin_fixed_floor (menu_x)),
		      0,
		      twin_fixed_to_int (twin_fixed_ceil (c_right - t_arc_2)),
		      window->client.top);
    
    twin_path_move (path, text_x - twin_fixed_floor (menu_x), text_y);
    twin_path_utf8 (path, window->name);
    twin_paint_path (pixmap, TWIN_FRAME_TEXT, path);

    twin_pixmap_reset_clip (pixmap);
	
    /* widgets */

    {
	twin_matrix_t	m;
	
	twin_matrix_identity (&m);
	twin_matrix_translate (&m, menu_x, icon_y);
	twin_matrix_scale (&m, icon_size, icon_size);
	twin_icon_draw (pixmap, TwinIconMenu, m);
	
	twin_matrix_identity (&m);
	twin_matrix_translate (&m, min_x, icon_y);
	twin_matrix_scale (&m, icon_size, icon_size);
	twin_icon_draw (pixmap, TwinIconMinimize, m);
	
	twin_matrix_identity (&m);
	twin_matrix_translate (&m, max_x, icon_y);
	twin_matrix_scale (&m, icon_size, icon_size);
	twin_icon_draw (pixmap, TwinIconMaximize, m);

	twin_matrix_identity (&m);
	twin_matrix_translate (&m, close_x, icon_y);
	twin_matrix_scale (&m, icon_size, icon_size);
	twin_icon_draw (pixmap, TwinIconClose, m);
	
	twin_matrix_identity (&m);
	twin_matrix_translate (&m, resize_x, resize_y);
	twin_matrix_scale (&m, 
			   twin_int_to_fixed (TWIN_TITLE_HEIGHT),
			   twin_int_to_fixed (TWIN_TITLE_HEIGHT));
	twin_icon_draw (pixmap, TwinIconResize, m);
    }

    twin_pixmap_clip (pixmap,
		      window->client.left, window->client.top,
		      window->client.right, window->client.bottom);
    twin_path_destroy (path);
}

void
twin_window_draw (twin_window_t *window)
{
    switch (window->style) {
    case TwinWindowPlain:
    default:
	break;
    case TwinWindowApplication:
	twin_window_frame (window);
	break;
    }
    if (window->draw)
	(*window->draw) (window);
}

twin_bool_t
twin_window_dispatch (twin_window_t *window, twin_event_t *event)
{
    twin_event_t    ev = *event;
    twin_bool_t	    delegate = TWIN_TRUE;

    if (!window->event)
	delegate = TWIN_FALSE;
    
    switch (ev.kind) {
    case TwinEventButtonDown:
	if (ev.u.pointer.x < window->client.left ||
	    window->client.right <= ev.u.pointer.x ||
	    ev.u.pointer.y < window->client.top ||
	    window->client.bottom <= ev.u.pointer.y)
	{
	    delegate = TWIN_FALSE;
	    break;
	}
	/* fall through... */
    case TwinEventButtonUp:
    case TwinEventMotion:
	ev.u.pointer.x -= window->client.left;
	ev.u.pointer.y -= window->client.top;
	break;
    default:
	break;
    }
    if (delegate && (*window->event) (window, &ev))
	return TWIN_TRUE;
    
    /*
     * simple window management
     */
    switch (event->kind) {
    case TwinEventButtonDown:
	twin_window_show (window);
	window->screen->button_x = event->u.pointer.x;
	window->screen->button_y = event->u.pointer.y;
	return TWIN_TRUE;
    case TwinEventButtonUp:
	window->screen->button_x = -1;
	window->screen->button_y = -1;
    case TwinEventMotion:
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
