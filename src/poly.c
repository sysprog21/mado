/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>
#include <string.h>

#include "twin_private.h"

typedef struct _twin_edge {
    twin_sfixed_t top, bot;
    twin_sfixed_t x;
    twin_sfixed_t e;
    twin_sfixed_t dx, dy;
    twin_sfixed_t inc_x;
    twin_sfixed_t step_x;
    /* One sample row (TWIN_POLY_STEP) in Bresenham terms: x moves by
     * row_x, e by row_e, plus inc_x once e reaches dy */
    twin_sfixed_t row_x;
    twin_sfixed_t row_e;
    int winding;
} twin_edge_t;

#define TWIN_POLY_SHIFT 2
#define TWIN_POLY_FIXED_SHIFT (4 - TWIN_POLY_SHIFT)
#define TWIN_POLY_SAMPLE (1 << TWIN_POLY_SHIFT)
#define TWIN_POLY_MASK (TWIN_POLY_SAMPLE - 1)
#define TWIN_POLY_STEP (TWIN_SFIXED_ONE >> TWIN_POLY_SHIFT)
#define TWIN_POLY_START (TWIN_POLY_STEP >> 1)
#define TWIN_POLY_CEIL(c) (((c) + (TWIN_POLY_STEP - 1)) & ~(TWIN_POLY_STEP - 1))
#define TWIN_POLY_COL(x) (((x) >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK)

static int _edge_compare_y(const void *a, const void *b)
{
    const twin_edge_t *ae = *(twin_edge_t *const *) a;
    const twin_edge_t *be = *(twin_edge_t *const *) b;

    return (int) (ae->top - be->top);
}

/* Order @order (pointers into the edge array) by top.  Outlines arrive in
 * long runs already in order, where insertion sort is near linear; only
 * big paths go to qsort, which then moves pointers, not edges. */
static void _edge_sort(twin_edge_t **order, int n)
{
    if (n > 48) {
        qsort(order, n, sizeof(*order), _edge_compare_y);
        return;
    }
    for (int i = 1; i < n; i++) {
        twin_edge_t *e = order[i];
        int j = i;
        for (; j > 0 && order[j - 1]->top > e->top; j--)
            order[j] = order[j - 1];
        order[j] = e;
    }
}

static void _edge_step_by(twin_edge_t *edge, twin_sfixed_t dy)
{
    twin_dfixed_t e;

    e = edge->e + (twin_dfixed_t) dy * edge->dx;
    edge->x += edge->step_x * dy + edge->inc_x * (e / edge->dy);
    edge->e = e % edge->dy;
}

/* _edge_step_by(edge, TWIN_POLY_STEP) without dividing: e < dy and
 * row_e < dy, so a single correction keeps e in range */
