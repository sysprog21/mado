/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

void
_twin_box_init (twin_box_t		*box,
		twin_box_t		*parent,
		twin_window_t		*window,
		twin_box_dir_t		dir,
		twin_dispatch_proc_t	dispatch)
{
    static twin_widget_layout_t	preferred = { 0, 0, 0, 0 };
    _twin_widget_init (&box->widget, parent, window, preferred, dispatch);
    box->dir = dir;
    box->children = NULL;
    box->button_down = NULL;
    box->focus = NULL;
}

static twin_dispatch_result_t
_twin_box_query_geometry (twin_box_t *box)
{
    twin_widget_t	    *child;
    twin_event_t	    ev;
    twin_widget_layout_t    preferred;

    preferred.width = 0;
    preferred.height = 0;
    if (box->dir == TwinBoxHorz)
    {
	preferred.stretch_width = 0;
	preferred.stretch_height = 10000;
    }
    else
    {
	preferred.stretch_width = 10000;
	preferred.stretch_height = 0;
    }
    /*
     * Find preferred geometry
     */
    for (child = box->children; child; child = child->next)
    {
	if (child->layout)
	{
	    ev.kind = TwinEventQueryGeometry;
	    (*child->dispatch) (child, &ev);
	}
	if (box->dir == TwinBoxHorz)
	{
	    preferred.width += child->preferred.width;
	    preferred.stretch_width += child->preferred.stretch_width;
	    if (child->preferred.height > preferred.height)
		preferred.height = child->preferred.height;
	    if (child->preferred.stretch_height < preferred.stretch_height)
		preferred.stretch_height = child->preferred.stretch_height;
	}
	else
	{
	    preferred.height += child->preferred.height;
	    preferred.stretch_height += child->preferred.stretch_height;
	    if (child->preferred.width > preferred.width)
		preferred.width = child->preferred.width;
	    if (child->preferred.stretch_width < preferred.stretch_width)
		preferred.stretch_width = child->preferred.stretch_width;
	}
    }
    box->widget.preferred = preferred;
    return TwinDispatchContinue;
}

static twin_dispatch_result_t
_twin_box_configure (twin_box_t *box)
{
    twin_coord_t    width = _twin_widget_width (box);
    twin_coord_t    height = _twin_widget_height (box);
    twin_coord_t    actual;
    twin_coord_t    pref;
    twin_coord_t    delta;
    twin_coord_t    delta_remain;
    twin_coord_t    stretch = 0;
    twin_coord_t    pos = 0;
    twin_widget_t   *child;
    
    if (box->dir == TwinBoxHorz)
    {
	stretch = box->widget.preferred.stretch_width;
	actual = width;
	pref = box->widget.preferred.width;
    }
    else
    {
	stretch = box->widget.preferred.stretch_height;
	actual = height;
	pref = box->widget.preferred.height;
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
	    if (box->dir == TwinBoxHorz)
		stretch_this = child->preferred.stretch_width;
	    else
		stretch_this = child->preferred.stretch_height;
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
	if (box->dir == TwinBoxHorz)
	{
	    twin_coord_t    child_w = child->preferred.width;
	    extents.top = 0;
	    extents.bottom = height;
	    extents.left = pos;
	    pos = extents.right = pos + child_w + delta_this;
	}
	else
	{
	    twin_coord_t    child_h = child->preferred.height;
	    extents.left = 0;
	    extents.right = width;
	    extents.top = pos;
	    pos = extents.bottom = pos + child_h + delta_this;
	}
	if (extents.left != ev.u.configure.extents.left ||
	    extents.top != ev.u.configure.extents.top ||
	    extents.right != ev.u.configure.extents.right ||
	    extents.bottom != ev.u.configure.extents.bottom)
	{
	    ev.kind = TwinEventConfigure;
	    ev.u.configure.extents = extents;
	    (*child->dispatch) (child, &ev);
	}
    }
    return TwinDispatchContinue;
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

    if (event->kind != TwinEventPaint &&
	_twin_widget_dispatch (widget, event) == TwinDispatchDone)
	return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventQueryGeometry:
	return _twin_box_query_geometry (box);
    case TwinEventConfigure:
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
		twin_rect_t	clip = twin_pixmap_save_clip (pixmap);
		twin_coord_t	ox, oy;

		twin_pixmap_get_origin (pixmap, &ox, &oy);
		if (child->shape != TwinShapeRectangle)
		    twin_fill (child->window->pixmap,
			       widget->background, TWIN_SOURCE,
			       child->extents.left, child->extents.top,
			       child->extents.right, child->extents.bottom);
		twin_pixmap_set_clip (pixmap, child->extents);
		twin_pixmap_origin_to_clip (pixmap);
		child->paint = TWIN_FALSE;
		(*child->dispatch) (child, event);
		twin_pixmap_restore_clip (pixmap, clip);
		twin_pixmap_set_origin (pixmap, ox, oy);
	    }
	break;
    default:
	break;
    }
    return TwinDispatchContinue;
}

twin_box_t *
twin_box_create (twin_box_t	*parent,
		 twin_box_dir_t	dir)
{
    twin_box_t	*box = malloc (sizeof (twin_box_t));
    if (!box)
	return 0;
    _twin_box_init (box, parent, 0, dir, _twin_box_dispatch);
    return box;
}

