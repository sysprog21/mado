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

#include "twin_x11.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

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
#define TWIN_CLOCK_WATER	0x40000000
#define TWIN_CLOCK_WATER_OUT	0x40404040
#define TWIN_CLOCK_WATER_UNDER	0xcccc0000
#define TWIN_CLOCK_BORDER	0xffbababa
#define TWIN_CLOCK_BORDER_WIDTH	D(0.01)

static void
twin_clock_set_transform (twin_pixmap_t	*clock,
			  twin_path_t	*path)
{
    twin_fixed_t    scale;

    scale = D(1) / 2;
    scale = twin_fixed_mul (scale, TWIN_FIXED_ONE - TWIN_CLOCK_BORDER_WIDTH * 3);
    twin_path_scale (path,
		     clock->width * scale,
		     clock->height * scale);

    twin_path_translate (path, D(1) + TWIN_CLOCK_BORDER_WIDTH * 3,
			 D(1) + TWIN_CLOCK_BORDER_WIDTH * 3);
}

static void
twin_clock_hand (twin_pixmap_t	*clock, 
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
    twin_path_circle (pen, fill_width);
    twin_path_convolve (path, stroke, pen);

    twin_paint_path (clock, fill_pixel, path);

    twin_paint_stroke (clock, out_pixel, path, out_width);
    
    twin_path_destroy (path);
    twin_path_destroy (pen);
    twin_path_destroy (stroke);
}

static twin_angle_t
twin_clock_minute_angle (int min)
{
    return min * TWIN_ANGLE_360 / 60 - TWIN_ANGLE_90;
}

static void
twin_clock_face (twin_pixmap_t *clock)
{
    twin_path_t	    *path = twin_path_create ();
    int		    m;

    twin_clock_set_transform (clock, path);

    twin_path_move (path, 0, 0);
    twin_path_circle (path, TWIN_FIXED_ONE);
    
    twin_paint_path (clock, TWIN_CLOCK_BACKGROUND, path);

    twin_paint_stroke (clock, TWIN_CLOCK_BORDER, path, TWIN_CLOCK_BORDER_WIDTH);

    {
	twin_state_t	    state = twin_path_save (path);
	twin_text_metrics_t metrics;
	twin_fixed_t	    height, width;
	static char	    *label = "twin";

	twin_path_empty (path);
	twin_path_rotate (path, twin_degrees_to_angle (-11));
	twin_path_set_font_size (path, D(0.5));
	twin_path_set_font_style (path, TWIN_TEXT_UNHINTED|TWIN_TEXT_OBLIQUE);
	twin_text_metrics_utf8 (path, label, &metrics);
	height = metrics.ascent + metrics.descent;
	width = metrics.right_side_bearing - metrics.left_side_bearing;
	
	twin_path_move (path, -width / 2, metrics.ascent - height/2);
	twin_path_draw (path, width / 2, metrics.ascent - height/2);
	twin_paint_stroke (clock, TWIN_CLOCK_WATER_UNDER, path, D(0.02));
	twin_path_empty (path);
	
	twin_path_move (path, -width / 2 - metrics.left_side_bearing, metrics.ascent - height/2);
	twin_path_utf8 (path, label);
	twin_paint_path (clock, TWIN_CLOCK_WATER, path);
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
	    twin_paint_stroke (clock, TWIN_CLOCK_TIC, path, D(0.01));
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
	    twin_paint_path (clock, TWIN_CLOCK_NUMBERS, path);
	}
        twin_path_restore (path, &state);
    }
    
    twin_path_destroy (path);
}

int nclock;

