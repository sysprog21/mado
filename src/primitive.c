/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

#define __inline

static twin_argb32_t __inline
in_over (twin_argb32_t	dst,
	 twin_argb32_t	src,
	 twin_a8_t	msk)
{
    uint16_t	t1, t2, t3, t4;
    twin_a8_t	a;

    switch (msk) {
    case 0:
	return dst;
    case 0xff:
	break;
    default:
	src = (twin_in(src,0,msk,t1) |
	       twin_in(src,8,msk,t2) |
	       twin_in(src,16,msk,t3) |
	       twin_in(src,24,msk,t4));
	break;
    }
    if (!src)
	return dst;
    a = ~(src >> 24);
    switch (a) {
    case 0:
	return src;
    case 0xff:
	dst = (twin_add (src, dst, 0, t1) |
	       twin_add (src, dst, 8, t2) |
	       twin_add (src, dst, 16, t3) |
	       twin_add (src, dst, 24, t4));
	break;
    default:
	dst = (twin_over (src, dst, 0, a, t1) |
	       twin_over (src, dst, 8, a, t2) |
	       twin_over (src, dst, 16, a, t3) |
	       twin_over (src, dst, 24, a, t4));
	break;
    }
    return dst;
}

static twin_argb32_t __inline
in (twin_argb32_t   src,
    twin_a8_t	    msk)
{
    uint16_t	t1, t2, t3, t4;

    return (twin_in(src,0,msk,t1) |
	    twin_in(src,8,msk,t2) |
	    twin_in(src,16,msk,t3) |
	    twin_in(src,24,msk,t4));
}

static twin_argb32_t __inline
over (twin_argb32_t	dst,
      twin_argb32_t	src)
{
    uint16_t	t1, t2, t3, t4;
    twin_a8_t	a;

    if (!src)
	return dst;
    a = ~(src >> 24);
    switch (a) {
    case 0:
	return src;
    case 0xff:
	dst = (twin_add (src, dst, 0, t1) |
	       twin_add (src, dst, 8, t2) |
	       twin_add (src, dst, 16, t3) |
	       twin_add (src, dst, 24, t4));
	break;
    default:
	dst = (twin_over (src, dst, 0, a, t1) |
	       twin_over (src, dst, 8, a, t2) |
	       twin_over (src, dst, 16, a, t3) |
	       twin_over (src, dst, 24, a, t4));
	break;
    }
    return dst;
}

static twin_argb32_t __inline
rgb16_to_argb32 (twin_rgb16_t v)
{
    return twin_rgb16_to_argb32(v);
}

static twin_argb32_t __inline
a8_to_argb32 (twin_a8_t v)
{
    return v << 24;
}

static twin_rgb16_t __inline
argb32_to_rgb16 (twin_argb32_t v)
{
    return twin_argb32_to_rgb16 (v);
}

static twin_a8_t __inline
argb32_to_a8 (twin_argb32_t v)
{
    return v >> 24;
}

/*
 * Naming convention
 *
 *  _twin_<src>_in_<msk>_op_<dst>
 *
 * Use 'c' for constant
 */

#define dst_argb32_get	    (*dst.argb32)
#define dst_argb32_set	    (*dst.argb32++) = 
#define dst_rgb16_get	    (rgb16_to_argb32(*dst.rgb16))
#define dst_rgb16_set	    (*dst.rgb16++) = argb32_to_rgb16
#define dst_a8_get	    (a8_to_argb32(*dst.a8))
#define dst_a8_set	    (*dst.a8++) = argb32_to_a8

#define src_c		    (src.c)
#define src_argb32	    (*src.p.argb32++)
#define src_rgb16	    (rgb16_to_argb32(*src.p.rgb16++))
#define src_a8		    (a8_to_argb32(*src.p.a8++))

#define msk_c		    (argb32_to_a8 (msk.c))
#define msk_argb32	    (argb32_to_a8 (*msk.p.argb32++))
#define msk_rgb16	    (0xff); (void)msk
#define msk_a8		    (*msk.p.a8++)

#define cat2(a,b) a##b
#define cat3(a,b,c) a##b##c
#define cat4(a,b,c,d) a##b##c##d
#define cat6(a,b,c,d,e,f) a##b##c##d##e##f

