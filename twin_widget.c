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

static void
_twin_widget_paint (twin_widget_t *widget)
{
    if (widget->background)
    {
	twin_pixmap_t	*pixmap = widget->window->pixmap;
	twin_coord_t	w = widget->extents.right - widget->extents.left;
	twin_coord_t	h = widget->extents.bottom - widget->extents.top;
	
	twin_fill (pixmap, widget->background, TWIN_SOURCE, 0, 0, w, h);
    }
}

twin_dispatch_result_t
_twin_widget_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    switch (event->kind) {
    case TwinEventConfigure:
	widget->extents = event->u.configure.extents;
	widget->layout = TWIN_FALSE;
	break;
    case TwinEventPaint:
	_twin_widget_paint (widget);
	widget->paint = TWIN_FALSE;
	break;
    default:
	break;
    }
    return TwinDispatchNone;
}

void
_twin_widget_init (twin_widget_t	*widget,
		   twin_box_t		*parent,
		   twin_window_t	*window,
		   twin_rect_t		preferred,
		   twin_stretch_t	hstretch,
		   twin_stretch_t	vstretch,
		   twin_dispatch_proc_t	dispatch)
{
    if (parent)
    {
	twin_widget_t   **prev;

	for (prev = &parent->children; *prev; prev = &(*prev)->next);
	widget->next = *prev;
	*prev = widget;
	window = parent->widget.window;
    }
    else
	widget->next = NULL;
    widget->window = window;
    widget->parent = parent;
    widget->paint = TWIN_TRUE;
    widget->preferred = preferred;
    widget->extents = preferred;
    widget->hstretch = hstretch;
    widget->vstretch = vstretch;
    widget->dispatch = dispatch;
    widget->background = 0x00000000;
}

void
_twin_widget_queue_paint (twin_widget_t   *widget)
{
    while (widget->parent)
    {
	if (widget->paint)
	    return;
	widget->paint = TWIN_TRUE;
	widget = &widget->parent->widget;
    }
    _twin_toplevel_queue_paint (widget);
}

void
_twin_widget_queue_layout (twin_widget_t   *widget)
{
    while (widget->parent)
    {
	if (widget->layout)
	    return;
	widget->layout = TWIN_TRUE;
	widget->paint = TWIN_TRUE;
	widget = &widget->parent->widget;
    }
    _twin_toplevel_queue_layout (widget);
}

twin_bool_t
_twin_widget_contains (twin_widget_t	*widget,
		       twin_coord_t	x,
		       twin_coord_t	y)
{
    return (0 <= x && x < _twin_widget_width(widget) && 
	    0 <= y && y < _twin_widget_height(widget)); 
}

twin_widget_t *
twin_widget_create (twin_box_t	    *parent,
		    twin_argb32_t   background,
		    twin_coord_t    width,
		    twin_coord_t    height,
		    twin_stretch_t  hstretch,
		    twin_stretch_t  vstretch)
{
    twin_widget_t   *widget = malloc (sizeof (twin_widget_t));
    twin_rect_t	    extents;

    if (!widget)
	return NULL;
    extents.left = 0;
    extents.top = 0;
    extents.right = width;
    extents.bottom = height;
    _twin_widget_init (widget, parent, 0, extents, hstretch, vstretch, 
		       _twin_widget_dispatch);
    widget->background = background;
    return widget;
}
		    
void
twin_widget_set (twin_widget_t *widget, twin_argb32_t background)
{
    widget->background = background;
    _twin_widget_queue_paint (widget);
}
