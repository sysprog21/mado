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

#include "font-edit.h"

static double min(double a, double b)
{
    return a < b ? a : b;
}

static double max(double a, double b)
{
    return a > b ? a : b;
}

double sqrt(double x)
{
    double i = x / 2;
    if (x < 0)
        return 0;
    while (fabs(i - (x / i)) / i > 0.000000000000001)
        i = (i + (x / i)) / 2;
    return i;
}

pts_t *new_pts(void)
{
    pts_t *pts = malloc(sizeof(pts_t));

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

void add_pt(pts_t *pts, pt_t *pt)
{
    if (pts->n == pts->s) {
        int ns = pts->s ? pts->s * 2 : 16;

        if (pts->pt)
            pts->pt = realloc(pts->pt, ns * sizeof(pt_t));
        else
            pts->pt = malloc(ns * sizeof(pt_t));
        pts->s = ns;
    }
    pts->pt[pts->n++] = *pt;
}

double distance_to_point(pt_t *a, pt_t *b)
{
    double dx = (b->x - a->x);
    double dy = (b->y - a->y);

    return sqrt(dx * dx + dy * dy);
}

double distance_to_line(pt_t *p, pt_t *p1, pt_t *p2)
{
    /*
     * Convert to normal form (AX + BY + C = 0)
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

static double distance_to_segments(pt_t *p, pts_t *s)
{
    double m = distance_to_segment(p, &s->pt[0], &s->pt[1]);
    int i;
    for (i = 1; i < s->n - 1; i++) {
        double d = distance_to_segment(p, &s->pt[i], &s->pt[i + 1]);
        m = min(d, m);
    }
    return m;
}

pt_t lerp(pt_t *a, pt_t *b)
{
    pt_t p;

    p.x = a->x + (b->x - a->x) / 2;
    p.y = a->y + (b->y - a->y) / 2;
    return p;
}

static void dcj(spline_t *s, spline_t *s1, spline_t *s2)
{
    pt_t ab = lerp(&s->a, &s->b);
    pt_t bc = lerp(&s->b, &s->c);
    pt_t cd = lerp(&s->c, &s->d);
    pt_t abbc = lerp(&ab, &bc);
    pt_t bccd = lerp(&bc, &cd);
    pt_t final = lerp(&abbc, &bccd);

    s1->a = s->a;
    s1->b = ab;
    s1->c = abbc;
    s1->d = final;

    s2->a = final;
    s2->b = bccd;
    s2->c = cd;
    s2->d = s->d;
}

static double spline_error(spline_t *s)
{
    double berr, cerr;

    berr = distance_to_line(&s->b, &s->a, &s->d);
    cerr = distance_to_line(&s->c, &s->a, &s->d);
    return max(berr, cerr);
}

static void decomp(pts_t *pts, spline_t *s, double tolerance)
{
    if (spline_error(s) <= tolerance)
        add_pt(pts, &s->a);
    else {
        spline_t s1, s2;
        dcj(s, &s1, &s2);
        decomp(pts, &s1, tolerance);
        decomp(pts, &s2, tolerance);
    }
}

static pts_t *decompose(spline_t *s, double tolerance)
{
    pts_t *result = new_pts();

    decomp(result, s, tolerance);
    add_pt(result, &s->d);
    return result;
}

static double spline_fit_error(pt_t *p, int n, spline_t *s, double tolerance)
{
    pts_t *sp = decompose(s, tolerance);
    double err = 0;
    int i;

    for (i = 0; i < n; i++) {
        double e = distance_to_segments(&p[i], sp);
        err += e * e;
    }
    dispose_pts(sp);
    return err;
}

spline_t fit(pt_t *p, int n)
{
    spline_t s;
    spline_t best_s;

    double tol = 0.5;
    double best_err = 10000;
    double sbx_min;
    double sbx_max;
    double sby_min;
    double sby_max;
    double scx_min;
    double scx_max;
    double scy_min;
    double scy_max;

    s.a = p[0];
    s.d = p[n - 1];

    if (s.a.x < s.d.x) {
        sbx_min = s.a.x;
        sbx_max = s.d.x;

        scx_max = s.d.x;
        scx_min = s.a.x;
    } else {
        sbx_max = s.a.x;
        sbx_min = s.d.x;

        scx_min = s.d.x;
        scx_max = s.a.x;
    }

    if (s.a.y < s.d.y) {
        sby_min = s.a.y;
        sby_max = s.d.y;

        scy_max = s.d.y;
        scy_min = s.a.y;
    } else {
        sby_max = s.a.y;
        sby_min = s.d.y;

        scy_min = s.d.y;
        scy_max = s.a.y;
    }

    tol = 0.5;
    for (s.b.x = sbx_min; s.b.x <= sbx_max; s.b.x += 1.0)
        for (s.b.y = sby_min; s.b.y <= sby_max; s.b.y += 1.0)
            for (s.c.x = scx_min; s.c.x <= scx_max; s.c.x += 1.0)
                for (s.c.y = scy_min; s.c.y <= scy_max; s.c.y += 1.0) {
                    double err = spline_fit_error(p, n, &s, tol);
                    if (err < best_err) {
                        best_err = err;
                        best_s = s;
                    }
                }
    return best_s;
}
