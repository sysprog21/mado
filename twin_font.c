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

#if 0
#include <stdio.h>
#define F(x) twin_fixed_to_double(x)
#define S(x) twin_sfixed_to_double(x)
#define G(x) ((double) (x))
#define DBGMSG(x)	printf x
#else
#define DBGMSG(x)
#endif

#define Scale(f)	(((twin_fixed_t) (f) * path->state.font_size) >> 5)
#define ScaleX(x)	Scale(x)
#define ScaleY(y)	Scale(y)

#define Hint(p)		(((p)->state.font_style & TWIN_TEXT_UNHINTED) == 0)

twin_bool_t
twin_has_ucs4 (twin_ucs4_t ucs4)
{
    return ucs4 <= TWIN_FONT_MAX && _twin_glyph_offsets[ucs4] != 0;
}

static int
compare_snap (const void *av, const void *bv)
{
    const twin_gfixed_t   *a = av;
    const twin_gfixed_t   *b = bv;

    return (int) (*a - *b);
}

#define SNAPI(path,p)	(Hint(path) ? (((p) + 0x7fff) & ~0xffff) : (p))
#define SNAPH(path,p)	(Hint(path) ? (((p) + 0x3fff) & ~0x7fff) : (p))

static twin_fixed_t
_snap (twin_path_t *path, twin_gfixed_t g, twin_gfixed_t *snap, int nsnap)
{
    int		    s;
    twin_fixed_t    v;

    v = Scale(g);
    for (s = 0; s < nsnap - 1; s++)
    {
	if (snap[s] <= g && g <= snap[s+1])
	{
	    twin_fixed_t    before = Scale(snap[s]);
	    twin_fixed_t    after = Scale(snap[s+1]);
	    twin_fixed_t    dist = after - before;
	    twin_fixed_t    snap_before = SNAPI(path, before);
	    twin_fixed_t    snap_after = SNAPI(path, after);
	    twin_fixed_t    move_before = snap_before - before;
	    twin_fixed_t    move_after = snap_after - after;
	    twin_fixed_t    dist_before = v - before;
	    twin_fixed_t    dist_after = after - v;
	    twin_fixed_t    move = ((int64_t) dist_before * move_after + 
				    (int64_t) dist_after * move_before) / dist;
	    DBGMSG (("%d <= %d <= %d\n", snap[s], g, snap[s+1]));
	    DBGMSG (("%9.4f <= %9.4f <= %9.4f\n", F(before), F(v), F(after)));
	    DBGMSG (("before: %9.4f -> %9.4f\n", F(before), F(snap_before)));
	    DBGMSG (("after: %9.4f -> %9.4f\n", F(after), F(snap_after)));
	    DBGMSG (("v: %9.4f -> %9.4f\n", F(v), F(v+move)));
	    v += move;
	    break;
	}
    }
    DBGMSG (("_snap: %d => %9.4f\n", g, F(v)));
    return v;
}

#define SNAPX(p)	_snap (path, p, snap_x, nsnap_x)
#define SNAPY(p)	_snap (path, p, snap_y, nsnap_y)

static int
_add_snap (twin_gfixed_t *snaps, int nsnap, twin_fixed_t snap)
{
    int s;

    for (s = 0; s < nsnap; s++)
	if (snaps[s] == snap)
	    return nsnap;
    snaps[nsnap++] = snap;
    return nsnap;
}

static const twin_gpoint_t *
_twin_ucs4_base(twin_ucs4_t ucs4)
{
    if (ucs4 > TWIN_FONT_MAX) ucs4 = 0;

    return _twin_glyphs + _twin_glyph_offsets[ucs4];
}

#define TWIN_FONT_BASELINE  9

static twin_fixed_t
_twin_pen_size (twin_path_t *path)
{
    twin_fixed_t    pen_size;
    
    pen_size = SNAPH(path, path->state.font_size / 24);
    if (Hint (path) && pen_size < TWIN_FIXED_HALF)
	pen_size = TWIN_FIXED_HALF;
    
    if (path->state.font_style & TWIN_TEXT_BOLD)
    {
	twin_fixed_t	pen_add = SNAPH(path, pen_size >> 1);
	if (Hint (path) && pen_add == 0) 
	    pen_add = TWIN_FIXED_HALF;
	pen_size += pen_add;
    }
    return pen_size;
}

