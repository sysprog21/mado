/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

twin_pixmap_t *
twin_pixmap_create (twin_format_t   format,
		    twin_coord_t    width,
		    twin_coord_t    height)
{
    twin_coord_t    stride = twin_bytes_per_pixel (format) * width;
    twin_area_t	    space = (twin_area_t) stride * height;
    twin_area_t	    size = sizeof (twin_pixmap_t) + space;
    twin_pixmap_t   *pixmap = malloc (size);
    if (!pixmap)
	return 0;
    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    pixmap->x = pixmap->y = 0;
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    twin_matrix_identity(&pixmap->transform);
    pixmap->clip.left = pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
    pixmap->origin_x = pixmap->origin_y = 0;
    pixmap->stride = stride;
    pixmap->disable = 0;
    pixmap->p.v = pixmap + 1;
    memset (pixmap->p.v, '\0', space);
    return pixmap;
}

twin_pixmap_t *
twin_pixmap_create_const (twin_format_t	    format,
			  twin_coord_t	    width,
			  twin_coord_t	    height,
			  twin_coord_t	    stride,
			  twin_pointer_t    pixels)
{
    twin_pixmap_t   *pixmap = malloc (sizeof (twin_pixmap_t));
    if (!pixmap)
	return 0;
    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    pixmap->x = pixmap->y = 0;
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    twin_matrix_identity(&pixmap->transform);
    pixmap->clip.left = pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
    pixmap->origin_x = pixmap->origin_y = 0;
    pixmap->stride = stride;
    pixmap->disable = 0;
    pixmap->p = pixels;
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
    if (pixmap->disable)
	twin_screen_disable_update (screen);
    
    if (lower == pixmap)
	lower = pixmap->down;
    
    if (pixmap->screen)
	twin_pixmap_hide (pixmap);
    
    pixmap->screen = screen;
    
    if (lower)
    {
	pixmap->down = lower;
	pixmap->up = lower->up;
	lower->up = pixmap;
	if (!pixmap->up)
	    screen->top = pixmap;
    }
    else
    {
	pixmap->down = NULL;
	pixmap->up = screen->bottom;
	screen->bottom = pixmap;
	if (!pixmap->up)
	    screen->top = pixmap;
    }

    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
}

void
twin_pixmap_hide (twin_pixmap_t *pixmap)
{
    twin_screen_t   *screen = pixmap->screen;
    twin_pixmap_t   **up, **down;

    if (!screen)
	return;

    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);

    if (pixmap->up)
	down = &pixmap->up->down;
    else
	down = &screen->top;

    if (pixmap->down)
	up = &pixmap->down->up;
    else
	up = &screen->bottom;

    *down = pixmap->down;
    *up = pixmap->up;

    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    if (pixmap->disable)
	twin_screen_enable_update (screen);
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
twin_pixmap_set_origin (twin_pixmap_t *pixmap,
			twin_coord_t ox, twin_coord_t oy)
{
	pixmap->origin_x = ox;
	pixmap->origin_y = oy;
}

void
twin_pixmap_offset (twin_pixmap_t *pixmap,
		    twin_coord_t offx, twin_coord_t offy)
{
	pixmap->origin_x += offx;
	pixmap->origin_y += offy;
}

void
twin_pixmap_get_origin (twin_pixmap_t *pixmap,
			twin_coord_t *ox, twin_coord_t *oy)
{
    *ox = pixmap->origin_x;
    *oy = pixmap->origin_y;
}

void
twin_pixmap_origin_to_clip (twin_pixmap_t *pixmap)
{
    pixmap->origin_x = pixmap->clip.left;
    pixmap->origin_y = pixmap->clip.top;
}

void
twin_pixmap_clip (twin_pixmap_t *pixmap,
		  twin_coord_t	left,	twin_coord_t top,
		  twin_coord_t	right,	twin_coord_t bottom)
{
    left += pixmap->origin_x;
    right += pixmap->origin_x;
    top += pixmap->origin_y;
    bottom += pixmap->origin_y;

    if (left > pixmap->clip.left)	pixmap->clip.left = left;
    if (top > pixmap->clip.top)		pixmap->clip.top = top;
    if (right < pixmap->clip.right)	pixmap->clip.right = right;
    if (bottom < pixmap->clip.bottom)	pixmap->clip.bottom = bottom;

    if (pixmap->clip.left >= pixmap->clip.right)
	pixmap->clip.right = pixmap->clip.left = 0;
    if (pixmap->clip.top >= pixmap->clip.bottom)
	pixmap->clip.bottom = pixmap->clip.top = 0;

    if (pixmap->clip.left < 0)
	pixmap->clip.left = 0;
    if (pixmap->clip.top < 0)
	pixmap->clip.top = 0;
    if (pixmap->clip.right > pixmap->width)
	pixmap->clip.right = pixmap->width;
    if (pixmap->clip.bottom > pixmap->height)
	pixmap->clip.bottom = pixmap->height;
}

void
twin_pixmap_set_clip (twin_pixmap_t *pixmap, twin_rect_t clip)
{
    twin_pixmap_clip (pixmap, 
		      clip.left, clip.top,
		      clip.right, clip.bottom);
}


twin_rect_t
twin_pixmap_get_clip (twin_pixmap_t *pixmap)
{
    twin_rect_t clip = pixmap->clip;

    clip.left -= pixmap->origin_x;
    clip.right -= pixmap->origin_x;
    clip.top -= pixmap->origin_y;
    clip.bottom -= pixmap->origin_y;

    return clip;
}

twin_rect_t
twin_pixmap_save_clip (twin_pixmap_t *pixmap)
{
    return pixmap->clip;
}

void
twin_pixmap_restore_clip (twin_pixmap_t *pixmap, twin_rect_t rect)
{
    pixmap->clip = rect;
}

void
twin_pixmap_reset_clip (twin_pixmap_t *pixmap)
{
    pixmap->clip.left = 0;		pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;	pixmap->clip.bottom = pixmap->height;
}

void
twin_pixmap_damage (twin_pixmap_t   *pixmap,
		    twin_coord_t    left,	twin_coord_t top,
		    twin_coord_t    right,	twin_coord_t bottom)
{
    if (pixmap->screen)
	twin_screen_damage (pixmap->screen,
			    left + pixmap->x,
			    top + pixmap->y,
			    right + pixmap->x,
			    bottom + pixmap->y);
}

static twin_argb32_t
_twin_pixmap_fetch (twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    twin_pointer_t  p = twin_pixmap_pointer (pixmap, x - pixmap->x, y - pixmap->y);
    // XXX FIX FOR TRANSFORM

    if (pixmap->x <= x && x < pixmap->x + pixmap->width &&
	pixmap->y <= y && y < pixmap->y + pixmap->height)
    {
	switch (pixmap->format) {
	case TWIN_A8:
	    return *p.a8 << 24;
	case TWIN_RGB16:
	    return twin_rgb16_to_argb32 (*p.rgb16);
	case TWIN_ARGB32:
	    return *p.argb32;
	}
    }
    return 0;
}

twin_bool_t
twin_pixmap_transparent (twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    return (_twin_pixmap_fetch (pixmap, x, y) >> 24) == 0;
}

void
twin_pixmap_move (twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
    pixmap->x = x;
    pixmap->y = y;
    twin_pixmap_damage (pixmap, 0, 0, pixmap->width, pixmap->height);
}

twin_bool_t
twin_pixmap_dispatch (twin_pixmap_t *pixmap, twin_event_t *event)
{
    if (pixmap->window)
	return twin_window_dispatch (pixmap->window, event);
    return TWIN_FALSE;
}

