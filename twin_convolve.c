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

/*
 * Find the point in path closest to a line normal to p1-p2 which passes through p1
 * (this does not do this yet, but it should)
 */
static int
_twin_path_leftmost (twin_path_t  *path,
		     twin_point_t *p1,
		     twin_point_t *p2)
{
    int		    p;
    int		    best = 0;
    twin_dfixed_t   A = p2->y - p1->y;
    twin_dfixed_t   B = p1->x - p2->x;
    twin_dfixed_t   C = ((twin_dfixed_t) p1->y * p2->x - 
			 (twin_dfixed_t) p1->x * p2->y);
    twin_dfixed_t   max = -0x7fffffff;
    
    for (p = 0; p < path->npoints; p++)
    {
	twin_fixed_t	x = path->points[p].x + p1->x;
	twin_fixed_t	y = path->points[p].y + p1->y;
	twin_dfixed_t	v = A * x + B * y + C;
	if (v > max)
	{
	    max = v;
	    best = p;
	}
    }
    return best;
}

static int
_twin_path_step (twin_path_t	*path,
		 int		p,
		 int		inc)
{
    for (;;)
    {
	int n = p + inc;
	if (n < 0) n += path->npoints;
	else if (n >= path->npoints) n -= path->npoints;
	if (path->points[n].x != path->points[p].x || 
	    path->points[n].y != path->points[p].y)
	    return n;
    }
    return 0;
}

static twin_bool_t
_clockwise (twin_point_t    *a1,
	    twin_point_t    *a2,
	    twin_point_t    *b1,
	    twin_point_t    *b2)
{
    twin_dfixed_t   adx = (a2->x - a1->x);
    twin_dfixed_t   ady = (a2->y - a1->y);
    twin_dfixed_t   bdx = (b2->x - b1->x);
    twin_dfixed_t   bdy = (b2->y - b1->y);
    twin_dfixed_t   diff = (ady * bdx - bdy * adx);

    return diff <= 0;
}

#include <stdio.h>

void
twin_path_convolve (twin_path_t	*path,
		    twin_path_t	*stroke,
		    twin_path_t	*pen)
{
    twin_point_t    *sp = stroke->points;
    twin_point_t    *pp = pen->points;
    int		    ns = stroke->npoints;
    int		    np = pen->npoints;
    int		    start = _twin_path_leftmost (pen,
						 &sp[0],
						 &sp[_twin_path_step(stroke,0,1)]);
    int		    ret = _twin_path_leftmost (pen,
					       &sp[ns-1],
					       &sp[_twin_path_step(stroke,ns-1,-1)]);
    int		    p;
    int		    s;
    int		    starget;
    int		    ptarget;
    int		    inc;
    twin_bool_t	    closed = TWIN_FALSE;

    if (sp[0].x == sp[ns - 1].x && sp[0].y == sp[ns - 1].y)
	closed = TWIN_TRUE;
    
    s = 0;
    p = start;
    twin_path_draw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
    
    /* step along the path first */
    inc = 1;
    starget = ns - 1;
    ptarget = ret;
    for (;;)
    {
	int	sn = _twin_path_step(stroke,s,inc);
	int	pn = _twin_path_step(pen,p,1);
	int	pm = _twin_path_step(pen,p,-1);

	/*
	 * step along pen (forwards or backwards) or stroke as appropriate
	 */
	 
	if (!_clockwise (&sp[s],&sp[sn],&pp[p],&pp[pn]))
	    p = pn;
	else if (_clockwise(&sp[s],&sp[sn],&pp[pm],&pp[p]))
	    p = pm;
	else
	    s = sn;
	
	twin_path_draw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	
	if (s == starget)
	{
	    if (closed)
	    {
		twin_path_close (path);
		if (inc == 1)
		    twin_path_move (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	    }
	    else
	    {
		/* draw a cap */
		while (p != ptarget)
		{
		    if (++p == np) p = 0;
		    twin_path_draw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
		}
	    }
	    if (inc == -1)
		break;
	    /* reach the end of the path?  Go back the other way now */
	    inc = -1;
	    starget = 0;
	    ptarget = start;
	}
    }
}
