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

#include <X11/Xlib.h>
#include "twin.h"
#include <string.h>
#include <stdio.h>

#define D(x) twin_double_to_fixed(x)

int styles[] = {
    TWIN_TEXT_ROMAN,
    TWIN_TEXT_OBLIQUE,
    TWIN_TEXT_BOLD,
    TWIN_TEXT_BOLD|TWIN_TEXT_OBLIQUE
};

#define WIDTH	512
#define HEIGHT	512

int
main (int argc, char **argv)
{
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

    extra = 0;
    stroke = 0;
    s = 0;
    fx = 0;
    fy = 0;
    g = 0 ;
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
    twin_path_string (stroke, D(200), D(200), TWIN_TEXT_ROMAN, "jelly world.");
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
    
#if 1
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
#endif
#if 0
    fx = D(3);
    fy = 0;
    for (g = 8; g < 30; g += 4)
    {
        twin_path_set_font_size (path, D(g));
#if 0
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_string (path, D(g), D(g), TWIN_TEXT_ROMAN,
			  " !\"#$%&'()*+,-./0123456789:;<=>?");
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_string (path, D(g), D(g), TWIN_TEXT_ROMAN,
			  "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_");
        fy += D(g+2);
	twin_path_move (path, fx, fy);
	twin_path_string (path, D(g), D(g), TWIN_TEXT_ROMAN,
			  "`abcdefghijklmnopqrstuvwxyz{|}~");
#endif
#if 1
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
	twin_path_utf8 (path, "t");
#endif
    }
#endif
#if 0
    fx = D(3);
    fy = D(8);
    for (g = 6; g < 36; g++)
    {
	twin_path_move (path, fx, fy);
	twin_path_utf8 (path, D(g), D(g),
			"the quick brown fox jumps over the lazy dog.");
	twin_path_utf8 (path, D(g), D(g),
			"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.");
	fy += D(g);
    }
#endif
    twin_fill_path (alpha, path);
    
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
    had_motion = TWIN_FALSE;
    for (;;)
    {
	if (had_motion)
	{
	    x = motion.xmotion.x - 50;
	    y = motion.xmotion.y - 50;
	    if (motion.xmotion.state & Button1Mask)
		twin_pixmap_move (red, x, y);
	    if (motion.xmotion.state & Button3Mask)
		twin_pixmap_move (blue, x, y);
	    had_motion = TWIN_FALSE;
	}
	if (twin_screen_damaged (x11->screen))
	    twin_x11_update (x11);
	XSync (dpy, False);
	do {
	    XNextEvent (dpy, &ev);
	    switch (ev.type) {
	    case Expose:
		twin_x11_damage (x11, &ev.xexpose);
		break;
	    case ButtonPress:
		if (ev.xbutton.button == 2)
		{
		    if (red->higher == 0)
			twin_pixmap_show (red, x11->screen, 0);
		    else
			twin_pixmap_show (blue, x11->screen, 0);
		}
		break;
	    case MotionNotify:
		had_motion = TWIN_TRUE;
		motion = ev;
		break;
	    }
	} while (QLength (dpy));
    }
}

