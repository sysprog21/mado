/*
 * $Id$
 *
 * Copyright © 2004 Carl Worth
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
 * EVENT SHALL CARL WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "twinint.h"

typedef struct _twin_spline {
    twin_point_t    a, b, c, d;
} twin_spline_t;

static void
_lerp_half (twin_point_t *a, twin_point_t *b, twin_point_t *result)
{
    result->x = a->x + ((b->x - a->x) >> 1);
    result->y = a->y + ((b->y - a->y) >> 1);
}

static void
_de_casteljau (twin_spline_t *spline, twin_spline_t *s1, twin_spline_t *s2)
{
    twin_point_t ab, bc, cd;
    twin_point_t abbc, bccd;
    twin_point_t final;

    _lerp_half (&spline->a, &spline->b, &ab);
    _lerp_half (&spline->b, &spline->c, &bc);
    _lerp_half (&spline->c, &spline->d, &cd);
    _lerp_half (&ab, &bc, &abbc);
    _lerp_half (&bc, &cd, &bccd);
    _lerp_half (&abbc, &bccd, &final);

    s1->a = spline->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = spline->d;
}

/*
 * Return an upper bound on the error (squared) that could
 * result from approximating a spline as a line segment 
 * connecting the two endpoints 
 */

static twin_dfixed_t
_twin_spline_error_squared (twin_spline_t *spline)
{
    twin_dfixed_t berr, cerr;

    berr = _twin_distance_to_line_squared (&spline->b, &spline->a, &spline->d);
    cerr = _twin_distance_to_line_squared (&spline->c, &spline->a, &spline->d);

    if (berr > cerr)
	return berr;
    else
	return cerr;
}

/*
 * Pure recursive spline decomposition.
 */

static void
_twin_spline_decompose (twin_path_t	*path,
			twin_spline_t	*spline, 
			twin_dfixed_t	tolerance_squared)
{
    if (_twin_spline_error_squared (spline) <= tolerance_squared)
    {
	twin_path_draw (path, spline->a.x, spline->a.y);
    }
    else
    {
	twin_spline_t s1, s2;
	_de_casteljau (spline, &s1, &s2);
	_twin_spline_decompose (path, &s1, tolerance_squared);
	_twin_spline_decompose (path, &s2, tolerance_squared);
    }
}

void
twin_path_curve (twin_path_t	*path,
		 twin_fixed_t	x1, twin_fixed_t y1,
		 twin_fixed_t	x2, twin_fixed_t y2,
		 twin_fixed_t	x3, twin_fixed_t y3)
{
    twin_spline_t   spline;

    if (path->npoints == 0)
	twin_path_move (path, 0, 0);
    spline.a = path->points[path->npoints - 1];
    spline.b.x = x1;
    spline.b.y = y1;
    spline.c.x = x2;
    spline.c.y = y2;
    spline.d.x = x3;
    spline.d.y = y3;
    _twin_spline_decompose (path, &spline, TWIN_FIXED_TOLERANCE * TWIN_FIXED_TOLERANCE);
    twin_path_draw (path, x3, y3);
}
