/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Screen compositor
 *
 * Each window keeps its pixels in a pixmap; the screen composes those
 * pixmaps scanline by scanline into a single span buffer and hands each
 * span to the backend.  There is no screen-sized buffer.
 *
 * Three things keep this cheap without graphics hardware:
 *
 *  - Damage is a short list of rectangles.  Two rectangles merge only when
 *    their union covers no more pixels than the pair, so a clock tick in the
 *    status bar does not drag the whole content area into the next frame.
 *
 *  - Occlusion culling.  Walking the layers top-down, each fully opaque
 *    layer adds its extent on the current row to a small set of covered
 *    intervals.  Every layer below is composited only where it is still
 *    visible; the background only fills what nothing covers.  A full-screen
 *    app on top of other apps therefore costs one copy per pixel.
 *
 *  - Shadows are synthesized during scan-out from a separable smoothstep
 *    profile, as a pseudo-layer directly under the casting pixmap.  They
 *    need no pixel storage and no blur pass, and the opaque pixmap above
 *    culls the shadow's interior automatically.
 */

#include <stdlib.h>
#include <string.h>

#include "twin_private.h"

unsigned _twin_debug;

#define TWIN_SHADOW_MAX_BLUR 63

twin_screen_t *twin_screen_create(twin_coord_t width,
                                  twin_coord_t height,
                                  twin_put_begin_t put_begin,
                                  twin_put_span_t put_span,
                                  void *closure)
{
    /* The window manager's state lives right after the screen */
    twin_screen_t *screen =
        twin_calloc(1, sizeof(twin_screen_t) + _twin_wm_state_size);
    if (!screen)
        return NULL;

    screen->width = width;
    screen->height = height;
    screen->background_color = 0xffffffff;
    screen->put_begin = put_begin;
    screen->put_span = put_span;
    screen->closure = closure;

#ifndef TWIN_SCRATCH_SIZE
#define TWIN_SCRATCH_SIZE (16 * 1024)
#endif
    screen->scratch_buf = twin_malloc(TWIN_SCRATCH_SIZE);
    screen->scratch_size = screen->scratch_buf ? TWIN_SCRATCH_SIZE : 0;

    _twin_wm_init(screen);
    /* Nothing has been shown yet: the first update paints everything */
    twin_screen_damage(screen, 0, 0, width, height);
    return screen;
}

void twin_screen_destroy(twin_screen_t *screen)
{
    /* Let the policy finish animations and drop pixmaps it owns; the
     * windows then go without it */
    screen->closing = true;
    _twin_wm_fini(screen);

    while (screen->windows)
        twin_window_destroy(screen->windows);
    while (screen->bottom)
        twin_pixmap_hide(screen->bottom);
    if (screen->background)
        twin_pixmap_destroy(screen->background);
    for (int i = 0; i < screen->path_cache_count; i++)
        twin_path_destroy(screen->path_cache[i]);
    if (screen->mask_cache)
        twin_pixmap_destroy(screen->mask_cache);
    twin_free(screen->span_cache);
    twin_free(screen->scratch_buf);
    twin_free(screen);
}

void twin_screen_enable_update(twin_screen_t *screen)
{
    --screen->disable;
}

void twin_screen_disable_update(twin_screen_t *screen)
{
    screen->disable++;
}

static inline twin_area_t _rect_area(const twin_rect_t *r)
{
    return (twin_area_t) (r->right - r->left) * (r->bottom - r->top);
}

static inline twin_rect_t _rect_union(const twin_rect_t *a,
                                      const twin_rect_t *b)
{
    twin_rect_t u = {
        .left = min(a->left, b->left),
        .right = max(a->right, b->right),
        .top = min(a->top, b->top),
        .bottom = max(a->bottom, b->bottom),
    };
    return u;
}

static inline bool _rect_contains(const twin_rect_t *outer,
                                  const twin_rect_t *inner)
{
    return outer->left <= inner->left && inner->right <= outer->right &&
           outer->top <= inner->top && inner->bottom <= outer->bottom;
}