void
twin_text_metrics_ucs4 (twin_path_t	    *path, 
			twin_ucs4_t	    ucs4, 
			twin_text_metrics_t *m)
{
    const twin_gpoint_t	*p = _twin_ucs4_base (ucs4);
    twin_fixed_t	x, y;
    twin_fixed_t	left, right;
    twin_fixed_t	top, bottom;
    twin_fixed_t	pen_size = _twin_pen_size (path);
    twin_fixed_t	baseline = SNAPI(path, Scale(TWIN_FONT_BASELINE));
    int			i;
    int			skip_xi;
    int			skip_yi;
    int			next_xi;
    int			next_yi;
    
    left = TWIN_FIXED_MAX;
    top = TWIN_FIXED_MAX;
    right = TWIN_FIXED_MIN;
    bottom = TWIN_FIXED_MIN;
    /*
     * Locate horizontal and vertical segments
     */
    skip_xi = 0;
    skip_yi = 0;
    for (i = 1; p[i].y != -64; i++)
    {
	if (p[i].x == -64)
	    continue;
	x = Scale (p[i].x);
	y = Scale (p[i].y);
	next_xi = skip_xi;
	next_yi = skip_yi;
	if (Hint (path) && p[i+1].y != -64 && p[i+1].x != -64)
	{
	    if (p[i].x == p[i+1].x)
	    {
		x = SNAPI(path, x);
		skip_xi = i + 2;
	    }
	    if (p[i].y == p[i+1].y)
	    {
		y = SNAPI(path, y);
		skip_yi = i + 2;
	    }
	}
	if (i >= next_xi)
	{
	    if (x < left)
		left = x;
	    if (x > right)
		right = x;
	}
	if (i >= next_yi)
	{
	    if (y < top)
		top = y;
	    if (y > bottom)
		bottom = y;
	}
    }
    
    left -= pen_size * 2;
    right += pen_size * 2;

    if (i == 1)
    {
	left = Scale(p[0].x);
	top = bottom = baseline;
	right = Scale(p[0].y);
    }
    m->left_side_bearing = SNAPI(path, -left);
    m->right_side_bearing = SNAPI(path,right);
    m->width = m->left_side_bearing + m->right_side_bearing;
    m->ascent = baseline - SNAPI(path, top);
    m->descent = SNAPI(path, bottom) - baseline;
    m->font_descent = SNAPI(path, path->state.font_size / 3);
    m->font_ascent = SNAPI(path,path->state.font_size) - m->font_descent;
}

void
twin_path_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    const twin_gpoint_t	*p = _twin_ucs4_base (ucs4);
    int			i;
    twin_spoint_t	origin;
    twin_fixed_t	xc, yc;
    twin_sfixed_t	sx, sy;
    twin_path_t		*stroke;
    twin_path_t		*pen;
    twin_fixed_t	w;
    twin_fixed_t	x, y;
    twin_fixed_t	pen_size;
    twin_matrix_t	pen_matrix;
    twin_fixed_t	pen_adjust;
    twin_gfixed_t    	snap_x[TWIN_GLYPH_MAX_POINTS];
    twin_gfixed_t    	snap_y[TWIN_GLYPH_MAX_POINTS];
    twin_text_metrics_t	metrics;
    int			nsnap_x = 0, nsnap_y = 0;
    
    twin_text_metrics_ucs4 (path, ucs4, &metrics);
    
    origin = _twin_path_current_spoint (path);
    
    if (Hint (path))
    {
	nsnap_x = 0;
	nsnap_y = 0;
    
	/* snap left and right boundaries */
	
	nsnap_x = _add_snap (snap_x, nsnap_x, p[0].x);
	nsnap_x = _add_snap (snap_x, nsnap_x, p[0].y);
	
	/* snap baseline, x height and cap height  */
	nsnap_y = _add_snap (snap_y, nsnap_y, 9);
	nsnap_y = _add_snap (snap_y, nsnap_y, -5);
	nsnap_y = _add_snap (snap_y, nsnap_y, -12);
	
	/*
	 * Locate horizontal and vertical segments
	 */
	for (i = 1; p[i].y != -64 && p[i+1].y != -64; i++)
	{
	    if (p[i].x == -64 || p[i+1].x == -64)
		continue;
	    if (p[i].x == p[i+1].x)
		nsnap_x = _add_snap (snap_x, nsnap_x, p[i].x);
	    if (p[i].y == p[i+1].y)
		nsnap_y = _add_snap (snap_y, nsnap_y, p[i].y);
	}
    
	qsort (snap_x, nsnap_x, sizeof (twin_gfixed_t), compare_snap);
	qsort (snap_y, nsnap_y, sizeof (twin_gfixed_t), compare_snap);
    
	DBGMSG (("snap_x:"));
	for (i = 0; i < nsnap_x; i++)
	    DBGMSG ((" %d", snap_x[i])); 
	DBGMSG (("\n"));
	
	DBGMSG (("snap_y:"));
	for (i = 0; i < nsnap_y; i++)
	    DBGMSG ((" %d", snap_y[i])); 
	DBGMSG (("\n"));
    }

    stroke = twin_path_create ();
    twin_path_set_matrix (stroke, twin_path_current_matrix (path));
    
    pen_size = _twin_pen_size (path);

    if (Hint (path))
	pen_adjust = pen_size & TWIN_FIXED_HALF;
    else
	pen_adjust = 0;
    
    pen = twin_path_create ();
    pen_matrix = twin_path_current_matrix (path);
    /* eliminate translation part */
    pen_matrix.m[2][0] = 0;
    pen_matrix.m[2][1] = 0;
    twin_path_set_matrix (pen, pen_matrix);

    twin_path_circle (pen, pen_size);

    xc = metrics.left_side_bearing + pen_adjust;
    yc = SNAPY(TWIN_FONT_BASELINE) + pen_adjust;
    
    for (i = 1; p[i].y != -64; i++)
	if (p[i].x == -64)
	    twin_path_close (stroke);
	else
	{
	    x = xc + SNAPX(p[i].x);
	    y = yc + SNAPY(p[i].y);

	    if (path->state.font_style & TWIN_TEXT_OBLIQUE)
		x -= y / 4;
	    sx = origin.x + _twin_matrix_dx (&path->state.matrix, x, y);
	    sy = origin.y + _twin_matrix_dy (&path->state.matrix, x, y);
	    DBGMSG(("x: %9.4f, y: %9.4f -> sx: %9.4f, sy: %9.4f\n",
		    F(x), F(y), S(sx), S(sy)));
	    _twin_path_sdraw (stroke, sx, sy);
	}

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (stroke);
    twin_path_destroy (pen);
    
    w = metrics.width;

    _twin_path_smove (path, 
		      origin.x + _twin_matrix_dx (&path->state.matrix, w, 0),
		      origin.y + _twin_matrix_dy (&path->state.matrix, w, 0));
}

