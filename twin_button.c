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

#define _twin_button_bw(button)	((button)->label.font_size / 5)

static void
_twin_button_paint (twin_button_t *button)
{
    twin_path_t		*path = twin_path_create ();
    twin_coord_t	w = button->label.widget.extents.right - button->label.widget.extents.left;
    twin_coord_t	h = button->label.widget.extents.bottom - button->label.widget.extents.top;
    twin_fixed_t	wf = twin_int_to_fixed (w);
    twin_fixed_t	hf = twin_int_to_fixed (h);
    twin_fixed_t	bf = _twin_button_bw (button);
    twin_fixed_t	bh = bf / 2;
    twin_argb32_t	top_color, bot_color;
    twin_pixmap_t	*pixmap = button->label.widget.window->pixmap;
    
    if (path)
    {
	if (button->active)
	{
	    top_color = 0x80000000;
	    bot_color = 0x80808080;
	}
	else
	{
	    top_color = 0x80808080;
	    bot_color = 0x80000000;
	}
	twin_path_move (path, bh, hf-bh);
	twin_path_draw (path, bh, bh);
	twin_path_draw (path, wf-bh, bh);
	twin_paint_stroke (pixmap, top_color, path, bf);
	twin_path_empty (path);
	twin_path_move (path, bh, hf-bh);
	twin_path_draw (path, wf-bh, hf-bh);
	twin_path_draw (path, wf-bh, bh);
	twin_paint_stroke (pixmap, bot_color, path, bf);
	twin_path_destroy (path);
    }
}

static void
_twin_button_set_label_offset (twin_button_t *button)
{
    twin_fixed_t    bf = _twin_button_bw (button);
    twin_fixed_t    bh = bf / 2;

    if (button->active)
	button->label.offset.y = button->label.offset.x = 0;
    else
	button->label.offset.y = button->label.offset.x = -bh;
    _twin_widget_queue_paint (&button->label.widget);
}

twin_dispatch_result_t
_twin_button_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_button_t    *button = (twin_button_t *) widget;

    _twin_label_dispatch (widget, event);
    switch (event->kind) {
    case TwinEventPaint:
	_twin_button_paint (button);
	return TWIN_TRUE;
    case TwinEventButtonDown:
	button->pressed = TWIN_TRUE;
	button->active = TWIN_TRUE;
	_twin_button_set_label_offset (button);
	return TWIN_TRUE;
    case TwinEventMotion:
	if (button->pressed)
	{
	    twin_bool_t	active = _twin_widget_contains (&button->label.widget,
							event->u.pointer.x,
							event->u.pointer.y);
	    if (active != button->active)
	    {
		button->active = active;
		_twin_button_set_label_offset (button);
	    }
	}
	return TWIN_TRUE;
    case TwinEventButtonUp:
	button->pressed = TWIN_FALSE;
	if (button->active)
	{
	    if (button->callback)
		(*button->callback) (&button->label.widget, button->closure);
	    button->active = TWIN_FALSE;
	    _twin_button_set_label_offset (button);
	}
	return TWIN_TRUE;
    default:
	break;
    }
    return TwinDispatchNone;
}

void
_twin_button_init (twin_button_t	*button,
		   twin_box_t		*parent,
		   const char		*value,
		   twin_argb32_t	foreground,
		   twin_fixed_t		font_size,
		   twin_style_t		font_style,
		   twin_dispatch_proc_t	dispatch)
{
    _twin_label_init (&button->label, parent, value,
		      foreground, font_size, font_style, dispatch);
    button->pressed = TWIN_FALSE;
    _twin_button_set_label_offset (button);
}


twin_button_t *
twin_button_create (twin_box_t	    *parent,
		   const char	    *value,
		   twin_argb32_t    foreground,
		   twin_fixed_t	    font_size,
		   twin_style_t	    font_style)
{
    twin_button_t    *button = malloc (sizeof (twin_button_t));

    if (!button)
	return 0;
    _twin_button_init (button, parent, value, foreground, 
		       font_size, font_style, _twin_button_dispatch);
    return button;
}

