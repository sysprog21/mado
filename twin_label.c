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
_twin_label_query_geometry (twin_label_t *label)
{
    twin_path_t		*path = twin_path_create ();
    twin_text_metrics_t	m;
    
    label->widget.preferred.left = 0;
    label->widget.preferred.top = 0;
    label->widget.preferred.right = twin_fixed_to_int (label->font_size);
    label->widget.preferred.bottom = twin_fixed_to_int (label->font_size) * 2;
    if (path)
    {
	twin_path_set_font_size (path, label->font_size);
	twin_path_set_font_style (path, label->font_style);
	twin_text_metrics_utf8 (path, label->label, &m);
	label->widget.preferred.right += twin_fixed_to_int (m.width);
	twin_path_destroy (path);
    }
}

static void
_twin_label_paint (twin_label_t *label)
{
    twin_path_t		*path = twin_path_create ();
    twin_text_metrics_t	m;
    twin_coord_t	w = label->widget.extents.right - label->widget.extents.left;
    twin_coord_t	h = label->widget.extents.bottom - label->widget.extents.top;

    if (path)
    {
	twin_fixed_t	wf = twin_int_to_fixed (w);
	twin_fixed_t	hf = twin_int_to_fixed (h);

	twin_path_set_font_size (path, label->font_size);
	twin_path_set_font_style (path, label->font_style);
	twin_text_metrics_utf8 (path, label->label, &m);
	twin_path_move (path, (wf - m.width) / 2,
			(hf - (m.font_ascent + m.font_descent)) / 2 + m.font_ascent);

	twin_path_utf8 (path, label->label);
	twin_paint_path (label->widget.window->pixmap, label->foreground, path);
	twin_path_destroy (path);
    }
}

twin_dispatch_result_t
_twin_label_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_label_t    *label = (twin_label_t *) widget;

    _twin_widget_dispatch (widget, event);
    switch (event->kind) {
    case TwinEventPaint:
	_twin_label_paint (label);
	return TwinDispatchNone;
    case TwinEventQueryGeometry:
	_twin_label_query_geometry (label);
	break;
    default:
	break;
    }
    return TwinDispatchNone;
}

void
twin_label_set (twin_label_t	*label,
		const char	*value,
		twin_argb32_t	foreground,
		twin_fixed_t	font_size,
		twin_style_t	font_style)
{
    if (value)
    {
	char    *new = malloc (strlen (value) + 1);

	if (new)
	{
	    if (label->label)
		free (label->label);
	    label->label = new;
	    strcpy (label->label, value);
	}
    }
    label->font_size = font_size;
    label->font_style = font_style;
    label->foreground = foreground;
    _twin_widget_queue_layout (&label->widget);
}

void
_twin_label_init (twin_label_t	*label,
		  twin_box_t	*parent,
		  const char	*value,
		  twin_argb32_t	foreground,
		  twin_fixed_t	font_size,
		  twin_style_t	font_style)
{
    static const twin_rect_t	empty = { 0, 0, 0, 0 };
    _twin_widget_init (&label->widget, parent, 0, 
		       empty, 1, 1, _twin_label_dispatch);
    label->label = NULL;
    twin_label_set (label, value, foreground, font_size, font_style);
}


twin_label_t *
twin_label_create (twin_box_t	    *parent,
		   const char	    *value,
		   twin_argb32_t    foreground,
		   twin_fixed_t	    font_size,
		   twin_style_t	    font_style)
{
    twin_label_t    *label = malloc (sizeof (twin_label_t));

    if (!label)
	return 0;
    _twin_label_init (label, parent, value, foreground, font_size, font_style);
    return label;
}