void _twin_rect_list_add(twin_rect_t *list, int *n, int max, twin_rect_t r)
{
    for (;;) {
        bool merged = false;
        for (int i = 0; i < *n; i++) {
            twin_rect_t *d = &list[i];
            if (_rect_contains(d, &r))
                return;
            twin_rect_t u = _rect_union(d, &r);
            if (_rect_area(&u) <= _rect_area(d) + _rect_area(&r)) {
                /* Merged: re-insert so it can absorb further neighbors */
                r = u;
                list[i] = list[--*n];
                merged = true;
                break;
            }
        }
        if (merged)
            continue;
        if (*n < max) {
            list[(*n)++] = r;
            return;
        }
        int best = 0;
        twin_area_t best_cost = 0;
        for (int i = 0; i < *n; i++) {
            twin_rect_t u = _rect_union(&list[i], &r);
            twin_area_t cost = _rect_area(&u) - _rect_area(&list[i]);
            if (i == 0 || cost < best_cost) {
                best = i;
                best_cost = cost;
            }
        }
        r = _rect_union(&list[best], &r);
        list[best] = list[--*n];
    }
}

void twin_screen_damage(twin_screen_t *screen,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom)
{
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > screen->width)
        right = screen->width;
    if (bottom > screen->height)
        bottom = screen->height;
    if (left >= right || top >= bottom)
        return;

    _twin_rect_list_add(screen->damage, &screen->n_damage, TWIN_DAMAGE_RECTS,
                        (twin_rect_t) {left, right, top, bottom});
}

void twin_screen_resize(twin_screen_t *screen,
                        twin_coord_t width,
                        twin_coord_t height)
{
    if (width == screen->width && height == screen->height)
        return;
    screen->width = width;
    screen->height = height;
    screen->n_damage = 0;
    twin_screen_disable_update(screen);
    /* Rotation or a resized host window: the policy re-places everything */
    _twin_screen_relayout(screen);
    twin_screen_damage(screen, 0, 0, screen->width, screen->height);
    twin_screen_enable_update(screen);
}

bool twin_screen_damaged(twin_screen_t *screen)
{
    return screen->n_damage > 0;
}

/*
 * Interval sets for occlusion: sorted, disjoint, half-open [l, r).
 * Capacity overflow only makes culling more conservative, never wrong.
 */
#define TWIN_SPAN_SET_MAX 8

typedef struct {
    twin_coord_t l, r;
} twin_span_t;

typedef struct {
    int n;
    twin_span_t s[TWIN_SPAN_SET_MAX];
} twin_span_set_t;

static void _span_set_add(twin_span_set_t *set, twin_coord_t l, twin_coord_t r)
{
    if (l >= r)
        return;
    /* Merge with every interval that touches [l, r). */
    int i = 0, o = 0;
    twin_span_t out[TWIN_SPAN_SET_MAX + 1];
    bool placed = false;
    for (; i < set->n; i++) {
        twin_span_t s = set->s[i];
        if (s.r < l) {
            out[o++] = s;
        } else if (r < s.l) {
            if (!placed) {
                out[o++] = (twin_span_t) {l, r};
                placed = true;
            }
            out[o++] = s;
        } else {
            l = min(l, s.l);
            r = max(r, s.r);
        }
    }
    if (!placed)
        out[o++] = (twin_span_t) {l, r};
    if (o > TWIN_SPAN_SET_MAX)
        return; /* keep the old (smaller) set: conservative */
    memcpy(set->s, out, sizeof(out[0]) * o);
    set->n = o;
}

/* Emit the parts of [l, r) not covered by @set; returns pieces written. */
static int _span_set_subtract(const twin_span_set_t *set,
                              twin_coord_t l,
                              twin_coord_t r,
                              twin_span_t *out,
                              int max_out)
{
    int n = 0;
    for (int i = 0; i < set->n && l < r; i++) {
        const twin_span_t *s = &set->s[i];
        if (s->r <= l)
            continue;
        if (s->l >= r)
            break;
        if (s->l > l) {
            if (n == max_out)
                return -1;
            out[n++] = (twin_span_t) {l, s->l};
        }
        l = max(l, s->r);
    }
    if (l < r) {
        if (n == max_out)
            return -1;
        out[n++] = (twin_span_t) {l, r};
    }
    return n;
}

