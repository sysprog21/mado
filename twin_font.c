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

#define Scale(f)	(((twin_fixed_t) (f) * path->state.font_size) >> 6)

#define Hint(p)		(((p)->state.font_style & TWIN_TEXT_UNHINTED) == 0)

twin_bool_t
twin_has_ucs4 (twin_ucs4_t ucs4)
{
    return ucs4 <= TWIN_FONT_MAX && _twin_glyph_offsets[ucs4] != 0;
}

#define SNAPI(p)	(((p) + 0x7fff) & ~0xffff)
#define SNAPH(p)	(((p) + 0x3fff) & ~0x7fff)

#if 0
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
	    twin_fixed_t    snap_before = SNAPI(before);
	    twin_fixed_t    snap_after = SNAPI(after);
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
#endif

#define SNAPX(p)	_snap (path, p, snap_x, nsnap_x)
#define SNAPY(p)	_snap (path, p, snap_y, nsnap_y)

static const twin_gpoint_t *
_twin_ucs4_base(twin_ucs4_t ucs4)
{
    if (ucs4 > TWIN_FONT_MAX) ucs4 = 0;

    return _twin_glyphs + _twin_glyph_offsets[ucs4];
}

static const signed char *
_twin_g_base (twin_ucs4_t ucs4)
{
    if (ucs4 > TWIN_FONT_MAX) ucs4 = 0;

    return _twin_gtable + _twin_g_offsets[ucs4];
}

static twin_fixed_t
_twin_pen_size (twin_path_t *path)
{
    twin_fixed_t    pen_size;
    
    pen_size = path->state.font_size / 24;
    
    if (Hint (path))
    {
	pen_size = SNAPH(pen_size);
	if( pen_size < TWIN_FIXED_HALF)
	pen_size = TWIN_FIXED_HALF;
    }
    
    if (path->state.font_style & TWIN_TEXT_BOLD)
    {
	twin_fixed_t	pen_add = pen_size >> 1;

	if (Hint (path))
	{
	    pen_add = SNAPH (pen_add);
	    if (pen_add == 0) 
		pen_add = TWIN_FIXED_HALF;
	}
	pen_size += pen_add;
    }
    return pen_size;
}

#define Margin(pen_size)    ((pen_size) * 2)

void
twin_text_metrics_ucs4 (twin_path_t	    *path, 
			twin_ucs4_t	    ucs4, 
			twin_text_metrics_t *m)
{
    const signed char	*g = _twin_g_base (ucs4);
    twin_fixed_t	pen_size = _twin_pen_size (path);
    twin_fixed_t	left = Scale(twin_glyph_left(g));
    twin_fixed_t	right = Scale (twin_glyph_right(g)) + pen_size * 2;
    twin_fixed_t	ascent = Scale (twin_glyph_ascent(g)) + pen_size * 2;
    twin_fixed_t	descent = Scale (twin_glyph_descent(g));
    twin_fixed_t	font_spacing = path->state.font_size;
    twin_fixed_t	font_descent = font_spacing / 3;
    twin_fixed_t	font_ascent = font_spacing - font_descent;

    if (Hint(path))
    {
	left = SNAPI(left);
	right = SNAPI(right);
	ascent = SNAPI(ascent);
	descent = SNAPI(descent);
	font_descent = SNAPI(font_descent);
	font_ascent = SNAPI(font_ascent);
    }
    m->left_side_bearing = left + Margin(pen_size);
    m->right_side_bearing = right + Margin(pen_size);
    m->ascent = ascent;
    m->descent = descent;
    m->width = m->right_side_bearing + Margin(pen_size);
    m->font_ascent = font_ascent + Margin(pen_size);
    m->font_descent = font_ascent + Margin(pen_size);
}

#include <stdio.h>

#define TWIN_MAX_POINTS	    50
#define TWIN_MAX_STROKE	    50

typedef enum { twin_gmove, twin_gline, twin_gcurve } twin_gcmd_t;

