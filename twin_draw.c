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

/* op, src, dst */
static twin_src_op  comp2[2][4][3] = {
    {	/* OVER */
	{   /* A8 */
	    _twin_a8_over_a8,
	    _twin_a8_over_rgb16,
	    _twin_a8_over_argb32,
	},
	{   /* RGB16 */
	    _twin_rgb16_over_a8,
	    _twin_rgb16_over_rgb16,
	    _twin_rgb16_over_argb32,
	},
	{   /* ARGB32 */
	    _twin_argb32_over_a8,
	    _twin_argb32_over_rgb16,
	    _twin_argb32_over_argb32,
	},
	{   /* C */
	    _twin_c_over_a8,
	    _twin_c_over_rgb16,
	    _twin_c_over_argb32,
	}
    },
    {	/* SOURCE */
	{   /* A8 */
	    _twin_a8_source_a8,
	    _twin_a8_source_rgb16,
	    _twin_a8_source_argb32,
	},
	{   /* RGB16 */
	    _twin_rgb16_source_a8,
	    _twin_rgb16_source_rgb16,
	    _twin_rgb16_source_argb32,
	},
	{   /* ARGB32 */
	    _twin_argb32_source_a8,
	    _twin_argb32_source_rgb16,
	    _twin_argb32_source_argb32,
	},
	{   /* C */
	    _twin_c_source_a8,
	    _twin_c_source_rgb16,
	    _twin_c_source_argb32,
	}
    }
};

/* op, src, msk, dst */
static twin_src_msk_op	comp3[2][4][4][3] = {
    {	/* OVER */
	{   /* A8 */
	    {	/* A8 */
		_twin_a8_in_a8_over_a8,
		_twin_a8_in_a8_over_rgb16,
		_twin_a8_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_a8_in_rgb16_over_a8,
		_twin_a8_in_rgb16_over_rgb16,
		_twin_a8_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_a8_in_argb32_over_a8,
		_twin_a8_in_argb32_over_rgb16,
		_twin_a8_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_a8_in_c_over_a8,
		_twin_a8_in_c_over_rgb16,
		_twin_a8_in_c_over_argb32,
	    },
	},
	{   /* RGB16 */
	    {	/* A8 */
		_twin_rgb16_in_a8_over_a8,
		_twin_rgb16_in_a8_over_rgb16,
		_twin_rgb16_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_rgb16_in_rgb16_over_a8,
		_twin_rgb16_in_rgb16_over_rgb16,
		_twin_rgb16_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_rgb16_in_argb32_over_a8,
		_twin_rgb16_in_argb32_over_rgb16,
		_twin_rgb16_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_rgb16_in_c_over_a8,
		_twin_rgb16_in_c_over_rgb16,
		_twin_rgb16_in_c_over_argb32,
	    },
	},
	{   /* ARGB32 */
	    {	/* A8 */
		_twin_argb32_in_a8_over_a8,
		_twin_argb32_in_a8_over_rgb16,
		_twin_argb32_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_argb32_in_rgb16_over_a8,
		_twin_argb32_in_rgb16_over_rgb16,
		_twin_argb32_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_argb32_in_argb32_over_a8,
		_twin_argb32_in_argb32_over_rgb16,
		_twin_argb32_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_argb32_in_c_over_a8,
		_twin_argb32_in_c_over_rgb16,
		_twin_argb32_in_c_over_argb32,
	    },
	},
	{   /* C */
	    {	/* A8 */
		_twin_c_in_a8_over_a8,
		_twin_c_in_a8_over_rgb16,
		_twin_c_in_a8_over_argb32,
	    },
	    {	/* RGB16 */
		_twin_c_in_rgb16_over_a8,
		_twin_c_in_rgb16_over_rgb16,
		_twin_c_in_rgb16_over_argb32,
	    },
	    {	/* ARGB32 */
		_twin_c_in_argb32_over_a8,
		_twin_c_in_argb32_over_rgb16,
		_twin_c_in_argb32_over_argb32,
	    },
	    {	/* C */
		_twin_c_in_c_over_a8,
		_twin_c_in_c_over_rgb16,
		_twin_c_in_c_over_argb32,
	    },
	},
    },
    {	/* SOURCE */
	{   /* A8 */
	    {	/* A8 */
		_twin_a8_in_a8_source_a8,
		_twin_a8_in_a8_source_rgb16,
		_twin_a8_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_a8_in_rgb16_source_a8,
		_twin_a8_in_rgb16_source_rgb16,
		_twin_a8_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_a8_in_argb32_source_a8,
		_twin_a8_in_argb32_source_rgb16,
		_twin_a8_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_a8_in_c_source_a8,
		_twin_a8_in_c_source_rgb16,
		_twin_a8_in_c_source_argb32,
	    },
	},
	{   /* RGB16 */
	    {	/* A8 */
		_twin_rgb16_in_a8_source_a8,
		_twin_rgb16_in_a8_source_rgb16,
		_twin_rgb16_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_rgb16_in_rgb16_source_a8,
		_twin_rgb16_in_rgb16_source_rgb16,
		_twin_rgb16_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_rgb16_in_argb32_source_a8,
		_twin_rgb16_in_argb32_source_rgb16,
		_twin_rgb16_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_rgb16_in_c_source_a8,
		_twin_rgb16_in_c_source_rgb16,
		_twin_rgb16_in_c_source_argb32,
	    },
	},
	{   /* ARGB32 */
	    {	/* A8 */
		_twin_argb32_in_a8_source_a8,
		_twin_argb32_in_a8_source_rgb16,
		_twin_argb32_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_argb32_in_rgb16_source_a8,
		_twin_argb32_in_rgb16_source_rgb16,
		_twin_argb32_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_argb32_in_argb32_source_a8,
		_twin_argb32_in_argb32_source_rgb16,
		_twin_argb32_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_argb32_in_c_source_a8,
		_twin_argb32_in_c_source_rgb16,
		_twin_argb32_in_c_source_argb32,
	    },
	},
	{   /* C */
	    {	/* A8 */
		_twin_c_in_a8_source_a8,
		_twin_c_in_a8_source_rgb16,
		_twin_c_in_a8_source_argb32,
	    },
	    {	/* RGB16 */
		_twin_c_in_rgb16_source_a8,
		_twin_c_in_rgb16_source_rgb16,
		_twin_c_in_rgb16_source_argb32,
	    },
	    {	/* ARGB32 */
		_twin_c_in_argb32_source_a8,
		_twin_c_in_argb32_source_rgb16,
		_twin_c_in_argb32_source_argb32,
	    },
	    {	/* C */
		_twin_c_in_c_source_a8,
		_twin_c_in_c_source_rgb16,
		_twin_c_in_c_source_argb32,
	    },
	},
    }
};

    
#define operand_index(o)    ((o)->source_kind == TWIN_SOLID ? 3 : o->u.pixmap->format)

