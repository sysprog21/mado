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

#define SNAPI(p)	(((p) + 0x8000) & ~0xffff)
#define SNAPH(p)	(((p) + 0x4000) & ~0x7fff)
#define FX(g,i)		(((g) * (i)->scale.x) >> 6)
#define FY(g,i)		(((g) * (i)->scale.y) >> 6)

typedef struct _twin_text_info {
    twin_point_t    scale;
    twin_point_t    pen;
    twin_point_t    margin;
    twin_point_t    reverse_scale;
    twin_bool_t	    snap;
    twin_matrix_t   matrix;
    twin_matrix_t   pen_matrix;
    int		    n_snap_x;
    twin_fixed_t    snap_x[TWIN_GLYPH_MAX_SNAP_X];
    int		    n_snap_y;
    twin_fixed_t    snap_y[TWIN_GLYPH_MAX_SNAP_Y];
} twin_text_info_t;

static void
_twin_text_compute_info (twin_path_t *path, twin_text_info_t *info)
{
    twin_spoint_t   origin = _twin_path_current_spoint (path);
    
    /*
     * Only hint axis aligned text 
     */
    if ((path->state.font_style & TWIN_TEXT_UNHINTED) == 0 &&
	((path->state.matrix.m[0][1] == 0 &&
	  path->state.matrix.m[1][0] == 0) ||
	 (path->state.matrix.m[0][0] == 0 &&
	  path->state.matrix.m[1][1] == 0)))
    {
	int	xi, yi;

	if (path->state.matrix.m[0][0] != 0)
	    xi = 0;
	else
	    xi = 1;
	yi = 1-xi;
	info->snap = TWIN_TRUE;
	info->matrix.m[xi][0] = TWIN_FIXED_ONE;
	info->matrix.m[xi][1] = 0;
	info->matrix.m[yi][0] = 0;
	info->matrix.m[yi][1] = TWIN_FIXED_ONE;
	info->matrix.m[2][0] = twin_sfixed_to_fixed (origin.x);
	info->matrix.m[2][1] = twin_sfixed_to_fixed (origin.y);
	info->scale.x = twin_fixed_mul (path->state.font_size, 
					path->state.matrix.m[0][xi]);
	info->reverse_scale.x = twin_fixed_div (TWIN_FIXED_ONE,
						path->state.matrix.m[0][xi]);
	if (info->scale.x < 0)
	{
	    info->scale.x = -info->scale.x;
	    info->reverse_scale.x = -info->reverse_scale.x;
	    info->matrix.m[0][xi] = -info->matrix.m[0][xi];
	    info->matrix.m[1][xi] = -info->matrix.m[1][xi];
	}
	info->scale.y = twin_fixed_mul (path->state.font_size,
					path->state.matrix.m[1][yi]);
	info->reverse_scale.y = twin_fixed_div (TWIN_FIXED_ONE,
						path->state.matrix.m[1][yi]);
	if (info->scale.y < 0)
	{
	    info->scale.y = -info->scale.y;
	    info->reverse_scale.y = -info->reverse_scale.y;
	    info->matrix.m[0][yi] = -info->matrix.m[0][yi];
	    info->matrix.m[1][yi] = -info->matrix.m[1][yi];
	}

	info->pen.x = SNAPH(info->scale.x / 24);
	info->pen.y = SNAPH(info->scale.y / 24);
	if (info->pen.x < TWIN_FIXED_HALF)
	    info->pen.x = TWIN_FIXED_HALF;
	if (info->pen.y < TWIN_FIXED_HALF)
	    info->pen.y = TWIN_FIXED_HALF;
	info->margin.x = info->pen.x;
	info->margin.y = info->pen.y;
	if (path->state.font_style & TWIN_TEXT_BOLD)
	{
	    twin_fixed_t    pen_x_add = SNAPH(info->pen.x >> 1);
	    twin_fixed_t    pen_y_add = SNAPH(info->pen.y >> 1);

	    if (pen_x_add < TWIN_FIXED_HALF)
		pen_x_add = TWIN_FIXED_HALF;
	    if (pen_y_add < TWIN_FIXED_HALF)
		pen_y_add = TWIN_FIXED_HALF;
	    info->pen.x += pen_x_add;
	    info->pen.y += pen_y_add;
	}
	DBGMSG (("pen: %9.4f %9.4f\n", F(info->pen.x), F(info->pen.y)));
	
	twin_matrix_translate (&info->matrix,
			       SNAPI(info->margin.x) + info->pen.x,
			       -info->pen.y);
	info->pen_matrix = info->matrix;
    }
    else
    {
	info->snap = TWIN_FALSE;
	info->matrix = path->state.matrix;
	info->matrix.m[2][0] = twin_sfixed_to_fixed (origin.x);
	info->matrix.m[2][1] = twin_sfixed_to_fixed (origin.y);
	info->scale.x = path->state.font_size;
	info->scale.y = path->state.font_size;
	
	if (path->state.font_style & TWIN_TEXT_BOLD)
	    info->pen.x = path->state.font_size / 16;
	else
	    info->pen.x = path->state.font_size / 24;
	info->pen.y = info->pen.x;

	info->margin.x = path->state.font_size / 24;
	info->margin.y = info->margin.x;
	
	info->pen_matrix = path->state.matrix;
	twin_matrix_translate (&info->matrix,
			       info->margin.x + info->pen.x, -info->pen.y);
    }
    info->pen_matrix.m[2][0] = 0; info->pen_matrix.m[2][1] = 0;
    twin_matrix_scale (&info->pen_matrix, info->pen.x, info->pen.y);
	
    if (path->state.font_style & TWIN_TEXT_OBLIQUE)
    {
	twin_matrix_t	m;
	
	m.m[0][0] = TWIN_FIXED_ONE;	    m.m[0][1] = 0;
	m.m[1][0] = -TWIN_FIXED_ONE / 4;    m.m[1][1] = TWIN_FIXED_ONE;
	m.m[2][0] = 0;			    m.m[2][1] = 0;
	twin_matrix_multiply (&info->matrix, &m, &info->matrix);
    }
    DBGMSG (("%9.4f %9.4f\n", F(info->matrix.m[0][0]), F(info->matrix.m[0][1])));
    DBGMSG (("%9.4f %9.4f\n", F(info->matrix.m[1][0]), F(info->matrix.m[1][1])));
    DBGMSG (("%9.4f %9.4f\n", F(info->matrix.m[2][0]), F(info->matrix.m[2][1])));
}