typedef struct _twin_gop {
    twin_gcmd_t	    cmd;
    twin_gpoint_t   p[3];
} twin_gop_t;
    
typedef struct _twin_stroke {
    int		    n;
    twin_gpoint_t   p[TWIN_MAX_STROKE];
} twin_stroke_t;

typedef struct _twin_snap {
    int		    n;
    twin_gfixed_t   s[TWIN_MAX_POINTS];
} twin_snap_t;

typedef struct _twin_glyph {
    twin_ucs4_t	    ucs4;
    int		    offset;
    twin_bool_t	    exists;
    twin_gfixed_t   left, right, top, bottom;
    int		    n;
    twin_stroke_t   s[TWIN_MAX_STROKE];
    int		    nop;
    twin_gop_t	    op[TWIN_MAX_STROKE];
    twin_snap_t	    snap_x;
    twin_snap_t	    snap_y;
} twin_glyph_t;

static twin_glyph_t glyphs[0x80];

static void
twin_add_snap (twin_snap_t *snap, twin_gfixed_t v)
{
    int	    n;

    for (n = 0; n < snap->n; n++)
    {
	if (snap->s[n] == v)
	    return;
	if (snap->s[n] > v)
	    break;
    }
    memmove (&snap->s[n+1], &snap->s[n], snap->n - n);
    snap->s[n] = v;
    snap->n++;
}

static int
twin_n_in_spline (twin_gpoint_t *p, int n)
{
    return 0;
}

static void
twin_spline_fit (twin_gpoint_t *p, int n, twin_gpoint_t *c1, twin_gpoint_t *c2)
{
}

static char *
_ucs4_string (twin_ucs4_t ucs4)
{
    static char	buf[10];
    if (ucs4 < ' ' || ucs4 > '~')
    {
	if (!ucs4)
	    return "\\0";
	sprintf (buf, "\\0%o", ucs4);
	return buf;
    }
    sprintf (buf, "%c", ucs4);
    return buf;
}

static twin_gfixed_t
px (const twin_gpoint_t *p, int i)
{
    return p[i].x << 1;
}

static twin_gfixed_t
py (const twin_gpoint_t *p, int i)
{
    return p[i].y << 1;
}

