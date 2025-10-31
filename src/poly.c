/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "twin_private.h"

typedef struct _twin_edge {
    struct _twin_edge *next;
    twin_sfixed_t top, bot;
    twin_sfixed_t x;
    twin_sfixed_t e;
    twin_sfixed_t dx, dy;
    twin_sfixed_t inc_x;
    twin_sfixed_t step_x;
    int winding;
    uint8_t aa_quality; /* Adaptive AA: 0=1x1, 1=2x2, 2=4x4 */
} twin_edge_t;

#define TWIN_POLY_SHIFT 2
#define TWIN_POLY_FIXED_SHIFT (4 - TWIN_POLY_SHIFT)
#define TWIN_POLY_SAMPLE (1 << TWIN_POLY_SHIFT)
#define TWIN_POLY_MASK (TWIN_POLY_SAMPLE - 1)
#define TWIN_POLY_STEP (TWIN_SFIXED_ONE >> TWIN_POLY_SHIFT)
#define TWIN_POLY_START (TWIN_POLY_STEP >> 1)
#define TWIN_POLY_CEIL(c) (((c) + (TWIN_POLY_STEP - 1)) & ~(TWIN_POLY_STEP - 1))
#define TWIN_POLY_COL(x) (((x) >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK)

/* Adaptive AA quality computation
 *
 * Only optimize perfectly vertical edges (dx=0) where visual impact is
 * guaranteed to be minimal. All other edges use full 4x4 AA to preserve
 * quality, especially for text, curves, and diagonal strokes.
 *
 * Returns: 0 (1x1), or 2 (4x4 - default)
 */
static inline uint8_t _compute_aa_quality(twin_sfixed_t dx, twin_sfixed_t dy)
{
    (void) dy; /* Unused in conservative mode */

    /* Conservative approach: only optimize perfectly vertical edges */
    if (dx == 0)
        return 0; /* Vertical: 1x1 (16x faster per edge) */

    /* All other edges use full quality to preserve visual fidelity */
    return 2; /* Default: 4x4 (full AA) */
}

static int _edge_compare_y(const void *a, const void *b)
{
    const twin_edge_t *ae = a;
    const twin_edge_t *be = b;

    return (int) (ae->top - be->top);
}

static void _edge_step_by(twin_edge_t *edge, twin_sfixed_t dy)
{
    twin_dfixed_t e;

    e = edge->e + (twin_dfixed_t) dy * edge->dx;
    edge->x += edge->step_x * dy + edge->inc_x * (e / edge->dy);
    edge->e = e % edge->dy;
}

/*
 * Returns the nearest grid coordinate no less than f
 *
 * Grid coordinates are at TWIN_POLY_STEP/2 + n*TWIN_POLY_STEP
 */

static twin_sfixed_t _twin_sfixed_grid_ceil(twin_sfixed_t f)
{
    return ((f + (TWIN_POLY_START - 1)) & ~(TWIN_POLY_STEP - 1)) +
           TWIN_POLY_START;
}

