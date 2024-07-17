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

typedef enum _op { OpMove, OpLine, OpCurve, OpNoop } op_t;

typedef struct {
    double x, y;
} pt_t;

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

typedef struct _pts_t {
    int n;
    int s;
    pt_t *pt;
} pts_t;

typedef struct _spline {
    pt_t a, b, c, d;
} spline_t;

spline_t fit(pt_t *p, int n);

pts_t *new_pts(void);

void dispose_pts(pts_t *pts);

void add_pt(pts_t *pts, pt_t *pt);

double distance_to_point(pt_t *a, pt_t *b);

double distance_to_line(pt_t *p, pt_t *p1, pt_t *p2);

double distance_to_segment(pt_t *p, pt_t *p1, pt_t *p2);

pt_t lerp(pt_t *a, pt_t *b);

#endif /* _TWIN_FEDIT_H_ */