#if defined(CONFIG_DROP_SHADOW)
/*
 * Shadow profile: smoothstep ramp across 2 * blur + 1 pixels, in 1/256
 * units.  One entry is cached; radii rarely change between rows.
 */
static uint16_t shadow_ramp[2 * TWIN_SHADOW_MAX_BLUR + 1];
static int shadow_ramp_blur = -1;

static const uint16_t *_shadow_ramp(int blur)
{
    if (blur != shadow_ramp_blur) {
        int n = 2 * blur;
        for (int i = 0; i <= n; i++) {
            /* t in 16.16, smoothstep 3t^2 - 2t^3 */
            int64_t t = twin_fixed_div(i, n ? n : 1);
            int64_t t2 = (t * t) >> 16;
            int64_t t3 = (t2 * t) >> 16;
            int64_t v = 3 * t2 - 2 * t3;
            shadow_ramp[i] = (uint16_t) ((v * 256 + 0x8000) >> 16);
        }
        shadow_ramp_blur = blur;
    }
    return shadow_ramp;
}

/* Coverage of a blurred box edge pair at position @v, 0..256 */
static inline int _shadow_cover(const uint16_t *ramp,
                                int blur,
                                twin_coord_t v,
                                twin_coord_t lo,
                                twin_coord_t hi)
{
    int n = 2 * blur + 1;
    int rise = v - (lo - blur);
    int fall = (hi + blur - 1) - v;
    int a = rise < 0 ? 0 : rise >= n ? 256 : ramp[rise];
    int b = fall < 0 ? 0 : fall >= n ? 256 : ramp[fall];
    int c = a + b - 256;
    return c < 0 ? 0 : c;
}

static inline twin_rect_t _shadow_rect(const twin_pixmap_t *p)
{
    twin_rect_t r = {
        .left = p->x + p->shadow.dx,
        .right = p->x + p->width + p->shadow.dx,
        .top = p->y + p->shadow.dy,
        .bottom = p->y + p->scan_height + p->shadow.dy,
    };
    return r;
}

#endif /* CONFIG_DROP_SHADOW */

twin_rect_t _twin_pixmap_extents(const twin_pixmap_t *p)
{
    twin_rect_t r = {p->x, p->x + p->width, p->y, p->y + p->scan_height};
#if defined(CONFIG_DROP_SHADOW)
    if (p->shadow.blur) {
        int b = min((int) p->shadow.blur, TWIN_SHADOW_MAX_BLUR);
        twin_rect_t s = _shadow_rect(p);
        s.left -= b;
        s.right += b;
        s.top -= b;
        s.bottom += b;
        r = _rect_union(&r, &s);
    }
#endif
    return r;
}

#if defined(CONFIG_DROP_SHADOW)
/* Premultiplied black at coverage @a (0..255) over dst: color scales,
 * coverage adds.  Two channels per multiply, rounding as twin_int_mult. */
static inline twin_argb32_t _shadow_over(twin_argb32_t v, uint32_t a)
{
    uint32_t k = 255 - a;
    uint32_t rb = _twin_mul_lanes(v & TWIN_LANES, k);
    uint32_t ag = _twin_mul_lanes((v >> 8) & TWIN_LANES, k);
    uint32_t alpha = a + (ag >> 16);
    return rb | (ag & 0xff) << 8 | (alpha > 255 ? 255 : alpha) << 24;
}

