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
	    
typedef struct _twin_edge {
    struct _twin_edge	*next;
    twin_sfixed_t	top, bot;
    twin_sfixed_t	x;
    twin_sfixed_t	e;
    twin_sfixed_t	dx, dy;
    twin_sfixed_t	inc_x;
    twin_sfixed_t	step_x;
    int			winding;
} twin_edge_t;

#define TWIN_POLY_SHIFT	    2
#define TWIN_POLY_FIXED_SHIFT	(4 - TWIN_POLY_SHIFT)
#define TWIN_POLY_SAMPLE    (1 << TWIN_POLY_SHIFT)
#define TWIN_POLY_MASK	    (TWIN_POLY_SAMPLE - 1)
#define TWIN_POLY_STEP	    (TWIN_SFIXED_ONE >> TWIN_POLY_SHIFT)
#define TWIN_POLY_START	    (TWIN_POLY_STEP >> 1)
#define TWIN_POLY_CEIL(c)   (((c) + (TWIN_POLY_STEP-1)) & ~(TWIN_POLY_STEP-1))
#define TWIN_POLY_COL(x)    (((x) >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK)

static int
_edge_compare_y (const void *a, const void *b)
{
    const twin_edge_t	*ae = a;
    const twin_edge_t	*be = b;

    return (int) (ae->top - be->top);
}

static void
_edge_step_by (twin_edge_t  *edge, twin_sfixed_t dy)
{
    twin_dfixed_t   e;
    
    e = edge->e + (twin_dfixed_t) dy * edge->dx;
    edge->x += edge->step_x * dy + edge->inc_x * (e / edge->dy);
    edge->e = e % edge->dy;
}

/*
 * Returns the nearest grid coordinate no less than f
 *
 * Grid coordinates are at TWIN_POLY_STEP/2 + n*TWIN_POLY_STEP
 */

static twin_sfixed_t
_twin_sfixed_grid_ceil (twin_sfixed_t f)
{
    return ((f + (TWIN_POLY_START - 1)) & ~(TWIN_POLY_STEP - 1)) + TWIN_POLY_START;
}

#if 0
#include <stdio.h>
#define F(x)	twin_sfixed_to_double(x)
#define DBGOUT(x...)	printf(x)
#else
#define DBGOUT(x...)
#endif

static int
_twin_edge_build (twin_spoint_t *vertices, int nvertices, twin_edge_t *edges,
		  twin_sfixed_t dx, twin_sfixed_t dy)
{
    int		    v, nv;
    int		    tv, bv;
    int		    e;
    twin_sfixed_t   y;

    e = 0;
    for (v = 0; v < nvertices; v++)
    {
	nv = v + 1;
	if (nv == nvertices) nv = 0;
	
	/* skip horizontal edges */
	if (vertices[v].y == vertices[nv].y)
	    continue;

	DBGOUT ("Vertex: %9.4f, %9.4f\n", F(vertices[v].x), F(vertices[v].y));

	/* figure winding */
	if (vertices[v].y < vertices[nv].y)
	{
	    edges[e].winding = 1;
	    tv = v;
	    bv = nv;
	}
	else
	{
	    edges[e].winding = -1;
	    tv = nv;
	    bv = v;
	}

	/* snap top to first grid point in pixmap */
	y = _twin_sfixed_grid_ceil (vertices[tv].y + dy);
	if (y < TWIN_POLY_START)
	    y = TWIN_POLY_START;
	
	/* skip vertices which don't span a sample row */
	if (y >= vertices[bv].y + dy)
	    continue;

	/* Compute bresenham terms */
	edges[e].dx = vertices[bv].x - vertices[tv].x;
	edges[e].dy = vertices[bv].y - vertices[tv].y;
	if (edges[e].dx >= 0)
	    edges[e].inc_x = 1;
	else
	{
	    edges[e].inc_x = -1;
	    edges[e].dx = -edges[e].dx;
	}
	edges[e].step_x = edges[e].inc_x * (edges[e].dx / edges[e].dy);
	edges[e].dx = edges[e].dx % edges[e].dy;

	edges[e].top = vertices[tv].y + dy;
	edges[e].bot = vertices[bv].y + dy;

	edges[e].x = vertices[tv].x + dx;
	edges[e].e = 0;

	/* step to first grid point */
	_edge_step_by (&edges[e], y - edges[e].top);

	edges[e].top = y;
	e++;
    }
    return e;
}
    
