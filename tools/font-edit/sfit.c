/*
 * Copyright (c) 2004 Keith Packard
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
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

#include "font-edit.h"

pts_t *new_pts(void)
{
    pts_t *pts = malloc(sizeof(pts_t));
    if (!pts)
        return NULL;

    pts->n = 0;
    pts->s = 0;
    pts->pt = 0;

    return pts;
}

void dispose_pts(pts_t *pts)
{
    if (pts->pt)
        free(pts->pt);
    free(pts);
}

bool add_pt(pts_t *pts, pt_t *pt)
{
    if (pts->n == pts->s) {
        int ns = pts->s ? pts->s * 2 : 16;
        pt_t *new_pt;

        if (pts->pt)
            new_pt = realloc(pts->pt, ns * sizeof(pt_t));
        else
            new_pt = malloc(ns * sizeof(pt_t));

        if (!new_pt)
            return false;

        pts->pt = new_pt;
        pts->s = ns;
    }
    pts->pt[pts->n++] = *pt;
    return true;
}

double distance_to_point(pt_t *a, pt_t *b)
{
    double dx = (b->x - a->x);
    double dy = (b->y - a->y);

    return sqrt(dx * dx + dy * dy);
}

double distance_to_line(pt_t *p, pt_t *p1, pt_t *p2)
{
    /* Convert to normal form (AX + BY + C = 0)
     *
     * (X - x1) * (y2 - y1) = (Y - y1) * (x2 - x1)
     *
     * X * (y2 - y1) - Y * (x2 - x1) - x1 * (y2 - y1) + y1 * (x2 - x1) = 0
     *
     * A = (y2 - y1)
     * B = (x1 - x2)
     * C = (y1x2 - x1y2)
     *
     * distance² = (AX + BY + C)² / (A² + B²)
     */
    double A = p2->y - p1->y;
    double B = p1->x - p2->x;
    double C = (p1->y * p2->x - p1->x * p2->y);
    double den, num;

    num = A * p->x + B * p->y + C;
    den = A * A + B * B;
    if (den == 0)
        return distance_to_point(p, p1);
    else
        return sqrt((num * num) / den);
}

double distance_to_segment(pt_t *p, pt_t *p1, pt_t *p2)
{
    double dx, dy;
    double pdx, pdy;
    pt_t px;

    /* intersection pt_t * (px):

       px = p1 + u(p2 - p1)
       (p - px) . (p2 - p1) = 0

    Thus:
      u = ((p - p1) . (p2 - p1)) / (||(p2 - p1)|| ^ 2);
    */

    dx = p2->x - p1->x;
    dy = p2->y - p1->y;

    if (dx == 0 && dy == 0)
        return distance_to_point(p, p1);

    pdx = p->x - p1->x;
    pdy = p->y - p1->y;

    double u = (pdx * dx + pdy * dy) / (dx * dx + dy * dy);

    if (u <= 0)
        return distance_to_point(p, p1);
    else if (u >= 1)
        return distance_to_point(p, p2);

    px.x = p1->x + u * (p2->x - p1->x);
    px.y = p1->y + u * (p2->y - p1->y);

    return distance_to_point(p, &px);
}

pt_t lerp(pt_t *a, pt_t *b)
{
    pt_t p;

    p.x = a->x + (b->x - a->x) / 2;
    p.y = a->y + (b->y - a->y) / 2;
    return p;
}

/* Chord-length parameterization
 * Assigns parameter t to each point based on cumulative chord length
 */
static void chord_length_parameterize(pt_t *p, int n, double *t)
{
    double total_len = 0.0;
    int i;

    t[0] = 0.0;

    /* Calculate cumulative chord lengths */
    for (i = 1; i < n; i++) {
        double dx = p[i].x - p[i - 1].x;
        double dy = p[i].y - p[i - 1].y;
        double len = sqrt(dx * dx + dy * dy);
        total_len += len;
        t[i] = total_len;
    }

    /* Normalize to [0, 1] */
    if (total_len > 0.0) {
        for (i = 1; i < n; i++)
            t[i] /= total_len;
    }
}