static void _shadow_span(twin_argb32_t *span,
                         twin_coord_t span_left,
                         const twin_pixmap_t *p,
                         twin_coord_t y,
                         twin_coord_t l,
                         twin_coord_t r)
{
    int blur = min((int) p->shadow.blur, TWIN_SHADOW_MAX_BLUR);
    const uint16_t *ramp = _shadow_ramp(blur);
    twin_rect_t s = _shadow_rect(p);
    int fy = _shadow_cover(ramp, blur, y, s.top, s.bottom);
    if (!fy)
        return;
    uint32_t row_alpha = (uint32_t) p->shadow.alpha * (uint32_t) fy;

    /* Between the two edge ramps both saturate and the horizontal coverage
     * is exactly 256 (see _shadow_cover), so the alpha is constant there */
    twin_coord_t flat_l = s.left + blur + 1, flat_r = s.right - blur - 1;
    uint32_t flat_a = (row_alpha * 256) >> 16;

    twin_argb32_t *d = span + (l - span_left);
    for (twin_coord_t x = l; x < r;) {
        if (x >= flat_l && x < flat_r) {
            twin_coord_t end = min(r, flat_r);
            if (flat_a) {
                for (; x < end; x++, d++)
                    *d = _shadow_over(*d, flat_a);
            } else {
                d += end - x;
                x = end;
            }
            continue;
        }
        int fx = _shadow_cover(ramp, blur, x, s.left, s.right);
        uint32_t a = (row_alpha * (uint32_t) fx) >> 16;
        if (a)
            *d = _shadow_over(*d, a);
        x++;
        d++;
    }
}
#else
static inline void _shadow_span(twin_argb32_t *span,
                                twin_coord_t span_left,
                                const twin_pixmap_t *p,
                                twin_coord_t y,
                                twin_coord_t l,
                                twin_coord_t r)
{
    (void) span, (void) span_left, (void) p, (void) y, (void) l, (void) r;
}
#endif

/*
 * One layer as seen by a single scanline.  Shadows appear as their own
 * entry directly below the pixmap casting them.
 */
#define TWIN_ROW_LAYERS 32
#define TWIN_ROW_PIECES 64

typedef struct {
    twin_pixmap_t *p;
    bool shadow;
    bool opaque_full; /* every pixel on this row is opaque: use copy */
    twin_coord_t l, r;
    int first, count; /* visible pieces in the piece pool */
} twin_row_layer_t;

static inline bool _layer_hidden(const twin_pixmap_t *p)
{
    return !p->opacity || _twin_pixmap_is_stub(p);
}

static void _composite_layer(twin_argb32_t *span,
                             twin_coord_t span_left,
                             const twin_row_layer_t *layer,
                             twin_coord_t y,
                             twin_coord_t l,
                             twin_coord_t r)
{
    twin_pixmap_t *p = layer->p;
    twin_pointer_t src = twin_pixmap_pointer(p, l - p->x, y - p->y);
    twin_argb32_t *dst = span + (l - span_left);
    int w = r - l;

    if (p->format == TWIN_RGB16) {
        if (p->opacity == 0xff) {
            twin_source_u s = {.p = src};
            _twin_rgb16_source_argb32((twin_pointer_t) {.argb32 = dst}, s, w);
        } else {
            _twin_screen_blend_rgb16(dst, src.rgb16, p->opacity, w);
        }
    } else if (p->format == TWIN_ARGB32) {
        twin_source_u s = {.p = src};
        if (p->opacity != 0xff)
            _twin_screen_blend_argb32(dst, src.argb32, p->opacity, w);
        else if (layer->opaque_full)
            memcpy(dst, src.argb32, (size_t) w * sizeof(*dst));
        else
            _twin_argb32_over_argb32((twin_pointer_t) {.argb32 = dst}, s, w);
    } else {
        /* A8 pixmaps on screen draw as black coverage */
        for (int i = 0; i < w; i++) {
            /* v / 255 for v <= 255 * 255, without a divide */
            uint32_t cov = (uint32_t) src.a8[i] * p->opacity;
            uint32_t a = (cov + 1 + (cov >> 8)) >> 8;
            uint32_t k = 255 - a;
            uint16_t t0, t1, t2, t3;
            twin_argb32_t v = dst[i];
            dst[i] = (twin_in(v, 0, k, t0) | twin_in(v, 8, k, t1) |
                      twin_in(v, 16, k, t2) | twin_in(v, 24, k, t3)) +
                     (a << 24);
        }
    }
}

