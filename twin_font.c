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

twin_bool_t
twin_has_ucs4 (twin_ucs4_t ucs4)
{
    return ucs4 <= TWIN_FONT_MAX && _twin_glyph_offsets[ucs4] != 0;
}

#if 0
static int
compare_snap (const void *av, const void *bv)
{
    const twin_gfixed_t   *a = av;
    const twin_gfixed_t   *b = bv;

    return (int) (*a - *b);
}

#define SNAPI(p)	(((p) + 0x7) & ~0xf)
#define SNAPH(p)	(((p) + 0x3) & ~0x7)

static twin_fixed_t
_snap (twin_gfixed_t g, twin_fixed_t scale, twin_gfixed_t *snap, int nsnap)
{
    int		    s;
    twin_fixed_t    v;

    v = S(g, scale);
    for (s = 0; s < nsnap - 1; s++)
    {
	if (snap[s] <= g && g <= snap[s+1])
	{
	    twin_fixed_t    before = S(snap[s],scale);
	    twin_fixed_t    after = S(snap[s+1],scale);
	    twin_fixed_t    dist = after - before;
	    twin_fixed_t    snap_before = SNAPI(before);
	    twin_fixed_t    snap_after = SNAPI(after);
	    twin_fixed_t    move_before = snap_before - before;
	    twin_fixed_t    move_after = snap_after - after;
	    twin_fixed_t    dist_before = v - before;
	    twin_fixed_t    dist_after = after - v;
	    twin_fixed_t    move = ((twin_dfixed_t) dist_before * move_after + 
				    (twin_dfixed_t) dist_after * move_before) / dist;
	    DBGOUT (("%d <= %d <= %d\n", snap[s], g, snap[s+1]));
	    DBGOUT (("%9.4f <= %9.4f <= %9.4f\n", F(before), F(v), F(after)));
	    DBGOUT (("before: %9.4f -> %9.4f\n", F(before), F(snap_before)));
	    DBGOUT (("after: %9.4f -> %9.4f\n", F(after), F(snap_after)));
	    DBGOUT (("v: %9.4f -> %9.4f\n", F(v), F(v+move)));
	    v += move;
	    break;
	}
    }
    DBGOUT (("_snap: %d => %9.4f\n", g, F(v)));
    return v;
}

#define SNAPX(p)	_snap (p, path->state.font_size, snap_x, nsnap_x)
#define SNAPY(p)	_snap (p, path->state.font_size, snap_y, nsnap_y)

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
#endif

static const twin_gpoint_t *
_twin_ucs4_base(twin_ucs4_t ucs4)
{
    if (ucs4 > TWIN_FONT_MAX) ucs4 = 0;

    return _twin_glyphs + _twin_glyph_offsets[ucs4];
}

#define TWIN_FONT_BASELINE  9

void
twin_path_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    const twin_gpoint_t	*p = 0;
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
#if 0
    twin_fixed_t	pen_adjust;
    twin_gfixed_t    	*snap_x, *snap_y;
    int			nsnap_x, nsnap_y;
    int			npoints;
#endif
    
    p = _twin_ucs4_base (ucs4);
    
    origin = _twin_path_current_spoint (path);
    
#if 0
    for (i = 1; p[i].y != -64; i++)
	;

    npoints = i - 1 + 3;
    
    snap_x = malloc ((npoints * 2) * sizeof (twin_gfixed_t));
    snap_y = snap_x + npoints;
    
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
#endif

#if 0
    DBGOUT (("snap_x:"));
    for (i = 0; i < nsnap_x; i++)
	DBGOUT ((" %d", snap_x[i])); 
    DBGOUT (("\n"));
    
    DBGOUT (("snap_y:"));
    for (i = 0; i < nsnap_y; i++)
	DBGOUT ((" %d", snap_y[i])); 
    DBGOUT (("\n"));
#endif

    stroke = twin_path_create ();
    twin_path_set_matrix (stroke, twin_path_current_matrix (path));
    
#if 0
    /* snap pen size to half integer value */
    sx = _twin_matrix_dx (&path->state.matrix, 
			  path->state.font_size, path->state.font_size);
    
    pen_size = SNAPH(sx / 24);
    if (pen_size < TWIN_SFIXED_HALF)
	pen_size = TWIN_SFIXED_HALF;
    
    if (path->state.font_style & TWIN_TEXT_BOLD)
    {
	twin_fixed_t	pen_add = SNAPH(pen_size >> 1);
	if (pen_add == 0) 
	    pen_add = TWIN_SFIXED_HALF;
	pen_size += pen_add;
    }
    
    pen_adjust = pen_size & TWIN_SFIXED_HALF;
#endif
    
    pen = twin_path_create ();
    pen_matrix = twin_path_current_matrix (path);
    /* eliminate translation part */
    pen_matrix.m[2][0] = 0;
    pen_matrix.m[2][1] = 0;
    twin_path_set_matrix (pen, pen_matrix);
    pen_size = path->state.font_size / 24;
    if (path->state.font_style & TWIN_TEXT_BOLD)
	pen_size += pen_size / 2;

    twin_path_circle (pen, pen_size);

    xc = -ScaleX(p[0].x);
    yc = ScaleY(TWIN_FONT_BASELINE);
    
    for (i = 1; p[i].y != -64; i++)
	if (p[i].x == -64)
	    twin_path_close (stroke);
	else
	{
	    x = xc + ScaleX(p[i].x);
	    y = yc + ScaleY(p[i].y);

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
    
#if 0
    free (snap_x);
#endif

    w = twin_width_ucs4 (path, ucs4);
    _twin_path_smove (path, 
		      origin.x + _twin_matrix_dx (&path->state.matrix, w, 0),
		      origin.y + _twin_matrix_dy (&path->state.matrix, w, 0));
}

int
twin_width_ucs4 (twin_path_t *path, twin_ucs4_t ucs4)
{
    const twin_gpoint_t	*p = _twin_ucs4_base (ucs4);
    twin_fixed_t	left, right;
    
    left = ScaleX (p[0].x);
    right = ScaleX (p[0].y);
    
    return right - left;
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