static void
twin_clock (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_pixmap_t   *clock = twin_pixmap_create (TWIN_ARGB32, w, h);
    struct timeval  tv;
    struct tm	    t;
    twin_angle_t    hour_angle, minute_angle, second_angle;

    twin_pixmap_move (clock, x, y);
    twin_pixmap_show (clock, screen, 0);
    
    for (;;)
    {
	twin_pixmap_disable_update (clock);
	twin_fill (clock, 0x00000000, TWIN_SOURCE, 0, 0, 
		   clock->width, clock->height);

	gettimeofday (&tv, NULL);

	localtime_r(&tv.tv_sec, &t);

	second_angle = ((t.tm_sec * 100 + tv.tv_usec / 10000) * 
			TWIN_ANGLE_360) / 6000 - TWIN_ANGLE_90;
	minute_angle = twin_clock_minute_angle (t.tm_min) + second_angle / 60;
	hour_angle = (t.tm_hour * TWIN_ANGLE_360 / 12 +
		      (minute_angle + TWIN_ANGLE_90) / 12 -
		      TWIN_ANGLE_90);
	twin_clock_face (clock);
	twin_clock_hand (clock, hour_angle, D(0.4), D(0.07), D(0.01),
			 TWIN_CLOCK_HOUR, TWIN_CLOCK_HOUR_OUT);
	twin_clock_hand (clock, minute_angle, D(0.8), D(0.05), D(0.01),
			 TWIN_CLOCK_MINUTE, TWIN_CLOCK_MINUTE_OUT);
	twin_clock_hand (clock, second_angle, D(0.9), D(0.01), D(0.01),
			 TWIN_CLOCK_SECOND, TWIN_CLOCK_SECOND_OUT);

	twin_pixmap_enable_update (clock);
	
	gettimeofday (&tv, NULL);
	
	usleep (1000000 - tv.tv_usec);
    }
    nclock--;
}

typedef void (*twin_app_func_t) (twin_screen_t *screen,
				 int x, int y, int w, int h);

typedef struct _twin_app_args {
    twin_app_func_t func;
    twin_screen_t   *screen;
    int		    x, y, w, h;
} twin_app_args_t;

static void *
twin_app_thread (void *closure)
{
    twin_app_args_t *a = closure;

    (*a->func) (a->screen, a->x, a->y, a->w, a->h);
    free (a);
    return 0;
}

static void
twin_start_app (twin_app_func_t func, 
		twin_screen_t *screen, 
		int x, int y, int w, int h)
{
    twin_app_args_t *a = malloc (sizeof (twin_app_args_t));
    pthread_t	    thread;
    
    a->func = func;
    a->screen = screen;
    a->x = x;
    a->y = y;
    a->w = w;
    a->h = h;
    pthread_create (&thread, NULL, twin_app_thread, a);
}

static void
twin_start_clock (twin_screen_t *screen, int x, int y, int w, int h)
{
    ++nclock;
    twin_start_app (twin_clock, screen, x, y, w, h);
}

int styles[] = {
    TWIN_TEXT_ROMAN,
    TWIN_TEXT_OBLIQUE,
    TWIN_TEXT_BOLD,
    TWIN_TEXT_BOLD|TWIN_TEXT_OBLIQUE
};

#define WIDTH	256
#define HEIGHT	256

int
main (int argc, char **argv)
{
    Status	    status = XInitThreads ();
    Display	    *dpy = XOpenDisplay (0);
    twin_x11_t	    *x11 = twin_x11_create (dpy, WIDTH, HEIGHT);
    twin_pixmap_t   *red = twin_pixmap_create (TWIN_ARGB32, WIDTH, HEIGHT);
    twin_pixmap_t   *blue = twin_pixmap_create (TWIN_ARGB32, 100, 100);
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, WIDTH, HEIGHT);
    twin_operand_t  source, mask;
    twin_path_t	    *path;
    XEvent	    ev, motion;
    twin_bool_t	    had_motion;
    int		    x, y;
    twin_path_t	    *pen;
    twin_path_t	    *stroke;
    twin_path_t	    *extra;
    int		    s;
    twin_fixed_t    fx, fy;
    int		    g;

    (void) ev;
    (void) motion;
    (void) had_motion;
    (void) x;
    (void) y;
    (void) red;
    (void) blue;
    (void) status;
    (void) alpha;
    (void) source;
    (void) mask;
    (void) source;
    (void) path;
    
    extra = 0;
    stroke = 0;
    pen = 0;
    s = 0;
    fx = 0;
    fy = 0;
    g = 0 ;

#if 0
    pen = twin_path_create ();
    twin_path_circle (pen, D (1));
    
    twin_fill (red, 0x00000000, TWIN_SOURCE, 0, 0, WIDTH, HEIGHT);
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, WIDTH, HEIGHT);

    path = twin_path_create ();