static void _background_span(twin_screen_t *screen,
                             twin_argb32_t *span,
                             twin_coord_t span_left,
                             twin_coord_t y,
                             twin_coord_t l,
                             twin_coord_t r)
{
    twin_pixmap_t *bg = screen->background;
    if (!bg) {
        _twin_screen_fill(span + (l - span_left), screen->background_color,
                          r - l);
        return;
    }
    twin_coord_t p_width = bg->width;
    twin_coord_t p_y = y % bg->height;
    twin_coord_t p_this;
    for (twin_coord_t x = l; x < r; x += p_this) {
        twin_coord_t m_left = x % p_width;
        p_this = p_width - m_left;
        if (x + p_this > r)
            p_this = r - x;
        twin_pointer_t dst = {.argb32 = span + (x - span_left)};
        twin_source_u src = {.p = twin_pixmap_pointer(bg, m_left, p_y)};
        if (bg->format == TWIN_RGB16)
            _twin_rgb16_source_argb32(dst, src, p_this);
        else
            _twin_argb32_source_argb32(dst, src, p_this);
    }
}

/* Is @p, on row @y, opaque across [l, r)?  Returns the opaque span. */
static bool _layer_opaque_span(const twin_pixmap_t *p,
                               twin_coord_t y,
                               twin_coord_t *ol,
                               twin_coord_t * or)
{
    if (p->opacity != 0xff || (_twin_debug & TWIN_DEBUG_NO_OCCLUSION))
        return false;
    const twin_rect_t *o = &p->opaque_rect;
    if (o->left >= o->right)
        return false;
    if (y < p->y + o->top || y >= p->y + o->bottom)
        return false;
    *ol = p->x + o->left;
    *or = p->x + o->right;
    return true;
}

