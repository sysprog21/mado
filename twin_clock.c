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

#include <twin_clock.h>
#include <twinint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

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

#define _twin_clock_pixmap(clock)   ((clock)->widget.window->pixmap)

static void
twin_clock_set_transform (twin_clock_t	*clock,
			  twin_path_t	*path)
{
    twin_fixed_t    scale;

    scale = (TWIN_FIXED_ONE - TWIN_CLOCK_BORDER_WIDTH * 3) / 2;
    twin_path_scale (path, _twin_widget_width (clock) * scale,
		     _twin_widget_height (clock) * scale);

    twin_path_translate (path, 
			 TWIN_FIXED_ONE + TWIN_CLOCK_BORDER_WIDTH * 3,
			 TWIN_FIXED_ONE + TWIN_CLOCK_BORDER_WIDTH * 3);
    
    twin_path_rotate (path, -TWIN_ANGLE_90);
}

static void
twin_clock_hand (twin_clock_t	*clock, 
		 twin_angle_t	angle, 
		 twin_fixed_t	len,
		 twin_fixed_t	fill_width,
		 twin_fixed_t	out_width,
		 twin_argb32_t	fill_pixel,
		 twin_argb32_t	out_pixel)
{
    twin_path_t	    *stroke = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_path_t	    *path = twin_path_create ();
    twin_matrix_t   m;

    twin_clock_set_transform (clock, stroke);

    twin_path_rotate (stroke, angle);
    twin_path_move (stroke, D(0), D(0));
    twin_path_draw (stroke, len, D(0));

    m = twin_path_current_matrix (stroke);
    m.m[2][0] = 0;
    m.m[2][1] = 0;
    twin_path_set_matrix (pen, m);
    twin_path_set_matrix (path, m);
    twin_path_circle (pen, 0, 0, fill_width);
    twin_path_convolve (path, stroke, pen);

    twin_paint_path (_twin_clock_pixmap(clock), fill_pixel, path);

    twin_paint_stroke (_twin_clock_pixmap(clock), out_pixel, path, out_width);
    
    twin_path_destroy (path);
    twin_path_destroy (pen);
    twin_path_destroy (stroke);
}

static twin_angle_t
twin_clock_minute_angle (int min)
{
    return min * TWIN_ANGLE_360 / 60;
}

static void
_twin_clock_face (twin_clock_t *clock)
{
    twin_path_t	    *path = twin_path_create ();
    int		    m;

    twin_clock_set_transform (clock, path);

    twin_path_circle (path, 0, 0, TWIN_FIXED_ONE);
    
    twin_paint_path (_twin_clock_pixmap(clock), TWIN_CLOCK_BACKGROUND, path);

    twin_paint_stroke (_twin_clock_pixmap(clock), TWIN_CLOCK_BORDER, path, TWIN_CLOCK_BORDER_WIDTH);

    {
	twin_state_t	    state = twin_path_save (path);
	twin_text_metrics_t metrics;
	twin_fixed_t	    height, width;
	static char	    *label = "twin";

	twin_path_empty (path);
	twin_path_rotate (path, twin_degrees_to_angle (-11) + TWIN_ANGLE_90);
	twin_path_set_font_size (path, D(0.5));
	twin_path_set_font_style (path, TWIN_TEXT_UNHINTED|TWIN_TEXT_OBLIQUE);
	twin_text_metrics_utf8 (path, label, &metrics);
	height = metrics.ascent + metrics.descent;
	width = metrics.right_side_bearing - metrics.left_side_bearing;
	
	twin_path_move (path, -width / 2, metrics.ascent - height/2 + D(0.01));
	twin_path_draw (path, width / 2, metrics.ascent - height/2 + D(0.01));
	twin_paint_stroke (_twin_clock_pixmap(clock), TWIN_CLOCK_WATER_UNDER, path, D(0.02));
	twin_path_empty (path);
	
	twin_path_move (path, -width / 2 - metrics.left_side_bearing, metrics.ascent - height/2);
	twin_path_utf8 (path, label);
	twin_paint_path (_twin_clock_pixmap(clock), TWIN_CLOCK_WATER, path);
	twin_path_restore (path, &state);
    }

    twin_path_set_font_size (path, D(0.2));
    twin_path_set_font_style (path, TWIN_TEXT_UNHINTED);

    for (m = 1; m <= 60; m++)
    {
	twin_state_t	state = twin_path_save (path);
	twin_path_rotate (path, twin_clock_minute_angle (m) + TWIN_ANGLE_90);
        twin_path_empty (path);
	if (m % 5 != 0)
	{
	    twin_path_move (path, 0, -TWIN_FIXED_ONE);
	    twin_path_draw (path, 0, -D(0.9));
	    twin_paint_stroke (_twin_clock_pixmap(clock), TWIN_CLOCK_TIC, path, D(0.01));
	}
	else
	{
	    char		hour[3];
	    twin_text_metrics_t	metrics;
	    twin_fixed_t	width;
	    twin_fixed_t	left;
	    
	    sprintf (hour, "%d", m / 5);
	    twin_text_metrics_utf8 (path, hour, &metrics);
	    width = metrics.right_side_bearing - metrics.left_side_bearing;
	    left = -width / 2 - metrics.left_side_bearing;
	    twin_path_move (path, left, -D(0.98) + metrics.ascent);
	    twin_path_utf8 (path, hour);
	    twin_paint_path (_twin_clock_pixmap(clock), TWIN_CLOCK_NUMBERS, path);
	}
        twin_path_restore (path, &state);
    }
    
    twin_path_destroy (path);
}

