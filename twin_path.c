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

static int
_twin_cur_subpath_len (twin_path_t *path)
{
    int	start;
    
    if (path->nsublen)
	start = path->sublen[path->nsublen-1];
    else
	start = 0;
    return path->npoints - start;
}

void
twin_path_cur_point (twin_path_t *path, twin_fixed_t *x, twin_fixed_t *y)
{
    if (!path->npoints)
	twin_path_move (path, 0, 0);
    *x = path->points[path->npoints - 1].x;
    *y = path->points[path->npoints - 1].y;
}

void 
twin_path_move (twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    switch (_twin_cur_subpath_len (path)) {
    default:
	twin_path_close (path);
    case 0:
	twin_path_draw (path, x, y);
	break;
    case 1:
	path->points[path->npoints-1].x = x;
	path->points[path->npoints-1].y = y;
	break;
    }
}

void
twin_path_draw (twin_path_t *path, twin_fixed_t x, twin_fixed_t y)
{
    if (_twin_cur_subpath_len(path) > 0 &&
	path->points[path->npoints-1].x == x &&
	path->points[path->npoints-1].y == y)
	return;
    if (path->npoints == path->size_points)
    {
	int		size_points;
	twin_point_t	*points;
	
	if (path->size_points > 0)
	    size_points = path->size_points * 2;
	else
	    size_points = 16;
	if (path->points)
	    points = realloc (path->points, size_points * sizeof (twin_point_t));
	else
	    points = malloc (size_points * sizeof (twin_point_t));
	if (!points)
	    return;
	path->points = points;
	path->size_points = size_points;
    }
    path->points[path->npoints].x = x;
    path->points[path->npoints].y = y;
    path->npoints++;
}

void
twin_path_close (twin_path_t *path)
{
    switch (_twin_cur_subpath_len(path)) {
    case 1:
	path->npoints--;
    case 0:
	return;
    }
    
    if (path->nsublen == path->size_sublen)
    {
	int	size_sublen;
	int	*sublen;
	
	if (path->size_sublen > 0)
	    size_sublen = path->size_sublen * 2;
	else
	    size_sublen = 16;
	if (path->sublen)
	    sublen = realloc (path->sublen, size_sublen * sizeof (int));
	else
	    sublen = malloc (size_sublen * sizeof (int));
	if (!sublen)
	    return;
	path->sublen = sublen;
	path->size_sublen = size_sublen;
    }
    path->sublen[path->nsublen] = path->npoints;
    path->nsublen++;
}

static const twin_fixed_t _sin_table[] = {
    0x0000, /*  0 */
    0x0192, /*  1 */
    0x0324, /*  2 */
    0x04b5, /*  3 */
    0x0646, /*  4 */
    0x07d6, /*  5 */
    0x0964, /*  6 */
    0x0af1, /*  7 */
    0x0c7c, /*  8 */
    0x0e06, /*  9 */
    0x0f8d, /* 10 */
    0x1112, /* 11 */
    0x1294, /* 12 */
    0x1413, /* 13 */
    0x1590, /* 14 */
    0x1709, /* 15 */
    0x187e, /* 16 */
    0x19ef, /* 17 */
    0x1b5d, /* 18 */
    0x1cc6, /* 19 */
    0x1e2b, /* 20 */
    0x1f8c, /* 21 */
    0x20e7, /* 22 */
    0x223d, /* 23 */
    0x238e, /* 24 */
    0x24da, /* 25 */
    0x2620, /* 26 */
    0x2760, /* 27 */
    0x289a, /* 28 */
    0x29ce, /* 29 */
    0x2afb, /* 30 */
    0x2c21, /* 31 */
    0x2d41, /* 32 */
    0x2e5a, /* 33 */
    0x2f6c, /* 34 */
    0x3076, /* 35 */
    0x3179, /* 36 */
    0x3274, /* 37 */
    0x3368, /* 38 */
    0x3453, /* 39 */
    0x3537, /* 40 */
    0x3612, /* 41 */
    0x36e5, /* 42 */
    0x37b0, /* 43 */
    0x3871, /* 44 */
    0x392b, /* 45 */
    0x39db, /* 46 */
    0x3a82, /* 47 */
    0x3b21, /* 48 */
    0x3bb6, /* 49 */
    0x3c42, /* 50 */
    0x3cc5, /* 51 */
    0x3d3f, /* 52 */
    0x3daf, /* 53 */
    0x3e15, /* 54 */
    0x3e72, /* 55 */
    0x3ec5, /* 56 */
    0x3f0f, /* 57 */
    0x3f4f, /* 58 */
    0x3f85, /* 59 */
    0x3fb1, /* 60 */
    0x3fd4, /* 61 */
    0x3fec, /* 62 */
    0x3ffb, /* 63 */
    0x4000, /* 64 */
};