static void
twin_dump_glyphs (void)
{
    twin_ucs4_t	ucs4;
    int		offset = 0;
    int		i, j;
    twin_gop_t	*gop;

    for (ucs4 = 0; ucs4 < 0x80; ucs4++)
    {
	const twin_gpoint_t	*p = _twin_ucs4_base (ucs4);
	twin_glyph_t		*g = &glyphs[ucs4];
	twin_stroke_t		*s;
	twin_gfixed_t		origin;
	twin_gfixed_t		left, right, top, bottom;
	twin_gfixed_t		baseline;
	twin_gfixed_t		x, y;
	twin_bool_t		move;
	
	g->ucs4 = ucs4;
	g->n = 0;

	if (ucs4 && p == _twin_glyphs) continue;
	
	if (p[1].y == -64)
	{
	    origin = 0;
	    left = 0;
	    right = 4;
	    top = 18;
	    bottom = 18;
	    baseline = 18;
	}
	else
	{
	    origin = 64;
	    left = 64;
	    right = -64;
	    top = 64;
	    bottom = -64;
	    baseline = 18;
	    for (i = 1, move = TWIN_TRUE; p[i].y != -64; i++)
	    {
		if (p[i].x == -64) { move = TWIN_TRUE; continue; }
		if (py(p,i) <= baseline && px(p,i) < origin) origin = px(p,i);
		if (px(p,i) < left) left = px(p,i);
		if (px(p,i) > right) right = px(p,i);
		if (py(p,i) < top) top = py(p,i);
		if (py(p,i) > bottom) bottom = py(p,i); 
		move = TWIN_FALSE;
	    }
	}
	left -= origin;
	right -= origin;;
	bottom -= baseline;
	top -= baseline;

	/*
	 * Convert from hershey format to internal format
	 */
	for (i = 1, move = TWIN_TRUE; p[i].y != -64; i++)
	{
	    if (p[i].x == -64) { move = TWIN_TRUE; continue; }

	    x = px(p,i) - origin;
	    y = py(p,i) - baseline;
	    if (move)
		if (g->s[g->n].n)
		    ++g->n;
	    s = &g->s[g->n];
	    s->p[s->n].x = x;
	    s->p[s->n].y = y;
	    s->n++;
	    
	    move = TWIN_FALSE;
	}
    	if (g->s[g->n].n)
    	    ++g->n;

	g->left = left;
	g->right = right;
	g->top = top;
	g->bottom = bottom;

	/*
	 * Find snap points
	 */
	twin_add_snap (&g->snap_x, 0);	    /* origin */
	twin_add_snap (&g->snap_x, right);  /* right */
	
	twin_add_snap (&g->snap_y, 0);	    /* baseline */
	twin_add_snap (&g->snap_y, -15);    /* x height */
	twin_add_snap (&g->snap_y, -21);    /* cap height */
	
	for (i = 0; i < g->n; i++)
	{
	    s = &g->s[i];
	    for (j = 0; j < s->n - 1; j++)
	    {
		if (s->p[j].x == s->p[j+1].x)
		    twin_add_snap (&g->snap_x, s->p[j].x);
		if (s->p[j].y == s->p[j+1].y)
		    twin_add_snap (&g->snap_y, s->p[j].y);
	    }
	}

	/*
	 * Now convert to gops and try to locate splines
	 */
	
	gop = &g->op[0];
	for (i = 0; i < g->n; i++)
	{
	    s = &g->s[i];
	    gop->cmd = twin_gmove;
	    gop->p[0] = s->p[0];
	    gop++;
	    for (j = 0; j < s->n - 1;)
	    {
		int ns = twin_n_in_spline (s->p + j, s->n - j);

		if (ns)
		{
		    twin_spline_fit (s->p + j, ns,
				     &gop->p[0], &gop->p[1]);
		    gop->cmd = twin_gcurve;
		    gop->p[2] = s->p[j + ns - 1];
		    gop++;
		    j += ns;
		}
		else
		{
		    gop->cmd = twin_gline;
		    gop->p[0] = s->p[j+1];
		    gop++;
		    j++;
		}
	    }
	}
	g->nop = gop - &g->op[0];
	g->exists = TWIN_TRUE;
    }

    printf ("const signed char _twin_gtable[] = {\n");
    for (ucs4 = 0; ucs4 < 0x80; ucs4++)
    {
	twin_glyph_t		*g = &glyphs[ucs4];
	
	if (!g->exists) continue;
	
	g->offset = offset;

	printf ("/* 0x%x '%s' */\n", g->ucs4, _ucs4_string (g->ucs4));
	printf ("    %d, %d, %d, %d, %d, %d,\n",
		g->left, g->right, -g->top, g->bottom, g->snap_x.n, g->snap_y.n);

	offset += 6;

	printf ("   ");
	for (i = 0; i < g->snap_x.n; i++)
	    printf (" %d,", g->snap_x.s[i]);
	printf (" /* snap_x */\n");

	offset += g->snap_x.n;
	
	printf ("   ");
	for (i = 0; i < g->snap_y.n; i++)
	    printf (" %d,", g->snap_y.s[i]);
	printf (" /* snap_y */\n");
	
	offset += g->snap_y.n;

#define CO(n)	gop->p[n].x, gop->p[n].y
	for (i = 0; i < g->nop; i++)
	{
	    gop = &g->op[i];
	    switch (gop->cmd) {
	    case twin_gmove:
		printf ("    'm', %d, %d,\n", CO(0));
		offset += 3;
		break;
	    case twin_gline:
		printf ("    'l', %d, %d,\n", CO(0));
		offset += 3;
		break;
	    case twin_gcurve:
		printf ("    'c', %d, %d, %d, %d, %d, %d,\n",
			CO(0), CO(1), CO(2));
		offset += 7;
		break;
	    }
	}
	printf ("    'e',\n");
	offset++;
    }
    printf ("};\n\n");
    printf ("const uint16_t _twin_g_offsets[] = {");
    for (ucs4 = 0; ucs4 < 0x80; ucs4++)
    {
	twin_glyph_t		*g = &glyphs[ucs4];
	
	if ((ucs4 & 7) == 0)
	    printf ("\n    ");
	else
	    printf (" ");
	printf ("%4d,", g->offset);
    }
    printf ("\n};\n");
    fflush (stdout);
    exit (0);
}