static void
_span_fill (twin_pixmap_t   *pixmap,
	    twin_sfixed_t    y,
	    twin_sfixed_t    left,
	    twin_sfixed_t    right)
{
#if TWIN_POLY_SHIFT == 0
    /* 1x1 */
    static const twin_a8_t	coverage[1][1] = {
	{ 0xff },
    };
#endif
#if TWIN_POLY_SHIFT == 1
    /* 2x2 */
    static const twin_a8_t	coverage[2][2] = {
	{ 0x40, 0x40 },
	{ 0x3f, 0x40 },
    };
#endif
#if TWIN_POLY_SHIFT == 2
    /* 4x4 */
    static const twin_a8_t	coverage[4][4] = {
	{ 0x10, 0x10, 0x10, 0x10 },
	{ 0x10, 0x10, 0x10, 0x10 },
	{ 0x0f, 0x10, 0x10, 0x10 },
	{ 0x10, 0x10, 0x10, 0x10 },
    };
#endif
#if TWIN_POLY_SHIFT == 3
    /* 8x8 */
    static const twin_a8_t	coverage[8][8] = {
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 3, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
	{ 4, 4, 4, 4, 4, 4, 4, 4 },
    };
#endif
    const twin_a8_t *cover = coverage[(y >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK];
    int		    row = twin_sfixed_trunc (y);
    twin_a8_t	    *span = pixmap->p.a8 + row * pixmap->stride;
    twin_a8_t	    *s;
    twin_sfixed_t    x;
    twin_a16_t	    a;
    twin_a16_t	    w;
    int		    col;
    
    /* clip to pixmap */
    if (left < 0)
	left = 0;
    
    if (right > twin_int_to_sfixed (pixmap->width))
	right = twin_int_to_sfixed (pixmap->width);

    /* convert to sample grid */
    left = _twin_sfixed_grid_ceil (left) >> TWIN_POLY_FIXED_SHIFT;
    right = _twin_sfixed_grid_ceil (right) >> TWIN_POLY_FIXED_SHIFT;
    
    /* check for empty */
    if (right <= left)
	return;

    x = left;
    
    /* starting address */
    s = span + (x >> TWIN_POLY_SHIFT);
    
    /* first pixel */
    if (x & TWIN_POLY_MASK)
    {
	w = 0;
	col = 0;
	while (x < right && (x & TWIN_POLY_MASK))
	{
	    w += cover[col++];
	    x++;
	}
	a = *s + w;
	*s++ = twin_sat (a);
    }

    w = 0;
    for (col = 0; col < TWIN_POLY_SAMPLE; col++)
	w += cover[col];

    /* middle pixels */
    while (x + TWIN_POLY_MASK < right)
    {
	a = *s + w;
	*s++ = twin_sat (a);
	x += TWIN_POLY_SAMPLE;
    }
    
    /* last pixel */
    if (right & TWIN_POLY_MASK)
    {
	w = 0;
	col = 0;
	while (x < right)
	{
	    w += cover[col++];
	    x++;
	}
	a = *s + w;
	*s = twin_sat (a);
    }
}

static void
_twin_edge_fill (twin_pixmap_t *pixmap, twin_edge_t *edges, int nedges)
{
    twin_edge_t	    *active, *a, *n, **prev;
    int		    e;
    twin_sfixed_t    y;
    twin_sfixed_t    x0 = 0;
    int		    w;
    
    qsort (edges, nedges, sizeof (twin_edge_t), _edge_compare_y);
    e = 0;
    y = edges[0].top;
    active = 0;
    for (;;)
    {
	/* add in new edges */
	for (;e < nedges && edges[e].top <= y; e++)
	{
	    for (prev = &active; (a = *prev); prev = &(a->next))
		if (a->x > edges[e].x)
		    break;
	    edges[e].next = *prev;
	    *prev = &edges[e];
	}
	
	DBGOUT ("Y %9.4f:", F(y));
	/* walk this y value marking coverage */
	w = 0;
	for (a = active; a; a = a->next)
	{
	    DBGOUT (" %9.4f(%d)", F(a->x), a->winding);
	    if (w == 0)
		x0 = a->x;
	    w += a->winding;
	    if (w == 0)
	    {
		DBGOUT (" F ");
		_span_fill (pixmap, y, x0, a->x);
	    }
	}
	DBGOUT ("\n");
	
	/* step down, clipping to pixmap */
	y += TWIN_POLY_STEP;

	if (twin_sfixed_trunc (y) >= pixmap->height)
	    break;
	
	/* strip out dead edges */
	for (prev = &active; (a = *prev);)
	{
	    if (a->bot <= y)
		*prev = a->next;
	    else
		prev = &a->next;
	}

	/* check for all done */
	if (!active && e == nedges)
	    break;
	
	/* step all edges */
	for (a = active; a; a = a->next)
	    _edge_step_by (a, TWIN_POLY_STEP);
	
	/* fix x sorting */
	for (prev = &active; (a = *prev) && (n = a->next);)
	{
	    if (a->x > n->x)
	    {
		a->next = n->next;
		n->next = a;
		*prev = n;
		prev = &active;
	    }
	    else
		prev = &a->next;
	}
    }
}

void
twin_fill_path (twin_pixmap_t *pixmap, twin_path_t *path, int dx, int dy)
{
    twin_edge_t	    *edges;
    int		    nedges, n;
    int		    nalloc;
    int		    s;
    int		    p;
    twin_sfixed_t   sdx = twin_int_to_sfixed (dx);
    twin_sfixed_t   sdy = twin_int_to_sfixed (dy);

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
	    n = _twin_edge_build (path->points + p, npoints, edges + nedges,
				  sdx, sdy);
	    p = sublen;
	    nedges += n;
	}
    }
    _twin_edge_fill (pixmap, edges, nedges);
    free (edges);
}

