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

#include <twin_demoline.h>
#include <twinint.h>
#include <stdio.h>

#define D(x) twin_double_to_fixed(x)

#define TWIN_CLOCK_BACKGROUND	0xff3b80ae
#define TWIN_CLOCK_HOUR		0x80808080
#define TWIN_CLOCK_HOUR_OUT	0x30000000
#define TWIN_CLOCK_MINUTE	0x80808080
#define TWIN_CLOCK_MINUTE_OUT	0x30000000
#define TWIN_CLOCK_SECOND	0x80808080
#define TWIN_CLOCK_SECOND_OUT	0x30000000
#define TWIN_CLOCK_TIC		0xffbababa
#define TWIN_CLOCK_NUMBERS	0xffdedede
#define TWIN_CLOCK_WATER	0x60200000
#define TWIN_CLOCK_WATER_OUT	0x40404040
#define TWIN_CLOCK_WATER_UNDER	0x60400000
#define TWIN_CLOCK_BORDER	0xffbababa
#define TWIN_CLOCK_BORDER_WIDTH	D(0.01)

#define _twin_demoline_pixmap(demoline)   ((demoline)->widget.window->pixmap)

void
_twin_demoline_paint (twin_demoline_t *demoline)
{
    twin_path_t	*path;

    path = twin_path_create ();
    twin_path_set_cap_style (path, demoline->cap_style);
    twin_path_move (path, demoline->points[0].x, demoline->points[0].y);
    twin_path_draw (path, demoline->points[1].x, demoline->points[1].y);
    twin_paint_stroke (_twin_demoline_pixmap(demoline), 0xff000000, path,
		       demoline->line_width);
    twin_path_set_cap_style (path, TwinCapButt);
    twin_paint_stroke (_twin_demoline_pixmap(demoline), 0xffff0000, path,
		       twin_int_to_fixed (2));
    twin_path_destroy (path);
}

static twin_dispatch_result_t
_twin_demoline_update_pos (twin_demoline_t *demoline, twin_event_t *event)
{
    if (demoline->which < 0)
	return TwinDispatchContinue;
    demoline->points[demoline->which].x = twin_int_to_fixed (event->u.pointer.x);
    demoline->points[demoline->which].y = twin_int_to_fixed (event->u.pointer.y);
    _twin_widget_queue_paint (&demoline->widget);
    return TwinDispatchDone;
}

#define twin_fixed_abs(f)   ((f) < 0 ? -(f) : (f))

static int
_twin_demoline_hit (twin_demoline_t *demoline, twin_fixed_t x, twin_fixed_t y)
{
    int	i;

    for (i = 0; i < 2; i++)
	if (twin_fixed_abs (x - demoline->points[i].x) < demoline->line_width / 2 &&
	    twin_fixed_abs (y - demoline->points[i].y) < demoline->line_width / 2)
	    return i;
    return -1;
}

twin_dispatch_result_t
_twin_demoline_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_demoline_t    *demoline = (twin_demoline_t *) widget;

    if (_twin_widget_dispatch (widget, event) == TwinDispatchDone)
	return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
	_twin_demoline_paint (demoline);
	break;
    case TwinEventButtonDown:
	demoline->which = _twin_demoline_hit (demoline,
					      twin_int_to_fixed (event->u.pointer.x),
					      twin_int_to_fixed (event->u.pointer.y));
	return _twin_demoline_update_pos (demoline, event);
	break;
    case TwinEventMotion:
	return _twin_demoline_update_pos (demoline, event);
	break;
    case TwinEventButtonUp:
	if (demoline->which < 0)
	    return TwinDispatchContinue;
	_twin_demoline_update_pos (demoline, event);
	demoline->which = -1;
	return TwinDispatchDone;
	break;
    default:
	break;
    }
    return TwinDispatchContinue;
}

void
_twin_demoline_init (twin_demoline_t		*demoline, 
		  twin_box_t		*parent,
		  twin_dispatch_proc_t	dispatch)
{
    static const twin_widget_layout_t	preferred = { 0, 0, 1, 1 };
    _twin_widget_init (&demoline->widget, parent, 0, preferred, dispatch);
    twin_widget_set (&demoline->widget, 0xffffffff);
    demoline->line_width = twin_int_to_fixed (30);
    demoline->cap_style = TwinCapProjecting;
    demoline->points[0].x = twin_int_to_fixed (50);
    demoline->points[0].y = twin_int_to_fixed (50);
    demoline->points[1].x = twin_int_to_fixed (100);
    demoline->points[1].y = twin_int_to_fixed (100);
}
    
twin_demoline_t *
twin_demoline_create (twin_box_t *parent)
{
    twin_demoline_t    *demoline = malloc (sizeof (twin_demoline_t));
    
    _twin_demoline_init(demoline, parent, _twin_demoline_dispatch);
    return demoline;
}

void
twin_demoline_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create (screen, TWIN_ARGB32,
						      TwinWindowApplication,
						      x, y, w, h, name);
    twin_demoline_t    *demoline = twin_demoline_create (&toplevel->box);
    (void) demoline;
    twin_toplevel_show (toplevel);
}