static twin_fixed_t
_sin (int i, int n) 
{
    int	e = i << (6 - n);
    twin_fixed_t    v;
    
    e &= 0xff;
    if (e & 0x40)
	v = _sin_table[0x40 - (e & 0x3f)];
    else
	v = _sin_table[e & 0x3f];
    if (e & 0x80)
	v = -v;
    return v;
}

static twin_fixed_t
_cos (int i, int n)
{
    return _sin (i + (0x40 >> (6 - n)), n);
}

void
twin_path_circle (twin_path_t *path, twin_fixed_t radius)
{
    int		    sides = (4 * radius) / TWIN_FIXED_TOLERANCE;
    int		    n;
    twin_fixed_t    cx, cy;
    twin_dfixed_t   dradius = (twin_dfixed_t) radius;
    int		    i;

    if (sides > 256) sides = 256;
    n = 2;
    while ((1 << n) < sides)
	n++;

    if (!path->npoints)
    {
	cx = 0;
	cy = 0;
    }
    else
    {
	cx = path->points[path->npoints - 1].x;
	cy = path->points[path->npoints - 1].y;
    }

    twin_path_move (path, cx + radius, cy);

    for (i = 1; i < (1 << n); i++)
    {
	twin_fixed_t	x = (dradius * _cos (i, n - 2) + (1 << 13)) >> 14;
	twin_fixed_t	y = (dradius * _sin (i, n - 2) + (1 << 13)) >> 14;

	twin_path_draw (path, cx + x, cy + y);
    }
    twin_path_close (path);
}

void
twin_path_fill (twin_pixmap_t *pixmap, twin_path_t *path)
{
    twin_edge_t	*edges;
    int		nedges, n;
    int		nalloc;
    int		s;
    int		p;

    nalloc = path->npoints + path->nsublen + 1;
    edges = malloc (sizeof (twin_edge_t) * nalloc);
    p = 0;
    nedges = 0;
    for (s = 0; s <= path->nsublen; s++)
    {
	int sublen;
	int npoints;
	
	if (s == path->nsublen)
	    sublen = path->npoints;
	else
	    sublen = path->sublen[s];
	npoints = sublen - p;
	if (npoints > 1)
	{
	    n = _twin_edge_build (path->points + p, npoints, edges + nedges);
	    p = sublen;
	    nedges += n;
	}
    }
    _twin_edge_fill (pixmap, edges, nedges);
    free (edges);
}

void
twin_path_empty (twin_path_t *path)
{
    path->npoints = 0;
    path->nsublen = 0;
}

void
twin_path_append (twin_path_t *dst, twin_path_t *src)
{
    int	    p;
    int	    s = 0;

    for (p = 0; p < src->npoints; p++)
    {
	if (s < src->nsublen && p == src->sublen[s])
	{
	    twin_path_close (dst);
	    s++;
	}
	twin_path_draw (dst, src->points[p].x, src->points[p].y);
    }
}

twin_path_t *
twin_path_create (void)
{
    twin_path_t	*path;

    path = malloc (sizeof (twin_path_t));
    path->npoints = path->size_points = 0;
    path->nsublen = path->size_sublen = 0;
    path->points = 0;
    path->sublen = 0;
    return path;
}

void
twin_path_destroy (twin_path_t *path)
{
    if (path->points)
	free (path->points);
    if (path->sublen)
	free (path->sublen);
    free (path);
}