static void _composite_row(twin_screen_t *screen,
                           twin_argb32_t *span,
                           twin_coord_t y,
                           twin_coord_t left,
                           twin_coord_t right)
{
    twin_row_layer_t layers[TWIN_ROW_LAYERS];
    twin_span_t pieces[TWIN_ROW_PIECES];
    twin_span_set_t covered = {0};
    int n_layers = 0, n_pieces = 0;
    bool overflow = (_twin_debug & TWIN_DEBUG_ROW_FALLBACK) != 0;

    /* Collect layers top-down: cursor, then pixmaps (each before its
     * shadow). */
    twin_pixmap_t *top = screen->top;
#if defined(CONFIG_CURSOR)
    twin_pixmap_t *cursor = screen->cursor;
#else
    twin_pixmap_t *cursor = NULL;
#endif
    for (twin_pixmap_t *p = overflow ? NULL
                            : cursor ? cursor
                                     : top;
         p; p = (p == cursor) ? top : p->down) {
        if (_layer_hidden(p))
            continue;
        for (int pass = 0; pass < 2; pass++) {
            twin_coord_t l, r;
            bool is_shadow = pass == 1;
            if (!is_shadow) {
                if (y < p->y || y >= p->y + p->scan_height)
                    continue;
                l = p->x;
                r = p->x + p->width;
            } else {
                if (!_twin_pixmap_shadowed(p))
                    break;
                twin_rect_t e = _twin_pixmap_extents(p);
                if (y < e.top || y >= e.bottom)
                    break;
                l = e.left;
                r = e.right;
            }
            l = max(l, left);
            r = min(r, right);
            if (l >= r)
                continue;
            if (n_layers == TWIN_ROW_LAYERS) {
                overflow = true;
                break;
            }
            twin_row_layer_t *ly = &layers[n_layers++];
            ly->p = p;
            ly->shadow = is_shadow;
            ly->l = l;
            ly->r = r;
            ly->opaque_full = false;

            /* Visible part = layer extent minus everything opaque above */
            int room = TWIN_ROW_PIECES - n_pieces;
            int got =
                _span_set_subtract(&covered, l, r, pieces + n_pieces, room);
            if (got < 0) {
                if (room < 1) {
                    overflow = true;
                    break;
                }
                pieces[n_pieces] = (twin_span_t) {l, r}; /* conservative */
                got = 1;
            }
            ly->first = n_pieces;
            ly->count = got;
            n_pieces += got;

            twin_coord_t ol, or ;
            if (!is_shadow && _layer_opaque_span(p, y, &ol, &or)) {
                ly->opaque_full = (ol <= p->x && p->x + p->width <= or);
                _span_set_add(&covered, max(ol, left), min(or, right));
            }
        }
        if (overflow)
            break;
        if (p == cursor && !top)
            break;
    }

    if (overflow) {
        /* Too many layers on this row: composite everything bottom-up. */
        _background_span(screen, span, left, y, left, right);
        for (twin_pixmap_t *p = screen->bottom; p; p = p->up) {
            if (_layer_hidden(p))
                continue;
            if (_twin_pixmap_shadowed(p)) {
                twin_rect_t e = _twin_pixmap_extents(p);
                if (y >= e.top && y < e.bottom)
                    _shadow_span(span, left, p, y, max(e.left, left),
                                 min(e.right, right));
            }
            if (y < p->y || y >= p->y + p->scan_height)
                continue;
            twin_row_layer_t ly = {.p = p};
            twin_coord_t l = max(p->x, left);
            twin_coord_t r = min((twin_coord_t) (p->x + p->width), right);
            if (l < r)
                _composite_layer(span, left, &ly, y, l, r);
        }
#if defined(CONFIG_CURSOR)
        if (cursor && y >= cursor->y && y < cursor->y + cursor->height) {
            twin_row_layer_t ly = {.p = cursor};
            twin_coord_t l = max(cursor->x, left);
            twin_coord_t r =
                min((twin_coord_t) (cursor->x + cursor->width), right);
            if (l < r)
                _composite_layer(span, left, &ly, y, l, r);
        }
#endif
        screen->stats.layer_pixels += right - left;
        return;
    }

    /* Background where nothing opaque lands */
    {
        twin_span_t bg[TWIN_SPAN_SET_MAX + 1];
        int n = _span_set_subtract(&covered, left, right, bg,
                                   TWIN_SPAN_SET_MAX + 1);
        for (int i = 0; i < n; i++)
            _background_span(screen, span, left, y, bg[i].l, bg[i].r);
    }

    /* Bottom-up over the visible pieces */
    for (int i = n_layers - 1; i >= 0; i--) {
        twin_row_layer_t *ly = &layers[i];
        twin_coord_t visible = 0;
        for (int k = 0; k < ly->count; k++) {
            twin_span_t *pc = &pieces[ly->first + k];
            visible += pc->r - pc->l;
            if (ly->shadow) {
                _shadow_span(span, left, ly->p, y, pc->l, pc->r);
                screen->stats.shadow_pixels += pc->r - pc->l;
            } else {
                _composite_layer(span, left, ly, y, pc->l, pc->r);
            }
        }
        if (!ly->shadow) {
            screen->stats.layer_pixels += visible;
            screen->stats.occluded_pixels += (ly->r - ly->l) - visible;
        }
    }
}

