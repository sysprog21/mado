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

	y = vertices[tv].y;
	if (y < 0)
	    y = 0;
	y = (y + 0x7) & ~0x7;
	
	/* skip vertices which don't span a sample row */
	if (y >= vertices[bv].y)
	    continue;

	/* Compute bresenham terms for 2x2 oversampling 
	 * which is 8 sub-pixel steps per 
	 */

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

	edges[e].top = y;
	edges[e].bot = vertices[bv].y;

	edges[e].x = vertices[tv].x;
	edges[e].e = 0;

	_edge_step_by (&edges[e], y - edges[e].top);

	edges[e].top = y;
	e++;
    }
    qsort (edges, e, sizeof (twin_edge_t), _edge_compare_y);
    return e;
}
    
#define twin_fixed_ceil_half(f)	(((f) + 7) & ~7)
#define TWIN_FIXED_HALF	(TWIN_FIXED_ONE >> 1)

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
    
    for (x = twin_fixed_ceil_half (left); x < right; x += TWIN_FIXED_HALF)
    {
	a = (twin_a16_t) cover[(x >> 3) & 1] + (twin_a16_t) span[twin_fixed_trunc(x)];
	span[twin_fixed_trunc(x)] = twin_sat(a);
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
	/* strip out dead edges */
	for (prev = &active; (a = *prev); prev = &(a->next))
	    if (a->bot <= y)
		*prev = a->next;
	/* add in new edges */
	for (;e < nedges && edges[e].top <= y; e++)
	{
	    for (prev = &active; (a = *prev); prev = &(a->next))
		if (a->x > edges[e].x)
		    break;
	    edges[e].next = *prev;
	    *prev = &edges[e];
	}
	if (!active)
	    break;
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
	y += TWIN_FIXED_ONE >> 1;
	/* step all edges */
	for (a = active; a; a = a->next)
	    _edge_step_by (a, TWIN_FIXED_ONE >> 1);
	
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