#define _twin_in_op_name(src,op,msk,dst) cat6(_twin_,src,_in_,msk,op,dst)

#define _twin_op_name(src,op,dst) cat4(_twin_,src,op,dst)

#define make_twin_in_over(__dst,__src,__msk) \
void \
_twin_in_op_name(__src,_over_,__msk,__dst)(twin_pointer_t   dst, \
					   twin_source_u    src, \
					   twin_source_u    msk, \
					   int		    width) \
{ \
    twin_argb32_t   dst32; \
    twin_argb32_t   src32; \
    twin_a8_t	    msk8; \
    while (width--) { \
	dst32 = cat3(dst_,__dst,_get); \
	src32 = cat2(src_,__src); \
	msk8 = cat2(msk_,__msk); \
	dst32 = in_over (dst32, src32, msk8); \
	cat3(dst_,__dst,_set) (dst32); \
    } \
}

#define make_twin_in_source(__dst,__src,__msk) \
void \
_twin_in_op_name(__src,_source_,__msk,__dst)(twin_pointer_t	dst, \
					     twin_source_u	src, \
					     twin_source_u	msk, \
					     int		width) \
{ \
    twin_argb32_t   dst32; \
    twin_argb32_t   src32; \
    twin_a8_t	    msk8; \
    while (width--) { \
	src32 = cat2(src_,__src); \
	msk8 = cat2(msk_,__msk); \
	dst32 = in (src32, msk8); \
	cat3(dst_,__dst,_set) (dst32); \
    } \
}

#define make_twin_in_op_msks(op,dst,src) \
cat2(make_twin_in_,op)(dst,src,argb32) \
cat2(make_twin_in_,op)(dst,src,rgb16) \
cat2(make_twin_in_,op)(dst,src,a8) \
cat2(make_twin_in_,op)(dst,src,c)

#define make_twin_in_op_srcs_msks(op,dst) \
make_twin_in_op_msks(op,dst,argb32) \
make_twin_in_op_msks(op,dst,rgb16) \
make_twin_in_op_msks(op,dst,a8) \
make_twin_in_op_msks(op,dst,c)

#define make_twin_in_op_dsts_srcs_msks(op) \
make_twin_in_op_srcs_msks(op,argb32) \
make_twin_in_op_srcs_msks(op,rgb16) \
make_twin_in_op_srcs_msks(op,a8)

make_twin_in_op_dsts_srcs_msks(over)
make_twin_in_op_dsts_srcs_msks(source)

#define make_twin_over(__dst,__src) \
void \
_twin_op_name(__src,_over_,__dst) (twin_pointer_t   dst, \
				   twin_source_u    src, \
				   int		    width) \
{ \
    twin_argb32_t   dst32; \
    twin_argb32_t   src32; \
    while (width--) { \
	dst32 = cat3(dst_,__dst,_get); \
	src32 = cat2(src_,__src); \
	dst32 = over (dst32, src32); \
	cat3(dst_,__dst,_set) (dst32); \
    } \
}

#define make_twin_source(__dst,__src) \
void \
_twin_op_name(__src,_source_,__dst) (twin_pointer_t dst, \
				     twin_source_u  src, \
				     int	    width) \
{ \
    twin_argb32_t   dst32; \
    twin_argb32_t   src32; \
    while (width--) { \
	src32 = cat2(src_,__src); \
	dst32 = src32; \
	cat3(dst_,__dst,_set) (dst32); \
    } \
}

#define make_twin_op_srcs(op,dst) \
cat2(make_twin_,op)(dst,argb32) \
cat2(make_twin_,op)(dst,rgb16) \
cat2(make_twin_,op)(dst,a8) \
cat2(make_twin_,op)(dst,c)

#define make_twin_op_dsts_srcs(op) \
make_twin_op_srcs(op,argb32) \
make_twin_op_srcs(op,rgb16) \
make_twin_op_srcs(op,a8)

make_twin_op_dsts_srcs(over);
make_twin_op_dsts_srcs(source)

#ifdef HAVE_ALTIVEC

#include <altivec.h>

#define VUNALIGNED(p)	(((unsigned long)(p)) & 0xf)


/*
 * over_v: Altivec over function, some bits inspired by SDL
 */

