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

#include <twin_demo.h>

#define D(x) twin_double_to_fixed(x)

#if 0
static int styles[] = {
    TWIN_TEXT_ROMAN,
    TWIN_TEXT_OBLIQUE,
    TWIN_TEXT_BOLD,
    TWIN_TEXT_BOLD|TWIN_TEXT_OBLIQUE
};
#endif

#if 0
static void
twin_example_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, w, h);
    twin_operand_t  source, mask;
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);
    
    twin_window_set_name (window, "example");
    
    twin_path_circle (pen, D (1));
    
    /* graphics here in path */

    twin_fill_path (alpha, path, 0, 0);
    twin_path_destroy (path);
    twin_path_destroy (pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (pixmap, 0, 0,
		    &source, 0, 0, &mask, 0, 0, TWIN_OVER, w, h);
    twin_pixmap_destroy (alpha);
    twin_window_show (window);
}
#endif

static void
twin_line_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_path_t	    *stroke = twin_path_create ();
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, w, h);
    twin_operand_t  source, mask;
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 
	       0, 0, wid, hei);

    twin_window_set_name (window, "line");
    
    twin_path_circle (pen, D (1));
    
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

    twin_fill_path (alpha, path, 0, 0);
    twin_path_destroy (path);
    twin_path_destroy (pen);
    twin_path_destroy (stroke);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (pixmap, 0, 0,
		    &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid, hei);
    twin_pixmap_destroy (alpha);
    twin_window_show (window);
}

static void
twin_circletext_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, wid, hei);
    int		    s;
    twin_operand_t  source, mask;
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 
	       0, 0, wid, hei);
    twin_window_set_name (window, "circletext");
    
    twin_path_set_font_style (path, TWIN_TEXT_UNHINTED);
    twin_path_circle (pen, D (1));
    
    twin_path_translate (path, D(200), D(200));
    twin_path_set_font_size (path, D(15));
    for (s = 0; s < 41; s++)
    {
	twin_state_t	state = twin_path_save (path);
	twin_path_rotate (path, twin_degrees_to_angle (9 * s));
	twin_path_move (path, D(100), D(0));
	twin_path_utf8 (path, "Hello, world!");
	twin_path_restore (path, &state);
    }
    twin_fill_path (alpha, path, 0, 0);
    twin_path_destroy (path);
    twin_path_destroy (pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (pixmap, 0, 0,
		    &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid, hei);
    twin_pixmap_destroy (alpha);
    twin_window_show (window);
}

static void
twin_quickbrown_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, wid, hei);
    twin_operand_t  source, mask;
    twin_fixed_t    fx, fy;
    int		    s;
    
    twin_window_set_name (window, "Quick Brown");
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 
	       0, 0, wid, hei);
    
    twin_path_circle (pen, D (1));
    
    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s++)
    {
	twin_path_move (path, fx, fy);
	twin_path_set_font_size (path, D(s));
	twin_path_utf8 (path,
			"the quick brown fox jumps over the lazy dog.");
	twin_path_utf8 (path,
			"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
	fy += D(s);
    }

    twin_fill_path (alpha, path, 0, 0);
    twin_path_destroy (path);
    twin_path_destroy (pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (pixmap, 0, 0,
		    &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid, hei);
    twin_pixmap_destroy (alpha);
    twin_window_show (window);
}

static void
twin_ascii_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_path_t	    *pen = twin_path_create ();
    twin_pixmap_t   *alpha = twin_pixmap_create (TWIN_A8, wid, hei);
    twin_operand_t  source, mask;
    twin_fixed_t    fx, fy;
    int		    s;
    
    twin_window_set_name (window, "ASCII");
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 0, 0, wid, hei);
    twin_path_circle (pen, D (1));
    
    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s += 6)
    {
	twin_path_set_font_size (path, D(s));
        fy += D(s+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  " !\"#$%&'()*+,-./0123456789:;<=>?");
        fy += D(s+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
        fy += D(s+2);
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path,
			  "`abcdefghijklmnopqrstuvwxyz{|}~");
	fy += D(s+2);
    }

    twin_fill_path (alpha, path, 0, 0);
    twin_path_destroy (path);
    twin_path_destroy (pen);
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff000000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (pixmap, 0, 0, 
		    &source, 0, 0, &mask, 0, 0, TWIN_OVER, wid, hei);
    twin_pixmap_destroy (alpha);
    twin_window_show (window);
}

