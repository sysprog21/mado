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
_edge_compare_y (const void *a, const void *b)
{
    const twin_edge_t	*ae = a;
    const twin_edge_t	*be = b;

    return (int) (ae->top - be->top);
}

static void
_edge_step_by (twin_edge_t  *edge, twin_fixed_t dy)
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

static twin_fixed_t
_twin_fixed_grid_ceil (twin_fixed_t f)
{
    return ((f + (TWIN_POLY_START - 1)) & ~(TWIN_POLY_STEP - 1)) + TWIN_POLY_START;
}

int
_twin_edge_build (twin_point_t *vertices, int nvertices, twin_edge_t *edges)
{
    int		    v, nv;
    int		    tv, bv;
    int		    e;
    twin_fixed_t    y;

    e = 0;
    for (v = 0; v < nvertices; v++)
    {
	nv = v + 1;
	if (nv == nvertices) nv = 0;
	
	/* skip horizontal edges */
	if (vertices[v].y == vertices[nv].y)
	    continue;

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
	y = _twin_fixed_grid_ceil (vertices[tv].y);
	if (y < TWIN_POLY_START)
	    y = TWIN_POLY_START;
	
	/* skip vertices which don't span a sample row */
	if (y >= vertices[bv].y)
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

	edges[e].top = vertices[tv].y;
	edges[e].bot = vertices[bv].y;

	edges[e].x = vertices[tv].x;
	edges[e].e = 0;

	/* step to first grid point */
	_edge_step_by (&edges[e], y - edges[e].top);

	edges[e].top = y;
	e++;
    }
    qsort (edges, e, sizeof (twin_edge_t), _edge_compare_y);
    return e;
}
    
static void
_span_fill (twin_pixmap_t   *pixmap,
	    twin_fixed_t    y,
	    twin_fixed_t    left,
	    twin_fixed_t    right)
{
    /* 2x2 oversampling yields slightly uneven alpha values */
    static const twin_a8_t	coverage[2][2] = {
	{ 0x40, 0x40 },
	{ 0x3f, 0x40 },
    };
    const twin_a8_t *cover = coverage[(y >> 3) & 1];
    int		    row = twin_fixed_trunc (y);
    twin_a8_t	    *span = pixmap->p.a8 + row * pixmap->stride;
    twin_fixed_t    x;
    twin_a16_t	    a;
    twin_a16_t	    w;
    
    /* clip to pixmap */
    if (left < 0)
	left = 0;
    
    if (twin_fixed_trunc (right) > pixmap->width)
	right = twin_int_to_fixed (pixmap->width);

    left = _twin_fixed_grid_ceil (left);
    right = _twin_fixed_grid_ceil (right);
    
    /* check for empty */
    if (right < left)
	return;

    x = left;
    
    /* starting address */
    span += twin_fixed_trunc(x);
    
    /* first pixel */
    if (x & TWIN_FIXED_HALF)
    {
	a = *span + (twin_a16_t) cover[1];
	*span++ = twin_sat (a);
	x += TWIN_FIXED_HALF;
    }

    /* middle pixels */
    w = cover[0] + cover[1];
    while (x < right - TWIN_FIXED_HALF)
    {
	a = *span + w;
	*span++ = twin_sat (a);
	x += TWIN_FIXED_ONE;
    }
    
    /* last pixel */
    if (x < right)
    {
	a = *span + (twin_a16_t) cover[0];
	*span = twin_sat (a);
    }
}

void
_twin_edge_fill (twin_pixmap_t *pixmap, twin_edge_t *edges, int nedges)
{
    twin_edge_t	    *active, *a, *n, **prev;
    int		    e;
    twin_fixed_t    y;
    twin_fixed_t    x0;
    int		    w;
    
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
	
	/* walk this y value marking coverage */
	w = 0;
	for (a = active; a; a = a->next)
	{
	    if (w == 0)
		x0 = a->x;
	    w += a->winding;
	    if (w == 0)
		_span_fill (pixmap, y, x0, a->x);
	}
	
	/* step down, clipping to pixmap */
	y += TWIN_POLY_STEP;

	if (twin_fixed_trunc (y) >= pixmap->height)
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
twin_polygon (twin_pixmap_t *pixmap, twin_point_t *vertices, int nvertices)
{
    twin_edge_t	    *edges;
    int		    nedges;
    
    /*
     * Construct the edge table
     */
    edges = malloc (sizeof (twin_edge_t) * (nvertices + 1));
    if (!edges)
	return;
    nedges = _twin_edge_build (vertices, nvertices, edges);
    _twin_edge_fill (pixmap, edges, nedges);
    free (edges);
}