void twin_screen_update(twin_screen_t *screen)
{
    if (screen->disable || !screen->n_damage)
        return;

    /* Size the span buffer first: if that fails, the damage stays queued
     * for the next update instead of being lost. */
    int n = screen->n_damage;
    twin_coord_t max_width = 0;
    for (int i = 0; i < n; i++)
        max_width = max(max_width, (twin_coord_t) (screen->damage[i].right -
                                                   screen->damage[i].left));
    if (!screen->span_cache || screen->span_cache_width < max_width) {
        twin_argb32_t *c =
            twin_realloc(screen->span_cache, max_width * sizeof(twin_argb32_t));
        if (!c)
            return;
        screen->span_cache = c;
        screen->span_cache_width = max_width;
    }
    twin_argb32_t *span = screen->span_cache;

    /* Snapshot and clear before scan-out: backends may damage meanwhile. */
    twin_rect_t rects[TWIN_DAMAGE_RECTS];
    memcpy(rects, screen->damage, sizeof(rects[0]) * n);
    screen->n_damage = 0;

    screen->stats.frames++;
    for (int i = 0; i < n; i++) {
        twin_coord_t left = rects[i].left, right = rects[i].right;
        twin_coord_t top = rects[i].top, bottom = rects[i].bottom;
        right = min(right, screen->width);
        bottom = min(bottom, screen->height);
        if (left >= right || top >= bottom)
            continue;
        screen->stats.rects++;
        if (screen->put_begin)
            (*screen->put_begin)(left, top, right, bottom, screen->closure);
        for (twin_coord_t y = top; y < bottom; y++) {
            _composite_row(screen, span, y, left, right);
            (*screen->put_span)(left, y, right, span, screen->closure);
        }
        screen->stats.pixels += (uint64_t) (right - left) * (bottom - top);
    }
}

void twin_screen_set_active(twin_screen_t *screen, twin_pixmap_t *pixmap)
{
    twin_event_t ev = {0};
    twin_pixmap_t *old = screen->active;
    if (old == pixmap)
        return;
    screen->active = pixmap;
    /* Handlers run under the busy guard */
    _twin_screen_busy(screen);
    if (old) {
        ev.kind = TwinEventDeactivate;
        twin_pixmap_dispatch(old, &ev);
    }
    /* The deactivated window may have destroyed or replaced the new one */
    if (pixmap && screen->active == pixmap) {
        ev.kind = TwinEventActivate;
        twin_pixmap_dispatch(pixmap, &ev);
    }
    _twin_screen_idle(screen);
}

void twin_screen_set_background(twin_screen_t *screen, twin_pixmap_t *pixmap)
{
    if (screen->background)
        twin_pixmap_destroy(screen->background);
    screen->background = pixmap;
    twin_screen_damage(screen, 0, 0, screen->width, screen->height);
}

#if defined(CONFIG_CURSOR)
static void twin_screen_damage_cursor(twin_screen_t *screen)
{
    twin_screen_damage(screen, screen->cursor->x, screen->cursor->y,
                       screen->cursor->x + screen->cursor->width,
                       screen->cursor->y + screen->cursor->height);
}

void twin_screen_set_cursor(twin_screen_t *screen,
                            twin_pixmap_t *pixmap,
                            twin_coord_t hotspot_x,
                            twin_coord_t hotspot_y)
{
    twin_screen_disable_update(screen);

    if (screen->cursor)
        twin_screen_damage_cursor(screen);

    screen->cursor = pixmap;
    screen->curs_hx = hotspot_x;
    screen->curs_hy = hotspot_y;
    if (pixmap) {
        pixmap->x = screen->curs_x - hotspot_x;
        pixmap->y = screen->curs_y - hotspot_y;
        twin_screen_damage_cursor(screen);
    }

    twin_screen_enable_update(screen);
}

static void twin_screen_update_cursor(twin_screen_t *screen,
                                      twin_coord_t x,
                                      twin_coord_t y)
{
    twin_screen_disable_update(screen);

    if (screen->cursor)
        twin_screen_damage_cursor(screen);

    screen->curs_x = x;
    screen->curs_y = y;

    if (screen->cursor) {
        screen->cursor->x = screen->curs_x - screen->curs_hx;
        screen->cursor->y = screen->curs_y - screen->curs_hy;
        twin_screen_damage_cursor(screen);
    }

    twin_screen_enable_update(screen);
}
#endif /* CONFIG_CURSOR */

static void _twin_adj_mouse_evt(twin_event_t *event,
                                const twin_pixmap_t *pixmap)
{
    event->u.pointer.x = event->u.pointer.screen_x - pixmap->x;
    event->u.pointer.y = event->u.pointer.screen_y - pixmap->y;
}

static twin_pixmap_t *_twin_screen_pick(twin_screen_t *screen,
                                        twin_coord_t x,
                                        twin_coord_t y)
{
    for (twin_pixmap_t *p = screen->top; p; p = p->down) {
        if (p->window && _twin_window_hit(p->window, x, y))
            return p;
    }
    return NULL;
}

