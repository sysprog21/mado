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
 * Find the point in path which is furthest left of the line
 */
static int
_twin_path_leftpoint (twin_path_t   *path,
		      twin_spoint_t *p1,
		      twin_spoint_t *p2)
{
    twin_spoint_t   *points = path->points;
    int		    p;
    int		    best = 0;
    /*
     * Normal form of the line is Ax + By + C = 0,
     * these are the A and B factors.  As we're just comparing
     * across x and y, the value of C isn't relevant
     */
    twin_dfixed_t   Ap = p2->y - p1->y;
    twin_dfixed_t   Bp = p1->x - p2->x;
    
    twin_dfixed_t   max = -0x7fffffff;
    
    for (p = 0; p < path->npoints; p++)
    {
	twin_dfixed_t	vp = Ap * points[p].x + Bp * points[p].y;

	if (vp > max)
	{
	    max = vp;
	    best = p;
	}
    }
    return best;
}

static int
_around_order (twin_spoint_t    *a1,
	       twin_spoint_t    *a2,
	       twin_spoint_t    *b1,
	       twin_spoint_t    *b2)
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
#define F(x)	twin_sfixed_to_double(x)
#define DBGOUT(x...)	printf(x)

static double
_angle (twin_spoint_t *a, twin_spoint_t *b)
{
    twin_sfixed_t    dx = b->x - a->x;
    twin_sfixed_t    dy = b->y - a->y;
    double	    rad;

    rad = atan2 ((double) dy, (double) dx);
    return rad * 180 / M_PI;
}
#else
#define DBGOUT(x...)
#endif

/*
 * Convolve one subpath with a convex pen.  The result is
 * a closed path.
 */
static void
_twin_subpath_convolve (twin_path_t	*path,
			twin_path_t	*stroke,
			twin_path_t	*pen)
{
    twin_spoint_t    *sp   = stroke->points;
    twin_spoint_t    *pp   = pen->points;
    int		    ns    = stroke->npoints;
    int		    np    = pen->npoints;
    twin_spoint_t    *sp0  = &sp[0];
    twin_spoint_t    *sp1  = &sp[1];
    int		    start = _twin_path_leftpoint (pen, sp0, sp1);
    twin_spoint_t    *spn1 = &sp[ns-1];
    twin_spoint_t    *spn2 = &sp[ns-2];
    int		    ret   = _twin_path_leftpoint (pen, spn1, spn2);
    int		    p;
    int		    s;
    int		    starget;
    int		    ptarget;
    int		    inc;

    DBGOUT ("convolve stroke:\n");
    for (s = 0; s < ns; s++)
	DBGOUT ("\ts%02d: %9.4f, %9.4f\n", s, F(sp[s].x), F(sp[s].y));
    DBGOUT ("convolve pen:\n");
    for (p = 0; p < np; p++)
	DBGOUT ("\tp%02d: %9.4f, %9.4f\n", p, F(pp[p].x), F(pp[p].y));
    
    s = 0;
    p = start;
    DBGOUT ("start:  ");
    DBGOUT ("s%02d (%9.4f, %9.4f), p%02d (%9.4f, %9.4f): %9.4f, %9.4f\n",
	    s, F(sp[s].x), F(sp[s].y),
	    p, F(pp[p].x), F(pp[p].y),
	    F(sp[s].x + pp[p].x), F(sp[s].y + pp[p].y));
    _twin_path_smove (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
    
    /* step along the path first */
    inc = 1;
    starget = ns-1;
    ptarget = ret;
    for (;;)
    {
	/*
	 * Convolve the edges
	 */
	do
	{
	    int	sn = s + inc;
	    int	pn = (p == np - 1) ? 0 : p + 1;
	    int	pm = (p == 0) ? np - 1 : p - 1;
    
	    /*
	     * step along pen (forwards or backwards) or stroke as appropriate
	     */
	     
	    DBGOUT ("\tangles: stroke %9.4f +pen %9.4f -pen %9.4f\n",
		    _angle (&sp[s], &sp[sn]),
		    _angle (&pp[p], &pp[pn]),
		    _angle (&pp[pm], &pp[p]));
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
	    _twin_path_sdraw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	} while (s != starget);
	
	/*
	 * Finish this edge
	 */
	
	/* draw a cap */
	while (p != ptarget)
	{
	    if (++p == np) p = 0;
	    DBGOUT("cap:    ");
	    DBGOUT ("s%02d (%9.4f, %9.4f), p%02d (%9.4f, %9.4f): %9.4f, %9.4f\n",
		    s, F(sp[s].x), F(sp[s].y),
		    p, F(pp[p].x), F(pp[p].y),
		    F(sp[s].x + pp[p].x), F(sp[s].y + pp[p].y));
	    _twin_path_sdraw (path, sp[s].x + pp[p].x, sp[s].y + pp[p].y);
	}
	
	if (inc == -1)
	    break;
	
	/* reach the end of the path?  Go back the other way now */
	inc = -1;
	ptarget = start;
	starget = 0;
    }
}

void
twin_path_convolve (twin_path_t	*path,
		    twin_path_t	*stroke,
		    twin_path_t	*pen)
{
    int		p;
    int		s;
    twin_path_t	*hull = twin_path_convex_hull (pen);

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
	    _twin_subpath_convolve (path, &subpath, hull);
	    p = sublen;
	}
    }
    twin_path_destroy (hull);
}