static twin_time_t
_twin_clock_interval (void)
{
    struct timeval  tv;
    gettimeofday (&tv, NULL);

    return 1000 - (tv.tv_usec / 1000);
}

void
_twin_clock_paint (twin_clock_t *clock)
{
    struct timeval  tv;
    twin_angle_t    second_angle, minute_angle, hour_angle;
    struct tm	    t;
    
    gettimeofday (&tv, NULL);

    localtime_r(&tv.tv_sec, &t);

    _twin_clock_face (clock);

    second_angle = ((t.tm_sec * 100 + tv.tv_usec / 10000) * 
		    TWIN_ANGLE_360) / 6000;
    minute_angle = twin_clock_minute_angle (t.tm_min) + second_angle / 60;
    hour_angle = (t.tm_hour * TWIN_ANGLE_360 + minute_angle) / 12;
    twin_clock_hand (clock, hour_angle, D(0.4), D(0.07), D(0.01),
		     TWIN_CLOCK_HOUR, TWIN_CLOCK_HOUR_OUT);
    twin_clock_hand (clock, minute_angle, D(0.8), D(0.05), D(0.01),
		     TWIN_CLOCK_MINUTE, TWIN_CLOCK_MINUTE_OUT);
    twin_clock_hand (clock, second_angle, D(0.9), D(0.01), D(0.01),
		     TWIN_CLOCK_SECOND, TWIN_CLOCK_SECOND_OUT);
}

static twin_time_t
_twin_clock_timeout (twin_time_t now, void *closure)
{
    twin_clock_t   *clock = closure;
    _twin_widget_queue_paint (&clock->widget);
    return _twin_clock_interval ();
}

twin_dispatch_result_t
_twin_clock_dispatch (twin_widget_t *widget, twin_event_t *event)
{
    twin_clock_t    *clock = (twin_clock_t *) widget;

    if (_twin_widget_dispatch (widget, event) == TwinDispatchDone)
	return TwinDispatchDone;
    switch (event->kind) {
    case TwinEventPaint:
	_twin_clock_paint (clock);
	break;
    default:
	break;
    }
    return TwinDispatchContinue;
}

void
_twin_clock_init (twin_clock_t		*clock, 
		  twin_box_t		*parent,
		  twin_dispatch_proc_t	dispatch)
{
    static const twin_widget_layout_t	preferred = { 0, 0, 1, 1 };
    _twin_widget_init (&clock->widget, parent, 0, preferred, dispatch);
    clock->timeout = twin_set_timeout (_twin_clock_timeout,
				       _twin_clock_interval(),
				       clock);
}
    
twin_clock_t *
twin_clock_create (twin_box_t *parent)
{
    twin_clock_t    *clock = malloc (sizeof (twin_clock_t));
    
    _twin_clock_init(clock, parent, _twin_clock_dispatch);
    return clock;
}

void
twin_clock_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h)
{
    twin_toplevel_t *toplevel = twin_toplevel_create (screen, TWIN_ARGB32,
						      TwinWindowApplication,
						      x, y, w, h, name);
    twin_clock_t    *clock = twin_clock_create (&toplevel->box);
    (void) clock;
    twin_toplevel_show (toplevel);
}