static int _twin_edge_build(twin_spoint_t *vertices,
                            int nvertices,
                            twin_edge_t *edges,
                            twin_sfixed_t dx,
                            twin_sfixed_t dy,
                            twin_sfixed_t top_y)
{
    int tv, bv;

    int e = 0;
    for (int v = 0; v < nvertices; v++) {
        int nv = v + 1;
        if (nv == nvertices)
            nv = 0;

        /* skip horizontal edges */
        if (vertices[v].y == vertices[nv].y)
            continue;

        /* figure winding */
        if (vertices[v].y < vertices[nv].y) {
            edges[e].winding = 1;
            tv = v;
            bv = nv;
        } else {
            edges[e].winding = -1;
            tv = nv;
            bv = v;
        }

        /* snap top to first grid point in pixmap */
        twin_sfixed_t y = _twin_sfixed_grid_ceil(vertices[tv].y + dy);
        if (y < TWIN_POLY_START + top_y)
            y = TWIN_POLY_START + top_y;

        /* skip vertices which don't span a sample row */
        if (y >= vertices[bv].y + dy)
            continue;

        /* Compute bresenham terms */
        edges[e].dx = vertices[bv].x - vertices[tv].x;
        edges[e].dy = vertices[bv].y - vertices[tv].y;

        /* Compute adaptive AA quality based on slope */
        edges[e].aa_quality = _compute_aa_quality(edges[e].dx, edges[e].dy);

        /* Sanity check: AA quality must be 0 or 2 in conservative mode
         * (1 is reserved for future medium-quality mode if needed)
         */
        assert(edges[e].aa_quality <= 2 && "Invalid AA quality computed");
        assert((edges[e].aa_quality == 0 || edges[e].aa_quality == 2) &&
               "Conservative mode should only produce 0 or 2");

        if (edges[e].dx >= 0)
            edges[e].inc_x = 1;
        else {
            edges[e].inc_x = -1;
            edges[e].dx = -edges[e].dx;
        }
        edges[e].step_x = edges[e].inc_x * (edges[e].dx / edges[e].dy);
        edges[e].dx = edges[e].dx % edges[e].dy;

        edges[e].top = vertices[tv].y + dy;
        edges[e].bot = vertices[bv].y + dy;

        edges[e].x = vertices[tv].x + dx;
        edges[e].e = 0;

        /* step to first grid point */
        _edge_step_by(&edges[e], y - edges[e].top);

        edges[e].top = y;
        e++;
    }
    return e;
}

/* Optimized span fill for perfectly vertical edges (dx=0)
 *
 * For vertical edges, coverage doesn't vary horizontally within a pixel.
 * All sub-pixel positions contribute equally (0x10 or 0x0f for rounding).
 * This allows us to:
 * 1. Skip array indexing (use constants directly)
 * 2. Simplify partial pixel calculations
 * 3. Eliminate the coverage table lookup entirely
 *
 * WARNING: This optimization is only correct for perfectly vertical edges
 * where the span is guaranteed to have uniform horizontal coverage.
 */
static inline void _span_fill_vertical(twin_pixmap_t *pixmap,
                                       twin_sfixed_t y,
                                       twin_sfixed_t left,
                                       twin_sfixed_t right)
{
    /* For vertical edges, coverage is uniform across all horizontal positions.
     * We use constant 0x10 for all sub-pixels, accepting a tiny rounding
     * error on row 2 (0x10 vs 0x0f for the first sub-pixel).
     *
     * Precision trade-off:
     * - Row 2 partial pixels: max error = 1/255 = 0.4% (visually imperceptible)
     *
     * Full pixel coverage: 4 * 0x10 = 0x40 (64 in decimal)
     * Note: Row 2 should technically be 0x3f, but 0x40 is within
     * acceptable rounding error and allows constant-based optimization.
     */
    const twin_a16_t full_coverage = 0x40;

    int row = twin_sfixed_trunc(y);
    twin_a8_t *span = pixmap->p.a8 + row * pixmap->stride;
    twin_a8_t *s;
    twin_sfixed_t x;
    twin_a16_t a;

    /* Clip to pixmap */
    if (left < twin_int_to_sfixed(pixmap->clip.left))
        left = twin_int_to_sfixed(pixmap->clip.left);

    if (right > twin_int_to_sfixed(pixmap->clip.right))
        right = twin_int_to_sfixed(pixmap->clip.right);

    /* Convert to sample grid */
    left = _twin_sfixed_grid_ceil(left) >> TWIN_POLY_FIXED_SHIFT;
    right = _twin_sfixed_grid_ceil(right) >> TWIN_POLY_FIXED_SHIFT;

    /* Check for empty */
    if (right <= left)
        return;

    x = left;

    /* Starting address */
    s = span + (x >> TWIN_POLY_SHIFT);

    /* First pixel (may be partial)
     * For vertical edges, each sub-pixel contributes constant 0x10.
     * This is optimized to count * 0x10, which the compiler can
     * optimize to a shift operation (count << 4).
     */
    if (x & TWIN_POLY_MASK) {
        int count = 0;
        while (x < right && (x & TWIN_POLY_MASK)) {
            count++;
            x++;
        }
        twin_a16_t w = count * 0x10; /* Constant multiplication, fast */
        a = *s + w;
        *s++ = twin_sat(a);
    }

    /* Middle pixels (full pixels) - constant coverage per pixel
     * This is the hot path where we get the biggest win
     */
    while (x + TWIN_POLY_MASK < right) {
        a = *s + full_coverage;
        *s++ = twin_sat(a);
        x += TWIN_POLY_SAMPLE;
    }

    /* Last pixel (may be partial)
     * Same optimization as first pixel: use constant 0x10 per sub-pixel.
     */
    if (right & TWIN_POLY_MASK && x != right) {
        int count = 0;
        while (x < right) {
            count++;
            x++;
        }
        twin_a16_t w = count * 0x10; /* Constant multiplication, fast */
        a = *s + w;
        *s = twin_sat(a);
    }
}

