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

void
_twin_box_init (twin_box_t		*box,
		twin_box_t		*parent,
		twin_window_t		*window,
		twin_layout_t		layout,
		twin_dispatch_proc_t	dispatch)
{
    static twin_rect_t	preferred = { 0, 0, 0, 0 };
    _twin_widget_init (&box->widget, parent, window, preferred, 0, 0, dispatch);
    box->layout = layout;
    box->children = 0;
}

static twin_dispatch_result_t
_twin_box_query_geometry (twin_box_t *box)
{
    twin_widget_t   *widget;
    twin_event_t    ev;
    twin_coord_t    w, h;
    twin_coord_t    c_w, c_h;

    w = 0; h = 0;
    /*
     * Find preferred geometry
     */
    for (widget = box->children; widget; widget = widget->next)
    {
	ev.kind = TwinEventQueryGeometry;
	(*widget->dispatch) (widget, &ev);
	c_w = widget->preferred.right - widget->preferred.left;
	c_h = widget->preferred.bottom - widget->preferred.top;
	if (box->layout == TwinLayoutHorz)
	{
	    w += c_w;
	    if (c_h > h)
		h = c_h;
	}
	else
	{
	    h += c_h;
	    if (c_w > w)
		w = c_w;
	}
    }
    box->widget.preferred.left = 0;
    box->widget.preferred.top = 0;
    box->widget.preferred.right = w;
    box->widget.preferred.bottom = h;
    return TwinDispatchNone;
}

static twin_dispatch_result_t
_twin_box_configure (twin_box_t *box)
{
    twin_coord_t    w = box->widget.extents.right - box->widget.extents.left;
    twin_coord_t    h = box->widget.extents.bottom - box->widget.extents.top;
    twin_coord_t    actual;
    twin_coord_t    pref;
    twin_coord_t    delta;
    twin_coord_t    delta_remain;
    twin_coord_t    stretch = 0;
    twin_coord_t    pos = 0;
    twin_widget_t   *child;
    twin_dispatch_result_t  result = TwinDispatchNone;
    
    if (box->layout == TwinLayoutHorz)
    {
	for (child = box->children; child; child = child->next)
	    stretch += child->hstretch;
	actual = w;
	pref = box->widget.preferred.right - box->widget.preferred.left;
    }
    else
    {
	for (child = box->children; child; child = child->next)
	    stretch += child->vstretch;
	actual = h;
	pref = box->widget.preferred.bottom - box->widget.preferred.top;
    }
    if (!stretch) stretch = 1;
    delta = delta_remain = actual - pref;
    for (child = box->children; child; child = child->next)
    {
	twin_event_t	ev;
	twin_coord_t    stretch_this;
	twin_coord_t    delta_this;
	twin_rect_t	extents;
	
	if (!child->next)
	    delta_this = delta_remain;
	else
	{
	    if (box->layout == TwinLayoutHorz)
		stretch_this = child->hstretch;
	    else
		stretch_this = child->vstretch;
	    delta_this = delta * stretch_this / stretch;
	}
	if (delta_remain < 0)
	{
	    if (delta_this < delta_remain)
		delta_this = delta_remain;
	}
	else
	{
	    if (delta_this > delta_remain)
		delta_this = delta_remain;
	}
	delta_remain -= delta_this;
	if (box->layout == TwinLayoutHorz)
	{
	    twin_coord_t    child_w = (child->preferred.right -
				       child->preferred.left);
	    extents.top = 0;
	    extents.bottom = h;
	    extents.left = pos;
	    pos = extents.right = pos + child_w + delta_this;
	}
	else
	{
	    twin_coord_t    child_h = (child->preferred.bottom -
				       child->preferred.top);
	    extents.left = 0;
	    extents.right = w;
	    extents.top = pos;
	    pos = extents.bottom = pos + child_h + delta_this;
	}
	ev.kind = TwinEventConfigure;
	ev.u.configure.extents = extents;
	(*child->dispatch) (child, &ev);
	result = TwinDispatchPaint;
    }
    return result;
}

static twin_widget_t *
_twin_box_xy_to_widget (twin_box_t *box, twin_coord_t x, twin_coord_t y)
{
    twin_widget_t   *widget;

    for (widget = box->children; widget; widget = widget->next)
    {
	if (widget->extents.left <= x && x < widget->extents.right &&
	    widget->extents.top <= y && y < widget->extents.bottom)
	    return widget;
    }
    return NULL;
}

twin_dispatch_result_t
_twin_box_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_box_t	    *box = (twin_box_t *) widget;
    twin_event_t    ev;
    twin_widget_t   *child;

    switch (event->kind) {
    case TwinEventQueryGeometry:
	return _twin_box_query_geometry (box);
    case TwinEventConfigure:
	_twin_widget_dispatch (widget, event);
	return _twin_box_configure (box);
    case TwinEventButtonDown:
	box->button_down = _twin_box_xy_to_widget (box, 
						   event->u.pointer.x,
						   event->u.pointer.y);
	if (box->button_down && box->button_down->want_focus)
	    box->focus = box->button_down;
	/* fall through ... */
    case TwinEventButtonUp:
    case TwinEventMotion:
	if (box->button_down)
	{
	    child = box->button_down;
	    ev = *event;
	    ev.u.pointer.x -= child->extents.left;
	    ev.u.pointer.y -= child->extents.top;
	    return (*box->button_down->dispatch) (child, &ev);
	}
	break;
    case TwinEventKeyDown:
    case TwinEventKeyUp:
    case TwinEventUcs4:
	if (box->focus)
	    return (*box->focus->dispatch) (box->focus, event);
	break;
    case TwinEventPaint:
	box->widget.paint = TWIN_FALSE;
	for (child = box->children; child; child = child->next)
	    if (child->paint)
	    {
		twin_pixmap_t	*pixmap = box->widget.window->pixmap;
		twin_rect_t	clip = twin_pixmap_current_clip (pixmap);

		twin_pixmap_set_clip (pixmap, child->extents);
		child->paint = TWIN_FALSE;
		(*child->dispatch) (child, event);
		twin_pixmap_restore_clip (pixmap, clip);
	    }
	break;
    default:
	break;
    }
    return TwinDispatchNone;
}

twin_box_t *
twin_box_create (twin_box_t	*parent,
		 twin_layout_t	layout)
{
    twin_box_t	*box = malloc (sizeof (twin_box_t));
    if (!box)
	return 0;
    _twin_box_init (box, parent, 0, layout, _twin_box_dispatch);
    return box;
}

