/*
 * Copyright (c) 2004 Keith Packard
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

#ifndef _TWIN_FEDIT_H_
#define _TWIN_FEDIT_H_

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Geometric types */

/*
 * pt_t - Point in a 2-D coordinate system
 * @x: x-component
 * @y: y-component
 */
typedef struct {
    double x, y;
} pt_t;

/*
 * pts_t - Points in a 2-D coordinate system
 * @n: numeber of the stored points
 * @s: size of the storage
 * @pt: pointer of pt_ts
 */
typedef struct _pts_t {
    int n;
    int s;
    pt_t *pt;
} pts_t;

/*
 * spline_t - Cubic spline type
 * @a: starting point.
 * @b: first control point.
 * @c: last control point.
 * @d: ending point.
 */
typedef struct _spline {
    pt_t a, b, c, d;
} spline_t;

/* Command line tpyes */

typedef enum _op { OpMove, OpLine, OpCurve, OpNoop } op_t;

typedef struct _cmd {
    struct _cmd *next;
    op_t op;
    pt_t pt[3];
} cmd_t;

typedef struct _cmd_stack {
    struct _cmd_stack *prev;
    cmd_t *cmd;
} cmd_stack_t;

typedef struct _char {
    cmd_t *cmd;
    cmd_stack_t *stack;
    cmd_t *first;
    cmd_t *last;
} char_t;

/* Geometric functions */

/*
 * new_pts() - Allocate and initialize a new point
 *
 * Return: pts_t type, allocated point.
 */
pts_t *new_pts(void);

/*
 * dispose_pts() - Free all storage used by pts_t
 * @pts: the points to be free
 */
void dispose_pts(pts_t *pts);

/*
 * add_pt() - Add a point to pts_t
 * @pts: the object that receives the added points
 * @pt: the point to be added
 */
void add_pt(pts_t *pts, pt_t *pt);

/*
 * distance_to_point() - Calculate distance between two points
 * @a: point in 2-D coordinate
 * @b: point in 2-D coordinate
 *
 * Return: double type, the distance between point a and point b.
 */
double distance_to_point(pt_t *a, pt_t *b);

/*
 * distance_to_line() - Calculate distance from a point to a line
 * @p: point outside the line
 * @p1: one of the points used to calculate the line
 * @p2: one of the points used to calculate the line
 *
 * Return: double type, the distance from point p to the line.
 */
double distance_to_line(pt_t *p, pt_t *p1, pt_t *p2);

/*
 * distance_to_segment() - Calculate shortest distance from a point
 *                         to a line segment
 * @p: point outside the line segment
 * @p1: one of the points used to calculate the line segment
 * @p2: one of the points used to calculate the line segment
 *
 * Return: double type, the distance from point p to the line segment.
 */
double distance_to_segment(pt_t *p, pt_t *p1, pt_t *p2);

/*
 * lerp() - Interpolate linearly a point between two points
 * @a: one of the point to be interpolated
 * @b: one of the point to be interpolated
 *
 * Return: pt_t type, a interpolation point by two points.
 */
pt_t lerp(pt_t *a, pt_t *b);

/*
 * fit() - Fit a spline within a specified tolerance
 * @a: points
 * @n: number of points
 *
 * Return: spline_t type, a cubic spline of points.
 */
spline_t fit(pt_t *p, int n);

#endif /* _TWIN_FEDIT_H_ */