static void
_twin_text_compute_snap (twin_text_info_t   *info,
			 const signed char  *b)
{
    int			s, n;
    const signed char	*snap;

    snap = twin_glyph_snap_x (b);
    n = twin_glyph_n_snap_x (b);
    info->n_snap_x = n;
    for (s = 0; s < n; s++)
	info->snap_x[s] = FX(snap[s], info);

    snap = twin_glyph_snap_y (b);
    n = twin_glyph_n_snap_y (b);
    info->n_snap_y = n;
    for (s = 0; s < n; s++)
	info->snap_y[s] = FY(snap[s], info);
}

static twin_path_t *
_twin_text_compute_pen (twin_text_info_t *info)
{
    twin_path_t	*pen = twin_path_create ();

    twin_path_set_matrix (pen, info->pen_matrix);
    twin_path_circle (pen, TWIN_FIXED_ONE);
    return pen;
}

static twin_fixed_t
_twin_snap (twin_fixed_t    v,
	    twin_fixed_t    *snap,
	    int		    n)
{
    int	s;

    for (s = 0; s < n - 1; s++)
    {
	if (snap[s] <= v && v <= snap[s+1])
	{
	    twin_fixed_t    before = snap[s];
	    twin_fixed_t    after = snap[s+1];
	    twin_fixed_t    dist = after - before;
	    twin_fixed_t    snap_before = SNAPI(before);
	    twin_fixed_t    snap_after = SNAPI(after);
	    twin_fixed_t    move_before = snap_before - before;
	    twin_fixed_t    move_after = snap_after - after;
	    twin_fixed_t    dist_before = v - before;
	    twin_fixed_t    dist_after = after - v;
	    twin_fixed_t    move = ((int64_t) dist_before * move_after + 
				    (int64_t) dist_after * move_before) / dist;
	    DBGMSG (("%9.4f <= %9.4f <= %9.4f\n", F(before), F(v), F(after)));
	    DBGMSG (("before: %9.4f -> %9.4f\n", F(before), F(snap_before)));
	    DBGMSG (("after: %9.4f -> %9.4f\n", F(after), F(snap_after)));
	    DBGMSG (("v: %9.4f -> %9.4f\n", F(v), F(v+move)));
	    v += move;
	    break;
	}
    }
    return v;
}