void
twin_composite (twin_pixmap_t	*dst,
		twin_coord_t	dst_x,
		twin_coord_t	dst_y,
		twin_operand_t	*src,
		twin_coord_t	src_x,
		twin_coord_t	src_y,
		twin_operand_t	*msk,
		twin_coord_t	msk_x,
		twin_coord_t	msk_y,
		twin_operator_t	operator,
		twin_coord_t	width,
		twin_coord_t	height)
{
    twin_coord_t    iy;
    twin_coord_t    left, top, right, bottom;
    twin_coord_t    sdx, sdy;
    twin_source_u   s;

    dst_x += dst->clip.left;
    dst_y += dst->clip.top;
    left = dst_x;
    top = dst_y;
    right = dst_x + width;
    bottom = dst_y + height;
    /* clip */
    if (left < dst->clip.left)
	left = dst->clip.left;
    if (top < dst->clip.top)
	top = dst->clip.top;
    if (right > dst->clip.right)
	right = dst->clip.right;
    if (bottom > dst->clip.bottom)
	bottom = dst->clip.bottom;

    if (left >= right || top >= bottom)
	return;

    if (src->source_kind == TWIN_PIXMAP)
    {
	src_x += src->u.pixmap->clip.left;
	src_y += src->u.pixmap->clip.top;
    }
    else
        s.c = src->u.argb;
    
    sdx = src_x - dst_x;
    sdy = src_y - dst_y;

    if (msk)
    {
	twin_src_msk_op	op;
	twin_source_u   m;
	twin_coord_t	mdx, mdy;
	
	if (msk->source_kind == TWIN_PIXMAP)
	{
	    msk_x += msk->u.pixmap->clip.left;
	    msk_y += msk->u.pixmap->clip.top;
	}
	else
	    s.c = msk->u.argb;
	
	mdx = msk_x - dst_x;
	mdy = msk_y - dst_y;

	op = comp3[operator][operand_index(src)][operand_index(msk)][dst->format];
	for (iy = top; iy < bottom; iy++)
	{
	    if (src->source_kind == TWIN_PIXMAP)
		s.p = twin_pixmap_pointer (src->u.pixmap, left+sdx, iy+sdy);
	    if (msk->source_kind == TWIN_PIXMAP)
		m.p = twin_pixmap_pointer (msk->u.pixmap, left+mdx, iy+mdy);
	    (*op) (twin_pixmap_pointer (dst, left, iy),
		   s, m, right - left);
	}
    }
    else
    {
	twin_src_op	op;
	
	op = comp2[operator][operand_index(src)][dst->format];
	
	for (iy = top; iy < bottom; iy++)
	{
	    if (src->source_kind == TWIN_PIXMAP)
		s.p = twin_pixmap_pointer (src->u.pixmap, left+sdx, iy+sdy);
	    (*op) (twin_pixmap_pointer (dst, left, iy),
		   s, right - left);
	}
    }
    twin_pixmap_damage (dst, left, top, right, bottom);
}
	     
/*
 * array primary    index is OVER SOURCE
 * array secondary  index is ARGB32 RGB16 A8
 */
static twin_src_op  fill[2][3] = {
    {	/* OVER */
	_twin_c_over_a8,
	_twin_c_over_rgb16,
	_twin_c_over_argb32,
    },
    {	/* SOURCE */
	_twin_c_source_a8,
	_twin_c_source_rgb16,
	_twin_c_source_argb32,
    }
};

void
twin_fill (twin_pixmap_t    *dst,
	   twin_argb32_t    pixel,
	   twin_operator_t  operator,
	   twin_coord_t	    left,
	   twin_coord_t	    top,
	   twin_coord_t	    right,
	   twin_coord_t	    bottom)
{
    twin_src_op	    op;
    twin_source_u   src;
    twin_coord_t    iy;
    
    left += dst->clip.left;
    right += dst->clip.left;
    top += dst->clip.top;
    bottom += dst->clip.top;
    /* clip */
    if (left < dst->clip.left)
	left = dst->clip.left;
    if (right > dst->clip.right)
	right = dst->clip.right;
    if (top < dst->clip.top)
	top = dst->clip.top;
    if (bottom > dst->clip.bottom)
	bottom = dst->clip.bottom;
    if (left >= right || top >= bottom)
	return;
    src.c = pixel;
    op = fill[operator][dst->format];
    for (iy = top; iy < bottom; iy++)
	(*op) (twin_pixmap_pointer (dst, left, iy), src, right - left);
    twin_pixmap_damage (dst, left, top, right, bottom);
}
