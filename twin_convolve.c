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
 * Find the point in path which is left of the line and
 * closest to a line normal to p1-p2 passing through p1
 */
static int
_twin_path_leftpoint (twin_path_t  *path,
		      twin_point_t *p1,
		      twin_point_t *p2)
{
    int		    p;
    int		    best = 0;
    /*
     * Along the path
     */
    twin_dfixed_t   Ap = p2->y - p1->y;
    twin_dfixed_t   Bp = p1->x - p2->x;
    
    /*
     * Normal to the path
     */
    
    twin_fixed_t    xn = (p1->x - (p2->y - p1->y));
    twin_fixed_t    yn = (p1->y + (p2->x - p1->x));
    twin_dfixed_t   An = yn - p1->y;
    twin_dfixed_t   Bn = p1->x - xn;
    
    twin_dfixed_t   min = 0x7fffffff;
    
    for (p = 0; p < path->npoints; p++)
    {
	twin_fixed_t	x = path->points[p].x;
	twin_fixed_t	y = path->points[p].y;
	twin_dfixed_t	vp = Ap * x + Bp * y;
	twin_dfixed_t	vn = An * x + Bn * y;

	if (vn < 0) vn = -vn;
	if (vp > 0 && vn < min)
	{
	    min = vn;
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
    int n = p;
    for (;;)
    {
	n += inc;
	if (n < 0) n += path->npoints;
	else if (n >= path->npoints) n -= path->npoints;
	if (path->points[n].x != path->points[p].x || 
	    path->points[n].y != path->points[p].y)
	    return n;
    }
    return 0;
}

static int
_around_order (twin_point_t    *a1,
	       twin_point_t    *a2,
	       twin_point_t    *b1,
	       twin_point_t    *b2)
{
    twin_dfixed_t   adx = (a2->x - a1->x);
    twin_dfixed_t   ady = (a2->y - a1->y);
    twin_dfixed_t   bdx = (b2->x - b1->x);
    twin_dfixed_t   bdy = (b2->y - b1->y);
    twin_dfixed_t   diff = (ady * bdx - bdy * adx);

    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

#if 0
#include <stdio.h>
#include <math.h>
#define F(x)	twin_fixed_to_double(x)
#define DBGOUT(x...)	printf(x)

#if 0
static double
_angle (twin_point_t *a, twin_point_t *b)
{
    twin_fixed_t    dx = b->x - a->x;
    twin_fixed_t    dy = b->y - a->y;
    double	    rad;

    rad = atan2 ((double) dy, (double) dx);
    return rad * 180 / M_PI;
}
#endif
#else
#define DBGOUT(x...)
#endif

static void
_twin_subpath_convolve (twin_path_t	*path,
			twin_path_t	*stroke,
			twin_path_t	*pen)
{
    twin_point_t    *sp   = stroke->points;
    twin_point_t    *pp   = pen->points;
    int		    ns    = stroke->npoints;
    int		    np    = pen->npoints;
    twin_point_t    *sp0  = &sp[0];
    twin_point_t    *sp1  = &sp[_twin_path_step(stroke,0,1)];
    int		    start = _twin_path_leftpoint (pen, sp0, sp1);
    twin_point_t    *spn1 = &sp[ns-1];
    twin_point_t    *spn2 = &sp[_twin_path_step(stroke,ns-1,-1)];
    int		    ret   = _twin_path_leftpoint (pen, spn1, spn2);
    int		    p;
    int		    s;
    int		    starget;
    int		    ptarget;
    int		    inc;
    twin_bool_t	    closed = TWIN_FALSE;

    if (sp[0].x == sp[ns - 1].x && sp[0].y == sp[ns - 1].y)
	closed = TWIN_TRUE;

    DBGOUT ("convolve: closed(%s)\n", closed ? "true" : "false");
    DBGOUT ("stroke:\n");
    for (s = 0; s < ns; s++)
	DBGOUT ("\ts%02d: %9.4f, %9.4f\n", s, F(sp[s].x), F(sp[s].y));
    DBGOUT ("pen:\n");
    for (p = 0; p < np; p++)
	DBGOUT ("\tp%02d: %9.4f, %9.4f\n", p, F(pp[p].x), F(pp[p].y));
    
    s = 0;
    p = start;
    DBGOUT ("start:  ");
    DBGOUT ("s%02d (%9.4f, %9.4f), p%02d (%9.4f, %9.4f): %9.4f, %9.4f\n",
	    s, F(sp[s].x), F(sp[s].y),
	    p, F(pp[p].x), F(pp[p].y),
	    F(sp[s].x + pp[p].x), F(sp[s].y + pp[p].y));
    twin_path_move (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
    
    /* step along the path first */
    inc = 1;
    starget = ns - 1;
    if (closed)
	ptarget = start;
    else
	ptarget = ret;
    for (;;)
    {
	/*
	 * Convolve the edges
	 */
	while (s != starget)
	{
	    int	sn = _twin_path_step(stroke,s,inc);
	    int	pn = _twin_path_step(pen,p,1);
	    int	pm = _twin_path_step(pen,p,-1);
    
	    /*
	     * step along pen (forwards or backwards) or stroke as appropriate
	     */
	     
#if 0
	    DBGOUT ("\tangles: stroke %9.4f +pen %9.4f -pen %9.4f\n",
		    _angle (&sp[s], &sp[sn]),
		    _angle (&pp[p], &pp[pn]),
		    _angle (&pp[pm], &pp[p]));
#endif
	    if (_around_order (&sp[s],&sp[sn],&pp[p],&pp[pn]) > 0)
	    {
		DBGOUT ("+pen:   ");
		p = pn;
	    }
	    else if (_around_order (&sp[s],&sp[sn],&pp[pm],&pp[p]) < 0)
	    {
		DBGOUT ("-pen:   ");
		p = pm;
	    }
	    else
	    {
		DBGOUT ("stroke: ");
		s = sn;
	    }
	    DBGOUT ("s%02d (%9.4f, %9.4f), p%02d (%9.4f, %9.4f): %9.4f, %9.4f\n",
		    s, F(sp[s].x), F(sp[s].y),
		    p, F(pp[p].x), F(pp[p].y),
		    F(sp[s].x + pp[p].x), F(sp[s].y + pp[p].y));
	    twin_path_draw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	}
	
	/* draw a cap */
	while (p != ptarget)
	{
	    if (++p == np) p = 0;
	    DBGOUT("cap:    ");
	    DBGOUT ("s%02d (%9.4f, %9.4f), p%02d (%9.4f, %9.4f): %9.4f, %9.4f\n",
		    s, F(sp[s].x), F(sp[s].y),
		    p, F(pp[p].x), F(pp[p].y),
		    F(sp[s].x + pp[p].x), F(sp[s].y + pp[p].y));
	    twin_path_draw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	}
	/*
	 * Finish this edge
	 */
	if (closed)
	    twin_path_close (path);
	
	if (inc == -1)
	    break;
	
	/* reach the end of the path?  Go back the other way now */
	inc = -1;
	starget = 0;
	ptarget = start;
	
	if (closed)
	{
	    p = ret;
	    ptarget = ret;
	    twin_path_move (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	}
	else
	    ptarget = start;
    }
}

void
twin_path_convolve (twin_path_t	*path,
		    twin_path_t	*stroke,
		    twin_path_t	*pen)
{
    int		p;
    int		s;

    p = 0;
    for (s = 0; s <= stroke->nsublen; s++)
    {
	int sublen;
	int npoints;

	if (s == stroke->nsublen)
	    sublen = stroke->npoints;
	else
	    sublen = stroke->sublen[s];
	npoints = sublen - p;
	if (npoints > 1)
	{
	    twin_path_t	subpath;

	    subpath.points = stroke->points + p;
	    subpath.npoints = npoints;
	    subpath.sublen = 0;
	    subpath.nsublen = 0;
	    _twin_subpath_convolve (path, &subpath, pen);
	    p = sublen;
	}
    }
}