twin_bool_t
twin_has_ucs4 (twin_ucs4_t ucs4)
{
    return ucs4 <= TWIN_FONT_MAX && _twin_g_offsets[ucs4] != 0;
}

#define SNAPX(p)	_snap (path, p, snap_x, nsnap_x)
#define SNAPY(p)	_snap (path, p, snap_y, nsnap_y)

static const signed char *
_twin_g_base (twin_ucs4_t ucs4)
{
    if (ucs4 > TWIN_FONT_MAX) ucs4 = 0;

    return _twin_gtable + _twin_g_offsets[ucs4];
}

static twin_fixed_t
_twin_glyph_width (twin_text_info_t	*info,
		   const signed char	*b)
{
    twin_fixed_t    right = FX(twin_glyph_right(b), info) + info->pen.x * 2;
    twin_fixed_t    right_side_bearing;
    twin_fixed_t    width;

    if (info->snap)
	right = SNAPI(_twin_snap (right, info->snap_x, info->n_snap_x));
	
    right_side_bearing = right + info->margin.x;
    width = right_side_bearing + info->margin.x;
    return width;
}

void
twin_text_metrics_ucs4 (twin_path_t	    *path, 
			twin_ucs4_t	    ucs4, 
			twin_text_metrics_t *m)
{
    const signed char	*b = _twin_g_base (ucs4);
    twin_text_info_t	info;
    twin_fixed_t	left, right, ascent, descent;
    twin_fixed_t	font_spacing;
    twin_fixed_t	font_descent;
    twin_fixed_t	font_ascent;
    twin_fixed_t	margin_x, margin_y;

    _twin_text_compute_info (path, &info);
    if (info.snap)
	_twin_text_compute_snap (&info, b);

    left = FX(twin_glyph_left(b), &info);
    right = FX(twin_glyph_right(b), &info) + info.pen.x * 2;
    ascent = FY(twin_glyph_ascent(b), &info) + info.pen.y * 2;
    descent = FY(twin_glyph_descent(b), &info);
    margin_x = info.margin.x;
    margin_y = info.margin.y;
    
    font_spacing = FY(TWIN_GFIXED_ONE, &info);
    font_descent = font_spacing / 3;
    font_ascent = font_spacing - font_descent;
    if (info.snap)
    {
	left = SNAPI(_twin_snap (left, info.snap_x, info.n_snap_x));
	right = SNAPI(_twin_snap (right, info.snap_x, info.n_snap_x));
	ascent = SNAPI(_twin_snap (ascent, info.snap_y, info.n_snap_y));
	descent = SNAPI(_twin_snap (descent, info.snap_y, info.n_snap_y));
	font_descent = SNAPI(font_descent);
	font_ascent = SNAPI(font_ascent);
	
	left = twin_fixed_mul (left, info.reverse_scale.x);
	right = twin_fixed_mul (right, info.reverse_scale.x);
	ascent = twin_fixed_mul (ascent, info.reverse_scale.y);
	descent = twin_fixed_mul (descent, info.reverse_scale.y);
	font_descent = twin_fixed_mul (font_descent, info.reverse_scale.y);
	font_ascent = twin_fixed_mul (font_ascent, info.reverse_scale.y);
	margin_x = twin_fixed_mul (margin_x, info.reverse_scale.x);
	margin_y = twin_fixed_mul (margin_y, info.reverse_scale.y);
    }
    m->left_side_bearing = left + margin_x;
    m->right_side_bearing = right + margin_x;
    m->ascent = ascent;
    m->descent = descent;
    m->width = m->right_side_bearing + margin_x;
    m->font_ascent = font_ascent + margin_y;
    m->font_descent = font_descent + margin_y;
}

