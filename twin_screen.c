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

twin_screen_t *
twin_screen_create (int		    width,
		    int		    height, 
		    twin_put_span_t put_span,
		    void	    *closure)
{
    twin_screen_t   *screen = malloc (sizeof (twin_screen_t));
    if (!screen)
	return 0;
    screen->bottom = 0;
    screen->width = width;
    screen->height = height;
    screen->damage.left = screen->damage.right = 0;
    screen->damage.top = screen->damage.bottom = 0;
    screen->damaged = NULL;
    screen->damaged_closure = NULL;
    screen->disable = 0;
#if HAVE_PTHREAD_H
    pthread_mutex_init (&screen->screen_mutex, NULL);
#endif
    screen->put_span = put_span;
    screen->closure = closure;
    return screen;
}

void
twin_screen_lock (twin_screen_t *screen)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (&screen->screen_mutex);
#endif
}

void
twin_screen_unlock (twin_screen_t *screen)
{
#if HAVE_PTHREAD_H
    pthread_mutex_unlock (&screen->screen_mutex);
#endif
}

void
twin_screen_destroy (twin_screen_t *screen)
{
    while (screen->bottom)
	twin_pixmap_hide (screen->bottom);
    free (screen);
}

void
twin_screen_register_damaged (twin_screen_t *screen, 
			      void (*damaged) (void *),
			      void *closure)
{
    screen->damaged = damaged;
    screen->damaged_closure = closure;
}

void
twin_screen_enable_update (twin_screen_t *screen)
{
    if (--screen->disable == 0)
    {
	if (screen->damage.left < screen->damage.right &&
	    screen->damage.top < screen->damage.bottom)
	{
	    if (screen->damaged)
		(*screen->damaged) (screen->damaged_closure);
	}
    }
}

void
twin_screen_disable_update (twin_screen_t *screen)
{
    screen->disable++;
}

void
twin_screen_damage (twin_screen_t *screen,
		    int left, int top, int right, int bottom)
{
    if (screen->damage.left == screen->damage.right)
    {
	screen->damage.left = left;
	screen->damage.right = right;
	screen->damage.top = top;
	screen->damage.bottom = bottom;
    }
    else
    {
	if (left < screen->damage.left)
	    screen->damage.left = left;
	if (top < screen->damage.top)
	    screen->damage.top = top;
	if (screen->damage.right < right)
	    screen->damage.right = right;
	if (screen->damage.bottom < bottom)
	    screen->damage.bottom = bottom;
    }
    if (screen->damaged && !screen->disable)
	(*screen->damaged) (screen->damaged_closure);
}

void
twin_screen_resize (twin_screen_t *screen, int width, int height)
{
    screen->width = width;
    screen->height = height;
    twin_screen_damage (screen, 0, 0, screen->width, screen->height);
}

twin_bool_t
twin_screen_damaged (twin_screen_t *screen)
{
    return (screen->damage.left < screen->damage.right &&
	    screen->damage.top < screen->damage.bottom);
}

void
twin_screen_update (twin_screen_t *screen)
{
    if (!screen->disable &&
	screen->damage.left < screen->damage.right &&
	screen->damage.top < screen->damage.bottom)
    {
	int		x = screen->damage.left;
	int		y = screen->damage.top;
	int		width = screen->damage.right - screen->damage.left;
	int		height = screen->damage.bottom - screen->damage.top;
	twin_argb32_t	*span;
        twin_pixmap_t	*p;

	/* XXX what is the maximum number of lines? */
	span = malloc (width * sizeof (twin_argb32_t));
	if (!span)
	    return;
	
	while (height--)
	{
	    memset (span, 0xff, width * sizeof (twin_argb32_t));
	    for (p = screen->bottom; p; p = p->higher)
	    {
		twin_pointer_t  dst;
		twin_source_u	src;

		int left, right;
		/* bounds check in y */
		if (y < p->y)
		    continue;
		if (p->y + p->height <= y)
		    continue;
		/* bounds check in x*/
		left = x;
		if (left < p->x)
		    left = p->x;
		right = x + width;
		if (right > p->x + p->width)
		    right = p->x + p->width;
		if (left >= right)
		    continue;
		dst.argb32 = span + (left - x);
		src.p = twin_pixmap_pointer (p, left - p->x, y - p->y);
		if (p->format == TWIN_RGB16)
		    _twin_rgb16_source_argb32 (dst, src, right - left);
		else
		    _twin_argb32_over_argb32 (dst, src, right - left);
	    }
	    (*screen->put_span) (x, y, width, span, screen->closure);
	    y++;
	}
	free (span);
	screen->damage.left = screen->damage.right = 0;
	screen->damage.top = screen->damage.bottom = 0;
    }
}
