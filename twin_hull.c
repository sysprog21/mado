/*
 * $Id$
 *
 * Copyright © 2003 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carl Worth not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Carl Worth makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * CARL WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "twinint.h"

#if 0
#include <stdio.h>
#include <math.h>
#define S(x)	twin_sfixed_to_double(x)
#define DBGMSG(x)	printf x
#else
#define DBGMSG(x)
#endif

typedef struct twin_slope {
    twin_sfixed_t dx;
    twin_sfixed_t dy;
} twin_slope_t, twin_distance_t;

typedef struct _twin_hull {
    twin_spoint_t point;
    twin_slope_t slope;
    int discard;
} twin_hull_t;

static void
_twin_slope_init (twin_slope_t *slope, twin_spoint_t *a, twin_spoint_t *b)
{
    slope->dx = b->x - a->x;
    slope->dy = b->y - a->y;
}

static twin_hull_t *
_twin_hull_create (twin_path_t *path, int *nhull)
{
    int		    i, j;
    int		    n = path->npoints;
    twin_spoint_t    *p = path->points;
    twin_hull_t	    *hull;
    int		    e;
    
    e = 0;
    for (i = 1; i < n; i++)
	if (p[i].y < p[e].y || (p[i].y == p[e].y && p[i].x < p[e].x))
	    e = i;
    
    hull = malloc (n * sizeof (twin_hull_t));
    if (hull == NULL)
	return NULL;
    *nhull = n;

    DBGMSG (("original polygon: \n"));
    for (i = 0; i < n; i++) 
    {
	DBGMSG (("\t%d: %9.4f, %9.4f\n", i, S(p[i].x), S(p[i].y)));
	/* place extremum first in array */
	if (i == 0) j = e;
	else if (i == e) j = 0;
	else j = i;
	
	hull[i].point = p[j];
	_twin_slope_init (&hull[i].slope, &hull[0].point, &hull[i].point);

	/* Discard all points coincident with the extremal point */
	if (i != 0 && hull[i].slope.dx == 0 && hull[i].slope.dy == 0)
	    hull[i].discard = 1;
	else
	    hull[i].discard = 0;
    }

    return hull;
}

/* Compare two slopes. Slope angles begin at 0 in the direction of the
   positive X axis and increase in the direction of the positive Y
   axis.

   WARNING: This function only gives correct results if the angular
   difference between a and b is less than PI.

   <  0 => a less positive than b
   == 0 => a equal to be
   >  0 => a more positive than b
*/
static int
_twin_slope_compare (twin_slope_t *a, twin_slope_t *b)
{
    twin_dfixed_t diff;

    diff = ((twin_dfixed_t) a->dy * (twin_dfixed_t) b->dx -
	    (twin_dfixed_t) b->dy * (twin_dfixed_t) a->dx);

    if (diff > 0)
	return 1;
    if (diff < 0)
	return -1;

    if (a->dx == 0 && a->dy == 0)
	return 1;
    if (b->dx == 0 && b->dy ==0)
	return -1;

    return 0;
}

static int
_twin_hull_vertex_compare (const void *av, const void *bv)
{
    twin_hull_t *a = (twin_hull_t *) av;
    twin_hull_t *b = (twin_hull_t *) bv;
    int ret;

    ret = _twin_slope_compare (&a->slope, &b->slope);

    /* In the case of two vertices with identical slope from the
       extremal point discard the nearer point. */

    if (ret == 0) 
    {
	twin_dfixed_t a_dist, b_dist;
	a_dist = ((twin_dfixed_t) a->slope.dx * a->slope.dx +
		  (twin_dfixed_t) a->slope.dy * a->slope.dy);
	b_dist = ((twin_dfixed_t) b->slope.dx * b->slope.dx +
		  (twin_dfixed_t) b->slope.dy * b->slope.dy);
	if (a_dist < b_dist)
	{
	    a->discard = 1;
	    ret = -1;
	}
	else
	{
	    b->discard = 1;
	    ret = 1;
	}
    }

    return ret;
}

static int
_twin_hull_prev_valid (twin_hull_t *hull, int num_hull, int index)
{
    do {
	/* hull[0] is always valid, so don't test and wraparound */
	index--;
    } while (hull[index].discard);

    return index;
}

static int
_twin_hull_next_valid (twin_hull_t *hull, int num_hull, int index)
{
    do {
	index = (index + 1) % num_hull;
    } while (hull[index].discard);

    return index;
}

/*
 * Graham scan to compute convex hull
 */

static void
_twin_hull_eliminate_concave (twin_hull_t *hull, int num_hull)
{
    int i, j, k;
    twin_slope_t slope_ij, slope_jk;

    i = 0;
    j = _twin_hull_next_valid (hull, num_hull, i);
    k = _twin_hull_next_valid (hull, num_hull, j);

    do {
	DBGMSG (("i: %d j: %d k: %d\n", i, j, k));
	_twin_slope_init (&slope_ij, &hull[i].point, &hull[j].point);
	_twin_slope_init (&slope_jk, &hull[j].point, &hull[k].point);

	/* Is the angle formed by ij and jk concave? */
	if (_twin_slope_compare (&slope_ij, &slope_jk) >= 0) {
	    if (i == k)
		break;
	    hull[j].discard = 1;
	    j = i;
	    i = _twin_hull_prev_valid (hull, num_hull, j);
	} else {
	    i = j;
	    j = k;
	    k = _twin_hull_next_valid (hull, num_hull, j);
	}
    } while (j != 0);
}

/*
 * Convert the hull structure back to a simple path
 */
static twin_path_t *
_twin_hull_to_path (twin_hull_t *hull, int num_hull)
{
    twin_path_t	*path = twin_path_create ();
    int		i;

    DBGMSG (("convex hull\n"));
    for (i = 0; i < num_hull; i++) 
    {
	DBGMSG (("\t%d: %9.4f, %9.4f %c\n",
		 i, S(hull[i].point.x), S(hull[i].point.y), 
		 hull[i].discard ? '*' : ' '));
	if (hull[i].discard)
	    continue;
	_twin_path_sdraw (path, hull[i].point.x, hull[i].point.y);
    }

    return path;
}

/*
 * Given a path, return the convex hull using the Graham scan algorithm. 
 */

twin_path_t *
twin_path_convex_hull (twin_path_t *path)
{
    twin_hull_t *hull;
    int		num_hull;
    twin_path_t	*convex_path;

    hull = _twin_hull_create (path, &num_hull);
    
    if (hull == NULL)
	return 0;

    qsort (hull + 1, num_hull - 1, sizeof (twin_hull_t),
	   _twin_hull_vertex_compare);

    _twin_hull_eliminate_concave (hull, num_hull);

    convex_path = _twin_hull_to_path (hull, num_hull);

    free (hull);

    return convex_path;
}