twin_fixed_t
twin_width_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    twin_text_metrics_t	metrics;
    
    twin_text_metrics_ucs4 (path, ucs4, &metrics);
    return metrics.width;
}

static int
_twin_utf8_to_ucs4 (const char	    *src_orig,
		    twin_ucs4_t	    *dst)
{
    const char	    *src = src_orig;
    char	    s;
    int		    extra;
    twin_ucs4_t	    result;

    s = *src++;
    if (!s)
	return 0;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	if (!s)
	    return -1;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

void
twin_path_utf8 (twin_path_t *path, const char *string)
{
    int		len;
    twin_ucs4_t	ucs4;

    while ((len = _twin_utf8_to_ucs4(string, &ucs4)) > 0)
    {
	twin_path_ucs4 (path, ucs4);
	string += len;
    }
}

twin_fixed_t
twin_width_utf8 (twin_path_t *path, const char *string)
{
    int		    len;
    twin_ucs4_t	    ucs4;
    twin_fixed_t    w = 0;

    while ((len = _twin_utf8_to_ucs4(string, &ucs4)) > 0)
    {
	w += twin_width_ucs4 (path, ucs4);
	string += len;
    }
    return w;
}

void
twin_text_metrics_utf8 (twin_path_t	    *path, 
			const char	    *string,
			twin_text_metrics_t *m)
{
    int			len;
    twin_ucs4_t		ucs4;
    twin_fixed_t	w = 0;
    twin_text_metrics_t	c;
    twin_bool_t		first = TWIN_TRUE;

    while ((len = _twin_utf8_to_ucs4(string, &ucs4)) > 0)
    {
	twin_text_metrics_ucs4 (path, ucs4, &c);
	if (first)
	    *m = c;
	else
	{
	    c.left_side_bearing += w;
	    c.right_side_bearing += w;
	    c.width += w;

	    if (c.left_side_bearing > m->left_side_bearing)
		m->left_side_bearing = c.left_side_bearing;
	    if (c.right_side_bearing > m->right_side_bearing)
		m->right_side_bearing = c.right_side_bearing;
	    if (c.width > m->width)
		m->width = c.width;
	    if (c.ascent < m->ascent)
		m->ascent = c.ascent;
	    if (c.descent > m->descent)
		m->descent = c.descent;
	}
	w = c.width;
	string += len;
    }
}