void
twin_path_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    const signed char	*b = _twin_g_base (ucs4);
    const signed char	*g = twin_glyph_draw(b);
    twin_spoint_t	origin;
    twin_fixed_t	x1, y1, x2, y2, x3, y3;
    twin_path_t		*stroke;
    twin_path_t		*pen;
    twin_fixed_t	width;
    twin_text_info_t	info;
    
    _twin_text_compute_info (path, &info);
    if (info.snap)
	_twin_text_compute_snap (&info, b);

    origin = _twin_path_current_spoint (path);
    
    stroke = twin_path_create ();
    twin_path_set_matrix (stroke, info.matrix);
    pen = _twin_text_compute_pen (&info);
    
    for (;;) {
	switch (*g++) {
	case 'm':
	    x1 = FX(*g++, &info);
	    y1 = FY(*g++, &info);
	    if (info.snap)
	    {
		x1 = _twin_snap (x1, info.snap_x, info.n_snap_x);
		y1 = _twin_snap (y1, info.snap_y, info.n_snap_y);
	    }
	    DBGMSG (("m %9.4f %9.4f\n",
		    S(_twin_matrix_x (&stroke->state.matrix, x1, y1)),
		    S(_twin_matrix_y (&stroke->state.matrix, x1, y1))));
	    twin_path_move (stroke, x1, y1);
	    continue;
	case 'l':
	    x1 = FX(*g++, &info);
	    y1 = FY(*g++, &info);
	    if (info.snap)
	    {
		x1 = _twin_snap (x1, info.snap_x, info.n_snap_x);
		y1 = _twin_snap (y1, info.snap_y, info.n_snap_y);
	    }
	    DBGMSG (("l %9.4f %9.4f\n",
		    S(_twin_matrix_x (&stroke->state.matrix, x1, y1)),
		    S(_twin_matrix_y (&stroke->state.matrix, x1, y1))));
	    twin_path_draw (stroke, x1, y1);
	    continue;
	case 'c':
	    x1 = FX(*g++, &info);
	    y1 = FY(*g++, &info);
	    x2 = FX(*g++, &info);
	    y2 = FY(*g++, &info);
	    x3 = FX(*g++, &info);
	    y3 = FY(*g++, &info);
	    if (info.snap)
	    {
		x1 = _twin_snap (x1, info.snap_x, info.n_snap_x);
		y1 = _twin_snap (y1, info.snap_y, info.n_snap_y);
		x2 = _twin_snap (x2, info.snap_x, info.n_snap_x);
		y2 = _twin_snap (y2, info.snap_y, info.n_snap_y);
		x3 = _twin_snap (x3, info.snap_x, info.n_snap_x);
		y3 = _twin_snap (y3, info.snap_y, info.n_snap_y);
	    }
	    twin_path_curve (stroke, x1, y1, x2, y2, x3, y3);
	    continue;
	case 'e':
	    break;
	}
	break;
    }

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (pen);
    twin_path_destroy (stroke);
    
    width = _twin_glyph_width (&info, b);
    
    _twin_path_smove (path, 
		      origin.x + _twin_matrix_dx (&info.matrix, width, 0),
		      origin.y + _twin_matrix_dy (&info.matrix, width, 0));
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
	{
	    *m = c;
	    first = TWIN_FALSE;
	}
	else
	{
	    c.left_side_bearing += w;
	    c.right_side_bearing += w;
	    c.width += w;

	    if (c.left_side_bearing < m->left_side_bearing)
		m->left_side_bearing = c.left_side_bearing;
	    if (c.right_side_bearing > m->right_side_bearing)
		m->right_side_bearing = c.right_side_bearing;
	    if (c.width > m->width)
		m->width = c.width;
	    if (c.ascent > m->ascent)
		m->ascent = c.ascent;
	    if (c.descent > m->descent)
		m->descent = c.descent;
	}
	w = c.width;
	string += len;
    }
}