void _twin_screen_busy(twin_screen_t *screen)
{
    screen->dispatching++;
}

void _twin_screen_idle(twin_screen_t *screen)
{
    if (--screen->dispatching)
        return;
    /* Nobody can hold a destroyed window any more */
    while (screen->zombies) {
        twin_window_t *w = screen->zombies;
        screen->zombies = w->zombie_next;
        twin_free(w);
    }
}

/* Send @event to @pixmap as @kind, in @pixmap's coordinates */
static bool _twin_screen_send(twin_pixmap_t *pixmap,
                              const twin_event_t *event,
                              twin_event_kind_t kind)
{
    twin_event_t evt = *event;
    evt.kind = kind;
    _twin_adj_mouse_evt(&evt, pixmap);
    return twin_pixmap_dispatch(pixmap, &evt);
}

/* Point screen->target at the window under the pointer, sending Leave and
 * Enter.  Handlers may destroy windows: the old target is re-checked
 * through screen->target (destroying a pixmap clears it) and the new one
 * is picked again after Leave. */
static void _twin_screen_hover(twin_screen_t *screen, const twin_event_t *event)
{
    twin_coord_t x = event->u.pointer.screen_x, y = event->u.pointer.screen_y;
    twin_pixmap_t *ntarget = _twin_screen_pick(screen, x, y);
    if (screen->target == ntarget)
        return;
    if (screen->target) {
        _twin_screen_send(screen->target, event, TwinEventLeave);
        ntarget = _twin_screen_pick(screen, x, y);
    }
    screen->target = ntarget;
    if (ntarget)
        _twin_screen_send(ntarget, event, TwinEventEnter);
}

bool _twin_screen_route(twin_screen_t *screen, twin_event_t *event)
{
    /* A grab sends everything up to the release to the pressed window */
    if (screen->clicklock) {
        twin_pixmap_t *grab = screen->target;
        if (event->kind != TwinEventButtonUp)
            return grab && _twin_screen_send(grab, event, event->kind);
        /* The release ends the grab: deliver it where the press went, then
         * let hover catch up with the pointer */
        screen->clicklock = 0;
        bool handled = grab && _twin_screen_send(grab, event, event->kind);
        _twin_screen_hover(screen, event);
        return handled;
    }

    _twin_screen_hover(screen, event);
    twin_pixmap_t *pixmap = screen->target;
    if (!pixmap)
        return false;
    if (event->kind == TwinEventButtonDown)
        screen->clicklock = 1;
    return _twin_screen_send(pixmap, event, event->kind);
}

static bool _twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event);

bool twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event)
{
    /* Handlers may destroy windows; they are freed once we are out */
    _twin_screen_busy(screen);
    bool handled = _twin_screen_dispatch(screen, event);
    _twin_screen_idle(screen);
    return handled;
}

static bool _twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event)
{
    if (screen->event_filter && screen->event_filter(screen, event))
        return true;

    switch (event->kind) {
    case TwinEventMotion:
    case TwinEventButtonDown:
    case TwinEventButtonUp:
#if defined(CONFIG_CURSOR)
        twin_screen_update_cursor(screen, event->u.pointer.screen_x,
                                  event->u.pointer.screen_y);
#endif
        /* Window manager gestures see pointer events first */
        if (_twin_wm_filter(screen, event))
            return true;
        return _twin_screen_route(screen, event);
    case TwinEventKeyDown:
    case TwinEventKeyUp:
    case TwinEventUcs4:
        return screen->active && twin_pixmap_dispatch(screen->active, event);
    default:
        return false;
    }
}

twin_coord_t twin_screen_width(const twin_screen_t *screen)
{
    return screen->width;
}

twin_coord_t twin_screen_height(const twin_screen_t *screen)
{
    return screen->height;
}

const twin_screen_stats_t *twin_screen_stats(const twin_screen_t *screen)
{
    return &screen->stats;
}
