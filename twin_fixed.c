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
#define F(f)	    twin_fixed_to_double(f)
#define DBGMSG(x)   printf x
#else
#define DBGMSG(x)
#endif

#define uint32_lo(i)	((i) & 0xffff)
#define uint32_hi(i)	((i) >> 16)
#define uint32_carry16	((1) << 16)

twin_fixed_t
twin_fixed_mul (twin_fixed_t af, twin_fixed_t bf)
{
    uint32_t	    a;
    uint32_t	    b;
    uint16_t	    ah, al, bh, bl;
    uint32_t	    r0, r1, r2, r3;
    uint32_t	    hi, lo;
    twin_fixed_t    r;
    twin_bool_t	    negative = TWIN_FALSE;

    a = (uint32_t) af;
    b = (uint32_t) bf;
    if (af < 0)
    {
	negative = !negative;
	a = -a;
    }
    if (bf < 0)
    {
	negative = !negative;
	b = -b;
    }

    al = uint32_lo (a);
    ah = uint32_hi (a);
    bl = uint32_lo (b);
    bh = uint32_hi (b);

    r0 = (uint32_t) al * bl;
    r1 = (uint32_t) al * bh;
    r2 = (uint32_t) ah * bl;
    r3 = (uint32_t) ah * bh;

    r1 += uint32_hi(r0);    /* no carry possible */
    r1 += r2;		    /* but this can carry */
    if (r1 < r2)	    /* check */
	r3 += uint32_carry16;

    hi = r3 + uint32_hi(r1);
    lo = (uint32_lo (r1) << 16) + uint32_lo (r0);

    DBGMSG (("0x%x * 0x%x = 0x%x%08x\n", a, b, hi, lo));

    r = (twin_fixed_t) ((hi << 16) | (lo >> 16));
    if (negative)
	r = -r;
    DBGMSG (("%9.4f * %9.4f = %9.4f\n", 
	     twin_fixed_to_double(af), twin_fixed_to_double(bf),
	     twin_fixed_to_double (r)));
    return r;
}

twin_fixed_t
twin_fixed_div (twin_fixed_t a, twin_fixed_t b)
{
    twin_fixed_t q;
    q = (twin_fixed_t) ((((int64_t) a) << 16) / b);
    return q;
}

twin_fixed_t
twin_fixed_sqrt (twin_fixed_t a)
{
    twin_fixed_t    max, min, mid;
    twin_fixed_t    sqr;

    max = a;
    min = 0;
    while (max > min)
    {
	mid = (max + min) >> 1;
	sqr = twin_fixed_mul (mid, mid);
	if (sqr == a)
	    return mid;
	if (sqr < a)
	    min = mid + 1;
	else
	    max = mid - 1;
    }
    return (max + min) >> 1;
}

twin_sfixed_t
_twin_sfixed_sqrt (twin_sfixed_t as)
{
    twin_dfixed_t   max = as, min = 0, mid;
    twin_dfixed_t   a = twin_sfixed_to_dfixed (as);
    twin_dfixed_t   sqr;

    while (max > min)
    {
	mid = (max + min) >> 1;
	sqr = mid * mid;
	if (sqr == a)
	    return (twin_sfixed_t) mid;
	if (sqr < a)
	    min = mid + 1;
	else
	    max = mid - 1;
    }
    return (twin_sfixed_t) ((max + min) >> 1);
}
