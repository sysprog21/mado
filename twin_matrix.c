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
#define F(f)	(twin_fixed_to_double(f))
#define DBGMSG(x) printf x

static void
_twin_dump_matrix (char *name, const twin_matrix_t *m)
{
    int row, col;
    
    printf ("%-6.6s:", name);
    for (row = 0; row < 3; row++)
    {
	printf ("\t");
	for (col = 0; col < 2; col++)
	    printf ("%9.4f ", twin_fixed_to_double (m->m[row][col]));
	printf ("\n");
    }
}

#else
#define DBGMSG(x)
#define _twin_dump_matrix(n,m)
#endif

static void
_twin_matrix_multiply (twin_matrix_t	    *result,
		       const twin_matrix_t  *a,
		       const twin_matrix_t  *b)
{
    twin_matrix_t   r;
    int		    row, col, n;
    twin_fixed_t    t;

    for (row = 0; row < 3; row++)
	for (col = 0; col < 2; col++) {
	    if (row == 2)
		t = b->m[2][col];
	    else
		t = 0;
	    for (n = 0; n < 2; n++)
		t += twin_fixed_mul(a->m[row][n], b->m[n][col]);
	    r.m[row][col] = t;
	}
    _twin_dump_matrix ("a", a);
    _twin_dump_matrix ("b", b);
    _twin_dump_matrix ("r", &r);
    
    *result = r;
}

void
twin_matrix_identity (twin_matrix_t *m)
{
    m->m[0][0] = TWIN_FIXED_ONE;    m->m[0][1] = 0;
    m->m[1][0] = 0;		    m->m[1][1] = TWIN_FIXED_ONE;
    m->m[2][0] = 0;		    m->m[2][1] = 0;
}

void
twin_matrix_translate (twin_matrix_t *m, twin_fixed_t tx, twin_fixed_t ty)
{
    twin_matrix_t   t;
    
    t.m[0][0] = TWIN_FIXED_ONE;	    t.m[0][1] = 0;
    t.m[1][0] = 0;		    t.m[1][1] = TWIN_FIXED_ONE;
    t.m[2][0] = tx;		    t.m[2][1] = ty;
    _twin_matrix_multiply (m, &t, m);
}

void
twin_matrix_scale (twin_matrix_t *m, twin_fixed_t sx, twin_fixed_t sy)
{
    twin_matrix_t   t;
    
    t.m[0][0] = sx;		    t.m[0][1] = 0;
    t.m[1][0] = 0;		    t.m[1][1] = sy;
    t.m[2][0] = 0;		    t.m[2][1] = 0;
    _twin_matrix_multiply (m, &t, m);
}

twin_fixed_t
_twin_matrix_determinant (twin_matrix_t *matrix)
{
    twin_fixed_t    a, b, c, d;
    twin_fixed_t    det;
    
    a = matrix->m[0][0]; b = matrix->m[0][1];
    c = matrix->m[1][0]; d = matrix->m[1][1];

    det = twin_fixed_mul (a, d) - twin_fixed_mul (b, c);

    return det;
}

twin_point_t
_twin_matrix_expand (twin_matrix_t *matrix)
{
    twin_fixed_t    a = matrix->m[0][0];
    twin_fixed_t    d = matrix->m[1][1];
    twin_fixed_t    aa = twin_fixed_mul (a,a);
    twin_fixed_t    dd = twin_fixed_mul (d,d);
    twin_point_t    expand;

    expand.x = twin_fixed_sqrt (aa + dd);
    expand.y = twin_fixed_div (_twin_matrix_determinant (matrix),
			       expand.x);
    return expand;
}

void
twin_matrix_rotate (twin_matrix_t *m, twin_angle_t a)
{
    twin_matrix_t   t;
    twin_fixed_t    c = twin_cos (a);
    twin_fixed_t    s = twin_sin (a);

    t.m[0][0] = c;		    t.m[0][1] = s;
    t.m[1][0] = -s;		    t.m[1][1] = c;
    t.m[2][0] = 0;		    t.m[2][1] = 0;
    _twin_matrix_multiply (m, &t, m);
}

twin_sfixed_t
_twin_matrix_x (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    twin_sfixed_t   s;
    s = twin_fixed_to_sfixed (twin_fixed_mul (m->m[0][0], x) +
			      twin_fixed_mul (m->m[1][0], y) +
			      m->m[2][0]);
    DBGMSG (("x: %9.4f,%9.4f -> %9.4f\n", 
	     twin_fixed_to_double(x), twin_fixed_to_double(y),
	     twin_sfixed_to_double (s)));
    return s;
}

twin_sfixed_t
_twin_matrix_y (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    twin_sfixed_t   s;
    s = twin_fixed_to_sfixed (twin_fixed_mul (m->m[0][1], x) +
			      twin_fixed_mul (m->m[1][1], y) +
			      m->m[2][1]);
    DBGMSG (("y: %9.4f,%9.4f -> %9.4f\n", 
	     twin_fixed_to_double(x), twin_fixed_to_double(y),
	     twin_sfixed_to_double (s)));
    return s;
}

twin_sfixed_t
_twin_matrix_dx (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_to_sfixed (twin_fixed_mul (m->m[0][0], x) +
				 twin_fixed_mul (m->m[1][0], y));
    
}

twin_sfixed_t
_twin_matrix_dy (twin_matrix_t *m, twin_fixed_t x, twin_fixed_t y)
{
    return twin_fixed_to_sfixed (twin_fixed_mul (m->m[0][1], x) +
				 twin_fixed_mul (m->m[1][1], y));
}

twin_sfixed_t
_twin_matrix_len (twin_matrix_t *m, twin_fixed_t dx, twin_fixed_t dy)
{
    twin_fixed_t    xs = (twin_fixed_mul (m->m[0][0], dx) +
			  twin_fixed_mul (m->m[1][0], dy));
    twin_fixed_t    ys = (twin_fixed_mul (m->m[0][1], dx) +
			  twin_fixed_mul (m->m[1][1], dy));
    twin_fixed_t    ds = (twin_fixed_mul (xs, xs) +
			  twin_fixed_mul (ys, ys));
    return (twin_fixed_to_sfixed (twin_fixed_sqrt (ds)));
}