static inline void _edge_step_row(twin_edge_t *edge)
{
    twin_dfixed_t e = edge->e + edge->row_e;
    edge->x += edge->row_x;
    if (e >= edge->dy) {
        e -= edge->dy;
        edge->x += edge->inc_x;
    }
    edge->e = e;
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
        if (edges[e].dx >= 0)
            edges[e].inc_x = 1;
        else {
            edges[e].inc_x = -1;
            edges[e].dx = -edges[e].dx;
        }
        edges[e].step_x = edges[e].inc_x * (edges[e].dx / edges[e].dy);
        edges[e].dx = edges[e].dx % edges[e].dy;
        twin_dfixed_t row = (twin_dfixed_t) TWIN_POLY_STEP * edges[e].dx;
        edges[e].row_x = edges[e].step_x * TWIN_POLY_STEP +
                         edges[e].inc_x * (row / edges[e].dy);
        edges[e].row_e = row % edges[e].dy;

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

/* Add coverage @w (at most 0x40) to @n bytes, saturating at 0xff: four
 * bytes per load and store on the aligned middle part */
static void _span_add(twin_a8_t *s, int n, uint32_t w)
{
    for (; n && ((uintptr_t) s & 3); n--, s++) {
        twin_a16_t a = *s + w;
        *s = twin_sat(a);
    }
    uint32_t ww = w | w << 16;
    for (; n >= 4; n -= 4, s += 4) {
        uint32_t v;
        memcpy(&v, s, sizeof(v));
        v = _twin_sat_lanes((v & TWIN_LANES) + ww) |
            _twin_sat_lanes(((v >> 8) & TWIN_LANES) + ww) << 8;
        memcpy(s, &v, sizeof(v));
    }
    for (; n; n--, s++) {
        twin_a16_t a = *s + w;
        *s = twin_sat(a);
    }
}

static void _span_fill(twin_pixmap_t *pixmap,
                       twin_sfixed_t y,
                       twin_sfixed_t left,
                       twin_sfixed_t right)
{
#if TWIN_POLY_SHIFT == 0
    /* 1x1 */
    static const twin_a8_t coverage[1][1] = {
        {0xff},
    };
#endif
#if TWIN_POLY_SHIFT == 1
    /* 2x2 */
    static const twin_a8_t coverage[2][2] = {
        {0x40, 0x40},
        {0x3f, 0x40},
    };
#endif
#if TWIN_POLY_SHIFT == 2
    /* 4x4 */
    static const twin_a8_t coverage[4][4] = {
        {0x10, 0x10, 0x10, 0x10},
        {0x10, 0x10, 0x10, 0x10},
        {0x0f, 0x10, 0x10, 0x10},
        {0x10, 0x10, 0x10, 0x10},
    };
#endif
#if TWIN_POLY_SHIFT == 3
    /* 8x8 */
    static const twin_a8_t coverage[8][8] = {
        {4, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4, 4, 4},
        {4, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4, 4, 4},
        {3, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4, 4, 4},
        {4, 4, 4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4, 4, 4},
    };
#endif
    const twin_a8_t *cover =
        coverage[(y >> TWIN_POLY_FIXED_SHIFT) & TWIN_POLY_MASK];
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
    if (x + TWIN_POLY_MASK < right) {
        int n = (right - x) >> TWIN_POLY_SHIFT;
        _span_add(s, n, w);
        s += n;
        x += n * TWIN_POLY_SAMPLE;
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

/* Insert @e into @aet[0..n), ordered by x, after any edge at the same x */
static inline void _aet_insert(twin_edge_t **aet, int n, twin_edge_t *e)
{
    int j = n;
    for (; j > 0 && aet[j - 1]->x > e->x; j--)
        aet[j] = aet[j - 1];
    aet[j] = e;
}

/* Scan the edges top to bottom.  The active edges live in an array kept
 * ordered by x: new edges are inserted in place, finished ones compacted
 * out, and after each step one stable insertion pass restores the order,
 * which costs a move per crossing rather than a rescan per crossing. */
/* Banding state: the band holds mask rows [top, top + rows) */
typedef struct {
    const twin_band_t *b;
    twin_pixmap_t *pixmap;
    twin_coord_t top, rows;
} _band_state_t;

/* Hand the current band over and clear it for the next rows */
static void _band_next(_band_state_t *st)
{
    twin_coord_t rows = min(st->rows, (twin_coord_t) (st->b->total - st->top));
    st->b->flush(st->b->ctx, st->top, rows);
    for (twin_coord_t r = 0; r < rows; r++)
        memset(st->pixmap->p.a8 + r * st->pixmap->stride, 0,
               (size_t) st->pixmap->width);
    st->top += st->rows;
}

static void _twin_edge_fill(twin_pixmap_t *pixmap,
                            twin_edge_t **order,
                            twin_edge_t **aet,
                            int nedges,
                            bool even_odd,
                            _band_state_t *band)
{
    twin_sfixed_t x0 = 0;
    twin_coord_t bottom = band ? band->b->total : pixmap->clip.bottom;

    if (nedges <= 0)
        return;
    _edge_sort(order, nedges);
    int e = 0, na = 0;
    twin_sfixed_t y = order[0]->top;
    /* Every edge starts below the pixmap */
    if (twin_sfixed_trunc(y) >= bottom)
        return;
    for (;;) {
        /* add in new edges */
        for (; e < nedges && order[e]->top <= y; e++)
            _aet_insert(aet, na++, order[e]);

        /* move the band down to this row */
        twin_sfixed_t by = y;
        if (band) {
            while (twin_sfixed_trunc(y) >= band->top + band->rows)
                _band_next(band);
            by = y - twin_int_to_sfixed(band->top);
        }

        /* walk this y value marking coverage: inside while the winding
         * number is nonzero, or for even-odd while its parity is odd */
        int w = 0;
        for (int i = 0; i < na; i++) {
            twin_edge_t *a = aet[i];
            if (w == 0)
                x0 = a->x;
            w = even_odd ? w ^ 1 : w + a->winding;
            if (w == 0)
                _span_fill(pixmap, by, x0, a->x);
        }

        /* step down, clipping to pixmap */
        y += TWIN_POLY_STEP;

        if (twin_sfixed_trunc(y) >= bottom)
            break;

        /* strip out dead edges */
        int k = 0;
        for (int i = 0; i < na; i++)
            if (aet[i]->bot > y)
                aet[k++] = aet[i];
        na = k;

        /* check for all done */
        if (!na && e == nedges)
            break;

        /* step all edges, then fix x sorting */
        for (int i = 0; i < na; i++)
            _edge_step_row(aet[i]);
        for (int i = 1; i < na; i++)
            if (aet[i - 1]->x > aet[i]->x)
                _aet_insert(aet, i, aet[i]);
    }
}

static void _twin_fill(twin_pixmap_t *pixmap,
                       twin_path_t *path,
                       twin_coord_t dx,
                       twin_coord_t dy,
                       twin_scratch_t *scratch,
                       _band_state_t *band)
{
    twin_sfixed_t sdx = twin_int_to_sfixed(dx + pixmap->origin_x);
    twin_sfixed_t sdy = twin_int_to_sfixed(dy + pixmap->origin_y);

    int nalloc = path->npoints + path->nsublen + 1;
    /* The edges, then two pointers per edge (sort order and active list),
     * starting on a pointer boundary */
    size_t ptr_off =
        (sizeof(twin_edge_t) * nalloc + _Alignof(twin_edge_t *) - 1) &
        ~(_Alignof(twin_edge_t *) - 1);
    size_t edge_bytes = ptr_off + 2 * sizeof(twin_edge_t *) * nalloc;
    bool from_scratch = false;
    twin_edge_t *edges = NULL;

    if (scratch)
        edges = twin_scratch_alloc(scratch, edge_bytes,
                                   _Alignof(twin_edge_t) > _Alignof(void *)
                                       ? _Alignof(twin_edge_t)
                                       : _Alignof(void *));
    if (edges) {
        from_scratch = true;
    } else {
        edges = twin_malloc(edge_bytes);
        if (!edges)
            return;
    }

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
    twin_edge_t **order = (twin_edge_t **) ((uint8_t *) edges + ptr_off);
    for (int i = 0; i < nedges; i++)
        order[i] = &edges[i];
    _twin_edge_fill(pixmap, order, order + nalloc, nedges,
                    path->state.fill_rule == TwinFillEvenOdd, band);
    if (!from_scratch)
        twin_free(edges);
}

void twin_fill_path(twin_pixmap_t *pixmap,
                    twin_path_t *path,
                    twin_coord_t dx,
                    twin_coord_t dy,
                    twin_scratch_t *scratch)
{
    _twin_fill(pixmap, path, dx, dy, scratch, NULL);
}

void twin_fill_path_banded(twin_pixmap_t *band,
                           const twin_band_t *bands,
                           twin_path_t *path,
                           twin_coord_t dx,
                           twin_coord_t dy,
                           twin_scratch_t *scratch)
{
    _band_state_t st = {bands, band, 0, band->height};
    _twin_fill(band, path, dx, dy, scratch, &st);
    /* The rest, including bands the scan never reached */
    while (st.top < bands->total)
        _band_next(&st);
}