static void
twin_jelly_start (twin_screen_t *screen, int x, int y, int w, int h)
{
    twin_window_t   *window = twin_window_create (screen, TWIN_ARGB32,
						  TwinWindowApplication,
						  x, y, w, h);
    int		    wid = window->client.right - window->client.left;
    int		    hei = window->client.bottom - window->client.top;
    twin_pixmap_t   *pixmap = window->pixmap;
    twin_path_t	    *path = twin_path_create ();
    twin_fixed_t    fx, fy;
    int		    s;
    
    twin_window_set_name (window, "Jelly");
    
    twin_fill (pixmap, 0xffffffff, TWIN_SOURCE, 
	       0, 0, wid, hei);
    
    fx = D(3);
    fy = D(8);
    for (s = 6; s < 36; s += 2)
    {
	twin_path_set_font_size (path, D(s));
	fy += D(s + 2);
	twin_path_move (path, fx, fy);
#define TEXT	"jelly text"
/*	twin_path_set_font_style (path, TWIN_TEXT_UNHINTED); */
	twin_path_utf8 (path, TEXT);
	twin_paint_path (pixmap, 0xff000000, path);
	twin_path_empty (path);
	{
	    twin_text_metrics_t	m;
	    twin_path_t	*stroke = twin_path_create ();
	    twin_path_set_matrix (stroke, twin_path_current_matrix (path));
	    twin_text_metrics_utf8 (path, TEXT, &m);
	    twin_path_translate (stroke, TWIN_FIXED_HALF, TWIN_FIXED_HALF);
	    twin_path_move (stroke, fx, fy);
	    twin_path_draw (stroke, fx + m.width, fy);
	    twin_paint_stroke (pixmap, 0xffff0000, stroke, D(1));
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
	    twin_paint_stroke (pixmap, 0xff00ff00, stroke, D(1));
	    twin_path_destroy (stroke);
	}
    }
    twin_window_show (window);
}

void
twin_demo_start (twin_screen_t *screen, const char *name, int x, int y, int w, int h)
{
    twin_circletext_start (screen, x, y, w, h);
    twin_line_start (screen, x += 20, y += 20, w, h);
    twin_quickbrown_start (screen, x += 20, y += 20, w, h);
    twin_ascii_start (screen, x += 20, y += 20, w, h);
    twin_jelly_start (screen, x += 20, y += 20, w, h);
#if 0

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
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    w, h);
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
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    w, h);
#endif

#if 0
    path = twin_path_create ();
    stroke = twin_path_create ();

    twin_path_translate (stroke, D(100), D(100));
    twin_path_scale (stroke, D(10), D(10));
    twin_path_move (stroke, D(0), D(0));
    twin_path_draw (stroke, D(10), D(0));
    twin_path_convolve (path, stroke, pen);
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, path);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (red, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    w, h);
#endif
    
#if 1
    path = twin_path_create ();

    stroke = twin_path_create ();
    
    twin_path_move (stroke, D (10), D (40));
    twin_path_draw (stroke, D (40), D (40));
    twin_path_draw (stroke, D (10), D (10));
    twin_path_move (stroke, D (10), D (50));
    twin_path_draw (stroke, D (40), D (50));

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (stroke);

    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, path, 0, 0);
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

    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, stroke, 0, 0);
    
    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xffff0000;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
    
    twin_path_convolve (path, stroke, pen);
    
    twin_fill (alpha, 0x00000000, TWIN_SOURCE, 0, 0, w, h);
    twin_fill_path (alpha, path, 0, 0);

    source.source_kind = TWIN_SOLID;
    source.u.argb = 0xff0000ff;
    mask.source_kind = TWIN_PIXMAP;
    mask.u.pixmap = alpha;
    twin_composite (blue, 0, 0, &source, 0, 0, &mask, 0, 0, TWIN_OVER,
		    100, 100);
#endif

    twin_window_show (redw);
    twin_window_show (bluew);
#endif
}