void
twin_path_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    const signed char	*b = _twin_g_base (ucs4);
    const signed char	*g = twin_glyph_draw(b);
    twin_spoint_t	origin;
    twin_fixed_t	x1, y1, x2, y2, x3, y3;
    twin_fixed_t	xo, yo;
    twin_path_t		*stroke;
    twin_path_t		*pen;
    twin_fixed_t	pen_size = _twin_pen_size (path);
    twin_matrix_t	pen_matrix;
    twin_text_metrics_t	metrics;
    
    if (0)
    {
	static int been_here = 0;
	if (!been_here) { been_here = 1; twin_dump_glyphs (); }
    }

    twin_text_metrics_ucs4 (path, ucs4, &metrics);
    
    origin = _twin_path_current_spoint (path);
    
    stroke = twin_path_create ();
    
    twin_path_set_matrix (stroke, twin_path_current_matrix (path));
    
    pen = twin_path_create ();
    pen_matrix = twin_path_current_matrix (path);
    /* eliminate translation part */
    pen_matrix.m[2][0] = 0;
    pen_matrix.m[2][1] = 0;
    twin_path_set_matrix (pen, pen_matrix);

    twin_path_circle (pen, pen_size);

#define PX(_x,_y)   (origin.x + _twin_matrix_dx (&path->state.matrix, _x, _y))
#define PY(_x,_y)   (origin.y + _twin_matrix_dy (&path->state.matrix, _x, _y))
    
    xo = pen_size * 3;
    yo = -pen_size;
    
    for (;;) {
	switch (*g++) {
	case 'm':
	    x1 = Scale (*g++) + xo;
	    y1 = Scale (*g++) + yo;
	    if (path->state.font_style & TWIN_TEXT_OBLIQUE)
		x1 -= y1 >> 2;
	    _twin_path_smove (stroke, PX(x1,y1), PY(x1,y1));
	    continue;
	case 'l':
	    x1 = Scale (*g++) + xo;
	    y1 = Scale (*g++) + yo;
	    if (path->state.font_style & TWIN_TEXT_OBLIQUE)
		x1 -= y1 >> 2;
	    _twin_path_sdraw (stroke, PX(x1,y1), PY(x1,y1));
	    continue;
	case 'c':
	    x1 = Scale (*g++) + xo;
	    y1 = Scale (*g++) + yo;
	    x2 = Scale (*g++) + xo;
	    y2 = Scale (*g++) + yo;
	    x3 = Scale (*g++) + xo;
	    y3 = Scale (*g++) + yo;
	    if (path->state.font_style & TWIN_TEXT_OBLIQUE)
	    {
		x1 -= y1 >> 2;
		x2 -= y2 >> 2;
		x3 -= y3 >> 2;
	    }
	    _twin_path_scurve (stroke, PX(x1,y1), PY(x1,y1),
			       PX(x2,y2), PY(x2,y2), PX(x3,y3), PY(x3,y3));
	    continue;
	case 'e':
	    break;
	}
	break;
    }

    twin_path_convolve (path, stroke, pen);
    twin_path_destroy (stroke);
    twin_path_destroy (pen);
    
    _twin_path_smove (path, PX(metrics.width, 0), PY(metrics.width,0));
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