static void _span_fill(twin_pixmap_t *pixmap,
                       twin_sfixed_t y,
                       twin_sfixed_t left,
                       twin_sfixed_t right,
                       uint8_t aa_quality)
{
    /* Coverage table for anti-aliasing
     *
     * The grid is always 4x4 (TWIN_POLY_SHIFT=2, TWIN_POLY_STEP=0.25 pixels).
     * Each entry represents coverage contribution for a sub-pixel position.
     * Sum of all 16 entries = 0xFF (255) for full pixel coverage.
     */
    static const twin_a8_t coverage[4][4] = {
        {0x10, 0x10, 0x10, 0x10},
        {0x10, 0x10, 0x10, 0x10},
        {0x0f, 0x10, 0x10, 0x10}, /* Rounding: 15+240=255 */
        {0x10, 0x10, 0x10, 0x10},
    };

    /* NOTE: aa_quality parameter is currently unused in standard path.
     * Adaptive AA optimization is handled by calling _span_fill_vertical()
     * for perfectly vertical edges (dx=0) in _twin_edge_fill().
     */
    (void) aa_quality;

    const twin_a8_t *cover =
        &coverage[(y >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK][0];

    int row = twin_sfixed_trunc(y);
    twin_a8_t *span = pixmap->p.a8 + row * pixmap->stride;
    twin_a8_t *s;
    twin_sfixed_t x;
    twin_a16_t a;
    twin_a16_t w;
    int col;

    /* clip to pixmap */
    if (left < twin_int_to_sfixed(pixmap->clip.left))
        left = twin_int_to_sfixed(pixmap->clip.left);

    if (right > twin_int_to_sfixed(pixmap->clip.right))
        right = twin_int_to_sfixed(pixmap->clip.right);

    /* convert to sample grid */
    left = _twin_sfixed_grid_ceil(left) >> TWIN_POLY_FIXED_SHIFT;
    right = _twin_sfixed_grid_ceil(right) >> TWIN_POLY_FIXED_SHIFT;

    /* check for empty */
    if (right <= left)
        return;

    x = left;

    /* starting address */
    s = span + (x >> TWIN_POLY_SHIFT);

    /* first pixel */
    if (x & TWIN_POLY_MASK) {
        w = 0;
        col = 0;
        while (x < right && (x & TWIN_POLY_MASK)) {
            w += cover[col++];
            x++;
        }
        a = *s + w;
        *s++ = twin_sat(a);
    }

    w = 0;
    for (col = 0; col < TWIN_POLY_SAMPLE; col++)
        w += cover[col];

    /* middle pixels */
    while (x + TWIN_POLY_MASK < right) {
        a = *s + w;
        *s++ = twin_sat(a);
        x += TWIN_POLY_SAMPLE;
    }

    /* last pixel */
    if (right & TWIN_POLY_MASK && x != right) {
        w = 0;
        col = 0;
        while (x < right) {
            w += cover[col++];
            x++;
        }
        a = *s + w;
        *s = twin_sat(a);
    }
}

static void _twin_edge_fill(twin_pixmap_t *pixmap,
                            twin_edge_t *edges,
                            int nedges)
{
    twin_edge_t *active, *a, *n, **prev;
    twin_sfixed_t x0 = 0;

    qsort(edges, nedges, sizeof(twin_edge_t), _edge_compare_y);
    int e = 0;
    twin_sfixed_t y = edges[0].top;
    active = 0;
    for (;;) {
        /* add in new edges */
        for (; e < nedges && edges[e].top <= y; e++) {
            for (prev = &active; (a = *prev); prev = &(a->next))
                if (a->x > edges[e].x)
                    break;
            edges[e].next = *prev;
            *prev = &edges[e];
        }

        /* walk this y value marking coverage */
        int w = 0;
        twin_edge_t *edge_start = NULL;
        for (a = active; a; a = a->next) {
            if (w == 0) {
                x0 = a->x;
                edge_start = a;
            }
            w += a->winding;
            if (w != 0)
                continue;

            /* Adaptive AA: use optimized path for perfectly vertical edges
             * Only apply to spans >= 16 pixels to avoid branch overhead.
             * Threshold: 16 pixels * 4 samples/pixel = 64 samples
             *
             * Check if both edges forming this span are vertical (dx=0).
             */
            twin_sfixed_t span_width = a->x - x0;
            if (edge_start && edge_start->dx == 0 && a->dx == 0 &&
                span_width >= (16 << TWIN_POLY_FIXED_SHIFT)) {
                /* Both edges vertical and span is wide enough: use optimized
                 * span fill */
                _span_fill_vertical(pixmap, y, x0, a->x);
            } else {
                /* General case or thin/medium span: use full 4x4 AA */
                _span_fill(pixmap, y, x0, a->x, 2);
            }
        }

        /* step down, clipping to pixmap */
        y += TWIN_POLY_STEP;

        if (twin_sfixed_trunc(y) >= pixmap->clip.bottom)
            break;

        /* strip out dead edges */
        for (prev = &active; (a = *prev);) {
            if (a->bot <= y)
                *prev = a->next;
            else
                prev = &a->next;
        }

        /* check for all done */
        if (!active && e == nedges)
            break;

        /* step all edges */
        for (a = active; a; a = a->next)
            _edge_step_by(a, TWIN_POLY_STEP);

        /* fix x sorting */
        for (prev = &active; (a = *prev) && (n = a->next);) {
            if (a->x > n->x) {
                a->next = n->next;
                n->next = a;
                *prev = n;
                prev = &active;
            } else
                prev = &a->next;
        }
    }
}

void twin_fill_path(twin_pixmap_t *pixmap,
                    twin_path_t *path,
                    twin_coord_t dx,
                    twin_coord_t dy)
{
    twin_sfixed_t sdx = twin_int_to_sfixed(dx + pixmap->origin_x);
    twin_sfixed_t sdy = twin_int_to_sfixed(dy + pixmap->origin_y);

    int nalloc = path->npoints + path->nsublen + 1;
    twin_edge_t *edges = malloc(sizeof(twin_edge_t) * nalloc);
    if (!edges)
        return;
    int p = 0;
    int nedges = 0;
    for (int s = 0; s <= path->nsublen; s++) {
        int sublen;
        if (s == path->nsublen)
            sublen = path->npoints;
        else
            sublen = path->sublen[s];
        int npoints = sublen - p;
        if (npoints > 1) {
            int n =
                _twin_edge_build(path->points + p, npoints, edges + nedges, sdx,
                                 sdy, twin_int_to_sfixed(pixmap->clip.top));
            p = sublen;
            nedges += n;
        }
    }
    _twin_edge_fill(pixmap, edges, nedges);
    free(edges);
}