#if 0
    twin_path_move (path, D(3), D(0));
    for (g = 0; g < 4326; g++)
    {
	int glyph = g;
	if (!twin_has_glyph (glyph)) glyph = 0;
	twin_path_cur_point (path, &fx, &fy);
	if (fx + twin_glyph_width (glyph, D(10)) >= D(WIDTH) || g % 50 == 0)
	    twin_path_move (path, D(3), fy + D(10));
	twin_path_glyph (path, D(10), D(10), TWIN_TEXT_ROMAN,
			 glyph);
    }
#endif
#if 0
    stroke = twin_path_create ();
    twin_path_move (stroke, D(30), D(400));
    twin_path_set_font_size (stroke, D(100));
    twin_path_utf8 (stroke, "jelly world.");
    twin_path_convolve (path, stroke, pen);
/*    twin_path_append (path, stroke); */
    twin_path_destroy (stroke);
    stroke = twin_path_create ();
    twin_path_move (stroke, D(30), D(400));
    twin_path_draw (stroke, D(1000), D(400));
    twin_path_convolve (path, stroke, pen);
#endif
    
#if 0
    stroke = twin_path_create ();
    pen = twin_path_create ();
    twin_path_translate (stroke, D(100), D(100));
/*    twin_path_rotate (stroke, twin_degrees_to_angle (270)); */
    twin_path_rotate (stroke, twin_degrees_to_angle (270));
    twin_path_move (stroke, D(0), D(0));
    twin_path_draw (stroke, D(100), D(0));
    twin_path_set_matrix (pen, twin_path_current_matrix (stroke));
    twin_path_circle (pen, D(20));
    twin_path_convolve (path, stroke, pen);
#endif
    
#if 0
    stroke = twin_path_create ();
    twin_path_translate (stroke, D(250), D(250));
    twin_path_circle (stroke, 0x42aaa);
    extra = twin_path_convex_hull (stroke);
    twin_path_convolve (path, extra, pen);
#endif
    
#if 0
    {
	twin_state_t	state = twin_path_save (path);
    twin_path_translate (path, D(300), D(300));
    twin_path_set_font_size (path, D(15));
    for (s = 0; s < 41; s++)
    {
	twin_state_t	state = twin_path_save (path);
	twin_path_rotate (path, twin_degrees_to_angle (9 * s));
	twin_path_move (path, D(100), D(0));
	twin_path_utf8 (path, "Hello, world!");
	twin_path_restore (path, &state);
    }
	twin_path_restore (path, &state);
    }
#endif
#if 1
    fx = D(10);
    fy = 0;
    for (g = 10; g <= 38; g += 4)
    {
        twin_path_set_font_size (path, D(g));
#if 1
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  " !\"#$%&'()*+,-./0123456789:;<=>?");
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  "`abcdefghijklmnopqrstuvwxyz{|}~");
#endif
#if 0
	for (s = 0; s < 4; s++)
	{
	    fy += D(g+2);
	    twin_path_move (path, fx, fy);
	    twin_path_set_font_style (path, styles[s]);
	    twin_path_utf8 (path,
			    "the quick brown fox jumps over the lazy dog.");
	    twin_path_utf8 (path,
			    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
	}
#endif
#if 0
	fy += D(g);
	twin_path_move (path, fx, fy);
#define TEXT	"jelly world"
	twin_path_utf8 (path, TEXT);
	{
	    twin_text_metrics_t	m;
	    
	    stroke = twin_path_create ();
	    twin_path_set_matrix (stroke, twin_path_current_matrix (path));
	    twin_text_metrics_utf8 (path, TEXT, &m);
	    twin_path_move (stroke, fx, fy);
	    twin_path_draw (stroke, fx + m.width, fy);
	    twin_paint_stroke (red, 0xffff0000, stroke, D(1));
	    twin_path_empty (stroke);
	    twin_path_move (stroke, 
			    fx + m.left_side_bearing, fy - m.ascent);
	    twin_path_draw (stroke,
			    fx + m.right_side_bearing, fy - m.ascent);
	    twin_path_draw (stroke,
			    fx + m.right_side_bearing, fy + m.descent);
	    twin_path_draw (stroke,
			    fx + m.left_side_bearing, fy + m.descent);
	    twin_path_draw (stroke, 
			    fx + m.left_side_bearing, fy - m.ascent);
	    twin_paint_stroke (red, 0xff00ff00, stroke, D(1));
	}
#endif
    }