/* Least-squares cubic Bézier curve fitting - O(n) algorithm
 *
 * Given n points and fixed endpoints (a, d), finds control points (b, c)
 * that minimize squared distance to the curve.
 *
 * Uses chord-length parameterization and normal equations:
 *   For cubic Bézier: B(t) = (1-t)³a + 3(1-t)²t·b + 3(1-t)t²·c + t³d
 *
 *   Rearranged: p - (1-t)³a - t³d = A(t)·b + B(t)·c
 *   where A(t) = 3(1-t)²t, B(t) = 3(1-t)t²
 *
 * Solves 2x2 linear system for b and c using normal equations.
 */
spline_t fit(pt_t *p, int n)
{
    spline_t s;
    double *t;
    double C[2][2] = {{0, 0}, {0, 0}}; /* Coefficient matrix CᵀC */
    double X[2] = {0, 0};              /* Right-hand side for x coords */
    double Y[2] = {0, 0};              /* Right-hand side for y coords */
    double det;
    int i;

    /* Fixed endpoints */
    s.a = p[0];
    s.d = p[n - 1];

    /* Handle degenerate cases */
    if (n < 2) {
        s.b = s.a;
        s.c = s.d;
        return s;
    }

    /* Allocate parameter array */
    t = malloc(n * sizeof(double));
    if (!t) {
        /* Fallback: linear interpolation */
        s.b.x = s.a.x + (s.d.x - s.a.x) / 3.0;
        s.b.y = s.a.y + (s.d.y - s.a.y) / 3.0;
        s.c.x = s.a.x + 2.0 * (s.d.x - s.a.x) / 3.0;
        s.c.y = s.a.y + 2.0 * (s.d.y - s.a.y) / 3.0;
        return s;
    }

    /* Compute parameter values */
    chord_length_parameterize(p, n, t);

    /* Build normal equations: CᵀC and Cᵀrhs */
    for (i = 0; i < n; i++) {
        double ti = t[i];
        double ti2 = ti * ti;
        double ti3 = ti2 * ti;
        double one_minus_t = 1.0 - ti;
        double one_minus_t2 = one_minus_t * one_minus_t;
        double one_minus_t3 = one_minus_t2 * one_minus_t;

        /* Basis functions for control points b and c */
        double A = 3.0 * one_minus_t2 * ti; /* coefficient for b */
        double B = 3.0 * one_minus_t * ti2; /* coefficient for c */

        /* Subtract fixed endpoint contributions from points */
        double tmp_x = p[i].x - one_minus_t3 * s.a.x - ti3 * s.d.x;
        double tmp_y = p[i].y - one_minus_t3 * s.a.y - ti3 * s.d.y;

        /* Accumulate normal equations */
        C[0][0] += A * A;
        C[0][1] += A * B;
        C[1][1] += B * B;

        X[0] += A * tmp_x;
        X[1] += B * tmp_x;
        Y[0] += A * tmp_y;
        Y[1] += B * tmp_y;
    }

    C[1][0] = C[0][1]; /* Matrix is symmetric */

    free(t);

    /* Solve 2x2 system using Cramer's rule */
    det = C[0][0] * C[1][1] - C[0][1] * C[1][0];

    if (fabs(det) < 1e-10) {
        /* Singular matrix: fallback to linear interpolation */
        s.b.x = s.a.x + (s.d.x - s.a.x) / 3.0;
        s.b.y = s.a.y + (s.d.y - s.a.y) / 3.0;
        s.c.x = s.a.x + 2.0 * (s.d.x - s.a.x) / 3.0;
        s.c.y = s.a.y + 2.0 * (s.d.y - s.a.y) / 3.0;
    } else {
        /* Solve for control points */
        s.b.x = (X[0] * C[1][1] - X[1] * C[0][1]) / det;
        s.c.x = (C[0][0] * X[1] - C[1][0] * X[0]) / det;
        s.b.y = (Y[0] * C[1][1] - Y[1] * C[0][1]) / det;
        s.c.y = (C[0][0] * Y[1] - C[1][0] * Y[0]) / det;
    }

    return s;
}