static inline vector unsigned int over_v (vector unsigned char dst,
					  vector unsigned char src
					  )
{
    const vector unsigned char alphasplit =
	    vec_and (vec_lvsl (0, (int *)NULL), vec_splat_u8(0x0c));
    const vector unsigned char merge =
	    vec_add(vec_lvsl(0, (int *)NULL),
		    (vector unsigned char)vec_splat_u16(0x0f));
    vector unsigned char alpha, alphainv;
    vector unsigned short dmule, dmulo;
    const vector unsigned short v80 = vec_sl(vec_splat_u16(1), vec_splat_u16(7));
    const vector unsigned short v8 = vec_splat_u8(8);

    /* get source alpha values all over the vector */
    alpha = vec_perm(src, src, alphasplit);

    /* invert alpha */
    alphainv = vec_nor(alpha, alpha);

    /* multiply destination values with inverse alpha into 2 u16 vectors */
    dmule = vec_mule(dst, alphainv);
    dmulo = vec_mulo(dst, alphainv);

    /* round and merge back */
    dmule = vec_add(dmule, v80);
    dmulo = vec_add(dmulo, v80);
    dmule = vec_add(dmule, vec_sr(dmule, v8));
    dmulo = vec_add(dmulo, vec_sr(dmulo, v8));
    dst = vec_perm(dmule, dmulo, merge);

    /* return added value */
    return vec_adds(dst, src);
}

void _twin_vec_argb32_over_argb32 (twin_pointer_t 	dst,
				   twin_source_u	src,
				   int			width)
{
    twin_argb32_t   		dst32;
    twin_argb32_t   		src32;
    vector unsigned char 	edgeperm;
    vector unsigned char	src0v, src1v, srcv, dstv;

    /* Go scalar for small amounts as I can't be bothered */
    if (width <  8) {
	    _twin_argb32_over_argb32(dst, src, width);
	    return;
    }

    /* first run scalar until destination is aligned */
    while (VUNALIGNED(dst.v) && width--) {
	    dst32 = dst_argb32_get;
	    src32 = src_argb32;
	    dst32 = over (dst32, src32);
	    dst_argb32_set (dst32);
    }

    /* maybe we should have a special "aligned" version to avoid those
     * permutations...
     */
    edgeperm = vec_lvsl (0, src.p.argb32);
    src0v = vec_ld (0, src.p.argb32);
    while(width >= 4) {
	    dstv = vec_ld (0, dst.argb32);
	    src1v = vec_ld (16, src.p.argb32);
	    srcv = vec_perm (src0v, src1v, edgeperm);
	    dstv = over_v (dstv, srcv);
	    vec_st ((vector unsigned int)dstv, 0, dst.argb32);
	    src.p.argb32 += 4;
	    dst.argb32 += 4;
	    src0v = src1v;
	    width -= 4;
    }

    /* then run scalar again for remaining bits */
    while (width--) {
	    dst32 = dst_argb32_get;
	    src32 = src_argb32;
	    dst32 = over (dst32, src32);
	    dst_argb32_set (dst32);
    }
}

void _twin_vec_argb32_source_argb32 (twin_pointer_t	dst,
				     twin_source_u	src,
				     int	    	width)
{
    twin_argb32_t   		dst32;
    twin_argb32_t   		src32;
    vector unsigned char 	edgeperm;
    vector unsigned char	src0v, src1v, srcv;

    /* first run scalar until destination is aligned */
    while (VUNALIGNED(dst.v) && width--) {
	    src32 = src_argb32;
	    dst32 = src32;
	    dst_argb32_set (dst32);
    }

    /* maybe we should have a special "aligned" version to avoid those
     * permutations...
     */
    edgeperm = vec_lvsl (0, src.p.argb32);
    src0v = vec_ld (0, src.p.argb32);
    while(width >= 4) {
	    src1v = vec_ld (16, src.p.argb32);
	    srcv = vec_perm (src0v, src1v, edgeperm);
	    vec_st ((vector unsigned int)srcv, 0, dst.argb32);
	    src.p.argb32 += 4;
	    dst.argb32 += 4;
	    src0v = src1v;
	    width -= 4;
    }

    /* then run scalar again for remaining bits */
    while (width--) {
	    src32 = src_argb32;
	    dst32 = src32;
	    dst_argb32_set (dst32);
    }
}

#endif /* HAVE_ALTIVEC */