#endif
#if 0
    fx = D(3);
    fy = D(8);
    for (g = 6; g < 36; g++)
    {
	twin_path_move (path, fx, fy);
	twin_path_set_font_size (path, D(g));
	twin_path_utf8 (path,
			"the quick brown fox jumps over the lazy dog.");
	twin_path_utf8 (path,
			"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
	fy += D(g);
    }
#endif
    twin_fill_path (alpha, path, 0, 0);
    
    twin_path_destroy (path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    WIDTH, HEIGHT);

    twin_fill (blue, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);

#if 0
    path = twin_path_create ();

    twin_path_rotate (path, -TWIN_ANGLE_45);
    twin_path_translate (path, D(10), D(2));
    for (s = 0; s < 40; s++)
    {
	twin_path_rotate (path, TWIN_ANGLE_11_25 / 16);
	twin_path_scale (path, D(1.125), D(1.125));
	twin_path_move (path, D(0), D(0));
	twin_path_draw (path, D(1), D(0));
	twin_path_draw (path, D(1), D(1));
	twin_path_draw (path, D(0), D(1));
    }
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, WIDTH, HEIGHT);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    WIDTH, HEIGHT);
#endif

#if 0
    path = twin_path_create ();
    stroke = twin_path_create ();

    twin_path_translate (stroke, D(62), D(62));
    twin_path_scale (stroke,D(60),D(60));
    for (s = 0; s < 60; s++)
    {
        twin_state_t    save = twin_path_save (stroke);
	twin_angle_t    a = s * TWIN_ANGLE_90 / 15;
	    
	twin_path_rotate (stroke, a);
        twin_path_move (stroke, D(1), 0);
	if (s % 5 == 0)
	    twin_path_draw (stroke, D(0.85), 0);
	else
	    twin_path_draw (stroke, D(.98), 0);
        twin_path_restore (stroke, &save);
    }
    twin_path_convolve (path, stroke, pen);
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, WIDTH, HEIGHT);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    WIDTH, HEIGHT);
#endif

#if 0
    path = twin_path_create ();
    stroke = twin_path_create ();

    twin_path_translate (stroke, D(100), D(100));
    twin_path_scale (stroke, D(10), D(10));
    twin_path_move (stroke, D(0), D(0));
    twin_path_draw (stroke, D(10), D(0));
    twin_path_convolve (path, stroke, pen);
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, WIDTH, HEIGHT);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    WIDTH, HEIGHT);
#endif
    
#if 0
    path = twin_path_create ();

    stroke = twin_path_create ();
    
    twin_path_move (stroke, D (10), D (40));
    twin_path_draw (stroke, D (40), D (40));
    twin_path_draw (stroke, D (10), D (10));
    twin_path_move (stroke, D (10), D (50));
    twin_path_draw (stroke, D (40), D (50));

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (stroke);

    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill_path (alpha, path);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff00ff00;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
    
    twin_path_destroy (path);

    path = twin_path_create ();
    stroke = twin_path_create ();

    twin_path_move (stroke, D (50), D (50));
    twin_path_curve (stroke, D (70), D (70), D (80), D (70), D (100), D (50));

    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill_path (alpha, stroke);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
    
    twin_path_convolve (path, stroke, pen);
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, 100, 100);
    twin_fill_path (alpha, path);

    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff0000ff;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
#endif

    twin_pixmap_move (red, 0, 0);
    twin_pixmap_move (blue, 100, 100);
    twin_pixmap_show (red, x11->screen, 0);
    twin_pixmap_show (blue, x11->screen, 0);
    ++nclock;
#endif

#if 1
    twin_start_clock (x11->screen, 0, 0, WIDTH, HEIGHT);
#endif
#if 0
    twin_start_clock (x11->screen, 0, 0, 256, 256);
    twin_start_clock (x11->screen, 256, 0, 256, 256);
    twin_start_clock (x11->screen, 0, 256, 256, 256);
    twin_start_clock (x11->screen, 256, 256, 256, 256);
#endif
    while (nclock)
	sleep (1);
    return 0;
}

