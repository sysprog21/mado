/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Core rendering tests: compositor, coverage cache, text, gradients,
 * tweens, and object lifetimes.  Optimizations are checked differentially:
 * the optimized path must produce exactly the bytes of the plain one.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"

#define W 160
#define H 120

static twin_pixmap_t *solid(twin_format_t fmt,
                            twin_coord_t w,
                            twin_coord_t h,
                            twin_argb32_t color)
{
    twin_pixmap_t *p = twin_pixmap_create(fmt, w, h);
    twin_fill(p, color, TWIN_SOURCE, 0, 0, w, h);
    return p;
}

#if defined(CONFIG_DROP_SHADOW)
/* The shadow shows only outside its window: the shape picks what casts it,
 * the window's alpha masks it (opacity included, RGB16 too), and narrow
 * damage leaves pieces wholly beside the window that must stay in bounds */
static void test_shadow_mask(void)
{
    th_t *t = th_create(W, H);
    t->screen->background_color = 0xffffffff;
    twin_screen_damage(t->screen, 0, 0, W, H);

    /* Left half solid, right half clear; only the solid half casts */
    twin_pixmap_t *glass = solid(TWIN_ARGB32, 40, 40, 0x00000000);
    twin_fill(glass, 0xff000080, TWIN_SOURCE, 0, 0, 20, 40);
    twin_pixmap_move(glass, 40, 40);
    twin_pixmap_set_shadow(glass, 0, 4, 6, 0xc0);
    twin_pixmap_set_shadow_cast(glass, 0, (twin_rect_t) {0, 20, 0, 40});
    twin_pixmap_show(glass, t->screen, NULL);
    th_frame(t);
    CHECK_EQ(th_pixel(t, 50, 60), 0xff000080, "no shadow over the window");
    CHECK((th_pixel(t, 62, 60) & 0xff) < 0xff,
          "shadow shows through the clear half beside the caster");
    CHECK_EQ(th_pixel(t, 75, 60), 0xffffffff,
             "clear half past the blur: no shadow from the bounding box");

    /* Faded RGB16 is not opaque: part of its own shadow shows through */
    twin_pixmap_t *rgb = solid(TWIN_RGB16, 30, 30, 0xff808080);
    twin_pixmap_move(rgb, 120, 40);
    twin_pixmap_set_opacity(rgb, 0x80);
    twin_pixmap_show(rgb, t->screen, NULL);
    th_frame(t);
    twin_argb32_t plain = th_pixel(t, 135, 55);
    twin_pixmap_set_shadow(rgb, 0, 0, 6, 0xc0);
    th_frame(t);
    CHECK((th_pixel(t, 135, 55) & 0xff) < (plain & 0xff),
          "half-faded RGB16 lets its shadow through: %08x vs %08x",
          th_pixel(t, 135, 55), plain);

    /* An A8 pixmap masks by its bytes */
    twin_pixmap_t *a8 = solid(TWIN_A8, 8, 8, 0x80000000);
    twin_pixmap_move(a8, 60, 110);
    twin_pixmap_set_shadow(a8, 2, 2, 4, 0xc0);
    twin_pixmap_show(a8, t->screen, NULL);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "A8 shadow consistent");

    /* Narrow strips: pieces left or right of a pixmap, never past them */
    for (twin_coord_t x = 0; x < W; x += 3) {
        twin_screen_damage(t->screen, x, 0, x + 2, H);
        th_frame(t);
    }
    CHECK_EQ(th_damage_check(t), 0, "strip damage matches a full repaint");

    twin_pixmap_destroy(a8);
    twin_pixmap_destroy(rgb);
    twin_pixmap_destroy(glass);
    th_destroy(t);
}
#endif

static void test_damage_merge(void)
{
    th_t *t = th_create(W, H);
    twin_screen_t *s = t->screen;

    twin_screen_damage(s, 0, 0, 10, 10);
    twin_screen_damage(s, 100, 100, 110, 110);
    CHECK_EQ(s->n_damage, 2, "disjoint rectangles stay apart");

    twin_screen_damage(s, 10, 0, 20, 10); /* edge-adjacent: free merge */
    CHECK_EQ(s->n_damage, 2, "adjacent rectangle merges");
    twin_screen_damage(s, 2, 2, 8, 8); /* contained */
    CHECK_EQ(s->n_damage, 2, "contained rectangle absorbed");

    bool merged = false;
    for (int i = 0; i < s->n_damage; i++)
        if (s->damage[i].left == 0 && s->damage[i].right == 20)
            merged = true;
    CHECK(merged, "adjacent rectangles become one 20x10 rectangle");

    /* Overflow folds into the cheapest neighbor, never drops damage */
    for (int i = 0; i < 20; i++)
        twin_screen_damage(s, i * 7, (i * 13) % 100, i * 7 + 3,
                           (i * 13) % 100 + 3);
    CHECK(s->n_damage <= TWIN_DAMAGE_RECTS, "damage list bounded");
    twin_screen_update(s);
    CHECK_EQ(s->n_damage, 0, "update clears damage");
    th_destroy(t);
}

/* A scene that exercises every compositor path */
static void build_scene(th_t *t, twin_pixmap_t **out, int *n)
{
    twin_screen_t *s = t->screen;
    int k = 0;
    twin_pixmap_t *bg = solid(TWIN_RGB16, W, H, 0xff336699); /* opaque */
    twin_pixmap_show(bg, s, NULL);
    out[k++] = bg;

    twin_pixmap_t *card = solid(TWIN_ARGB32, 80, 60, 0xffeeeeee);
    card->opaque_rect = (twin_rect_t) {0, 80, 0, 60};
    twin_pixmap_move(card, 20, 20);
    twin_pixmap_set_shadow(card, 3, 4, 8, 0x90);
    twin_pixmap_show(card, s, bg);
    out[k++] = card;

    twin_pixmap_t *glass = solid(TWIN_ARGB32, 60, 60, 0x80800000); /* 50% */
    twin_pixmap_move(glass, 70, 40);
    twin_pixmap_show(glass, s, card);
    out[k++] = glass;

    twin_pixmap_t *faded = solid(TWIN_RGB16, 40, 40, 0xff00ff00);
    twin_pixmap_move(faded, 110, 10);
    twin_pixmap_set_opacity(faded, 0x60);
    twin_pixmap_show(faded, s, glass);
    out[k++] = faded;

    twin_pixmap_t *top = solid(TWIN_ARGB32, 30, 100, 0xff202020);
    top->opaque_rect = (twin_rect_t) {0, 30, 0, 100};
    twin_pixmap_move(top, 5, 10);
    twin_pixmap_set_shadow(top, -2, 0, 5, 0xff);
    twin_pixmap_show(top, s, faded);
    out[k++] = top;
    *n = k;
}

static void test_occlusion_differential(void)
{
    th_t *t = th_create(W, H);
    twin_pixmap_t *p[8];
    int n;
    build_scene(t, p, &n);

    twin_screen_damage(t->screen, 0, 0, W, H);
    t->screen->stats = (twin_screen_stats_t) {0};
    twin_screen_update(t->screen);
    uint32_t fast = th_crc(t);
    twin_screen_stats_t on = t->screen->stats;

    _twin_debug |= TWIN_DEBUG_NO_OCCLUSION;
    twin_screen_damage(t->screen, 0, 0, W, H);
    t->screen->stats = (twin_screen_stats_t) {0};
    twin_screen_update(t->screen);
    _twin_debug &= ~TWIN_DEBUG_NO_OCCLUSION;
    uint32_t slow = th_crc(t);
    twin_screen_stats_t off = t->screen->stats;

    CHECK(fast == slow, "occlusion culling changed pixels: %08x vs %08x", fast,
          slow);
    CHECK(on.occluded_pixels > 0, "opaque layers occluded something");
    CHECK(on.layer_pixels < off.layer_pixels,
          "culling composites fewer pixels (%llu vs %llu)",
          (unsigned long long) on.layer_pixels,
          (unsigned long long) off.layer_pixels);
    th_golden("core", "scene", fast, t);

    /* Partial damage must agree with the full frame */
    twin_pixmap_move(p[2], 75, 45);
    twin_pixmap_set_opacity(p[3], 0xc0);
    twin_pixmap_set_shadow(p[1], 6, 6, 12, 0x60);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "stale pixels after moves");

    for (int i = n - 1; i >= 0; i--)
        twin_pixmap_destroy(p[i]);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "stale pixels after teardown");
    th_destroy(t);
}

/* Full frame with extra debug switches; flags and stats are restored */
static uint32_t full_frame(th_t *t, unsigned debug)
{
    unsigned saved = _twin_debug;
    twin_screen_stats_t stats = t->screen->stats;
    _twin_debug |= debug;
    twin_screen_damage(t->screen, 0, 0, W, H);
    twin_screen_update(t->screen);
    _twin_debug = saved;
    t->screen->stats = stats;
    return th_crc(t);
}

/*
 * Rows with more layers than the culled path can track fall back to plain
 * bottom-up compositing.  Both paths must give the same pixels: compare
 * them on the shared scene, then on a stack deep enough to overflow.
 */
static void test_row_fallback(void)
{
    th_t *t = th_create(W, H);
    twin_pixmap_t *p[8];
    int n;
    build_scene(t, p, &n);
    uint32_t culled = full_frame(t, 0);
    CHECK(full_frame(t, TWIN_DEBUG_ROW_FALLBACK) == culled,
          "fallback path matches the culled path");

    enum { DEEP = 40 };
    twin_pixmap_t *deep[DEEP];
    twin_pixmap_t *below = p[n - 1];
    for (int i = 0; i < DEEP; i++) {
        bool opaque = i % 5 == 0;
        deep[i] = solid(i % 2 ? TWIN_RGB16 : TWIN_ARGB32, 60, 50,
                        opaque ? 0xff000000 | (0x050403u * i)
                               : 0x40000000 | (0x030507u * i));
        if (opaque)
            deep[i]->opaque_rect = (twin_rect_t) {0, 60, 0, 50};
        if (i % 7 == 3)
            twin_pixmap_set_shadow(deep[i], 2, 2, 4, 0x80);
        if (i % 6 == 1)
            twin_pixmap_set_opacity(deep[i], 0xa0);
        twin_pixmap_move(deep[i], 10 + i, 30 + (i % 4)); /* all overlap */
        twin_pixmap_show(deep[i], t->screen, below);
        below = deep[i];
    }
    uint32_t natural = full_frame(t, 0);
    CHECK(full_frame(t, TWIN_DEBUG_ROW_FALLBACK) == natural,
          "deep stack: overflowing rows match the forced fallback");
    CHECK_EQ(th_damage_check(t), 0, "deep stack: culling off agrees");
    twin_pixmap_move(deep[DEEP / 2], 90, 80);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "deep stack: partial damage consistent");

    for (int i = DEEP - 1; i >= 0; i--)
        twin_pixmap_destroy(deep[i]);
    for (int i = n - 1; i >= 0; i--)
        twin_pixmap_destroy(p[i]);
    th_destroy(t);
}

#if defined(CONFIG_DROP_SHADOW)
static void test_shadow(void)
{
    th_t *t = th_create(W, H);
    /* White whatever the window manager paints behind everything */
    t->screen->background_color = 0xffffffff;
    twin_screen_damage(t->screen, 0, 0, W, H);
    twin_pixmap_t *p = solid(TWIN_ARGB32, 40, 40, 0xff000080);
    twin_pixmap_move(p, 40, 40);
    twin_pixmap_set_shadow(p, 4, 4, 6, 0xc0);
    twin_pixmap_show(p, t->screen, NULL);
    th_frame(t);

    twin_argb32_t far = th_pixel(t, 5, 5);
    twin_argb32_t under = th_pixel(t, 82, 82); /* inside shadow, past edge */
    twin_argb32_t edge = th_pixel(t, 90, 60);  /* outer falloff */
    CHECK(far == 0xffffffff, "background untouched far away: %08x", far);
    CHECK((under & 0xff) < 0xc0, "shadow darkens next to the box: %08x", under);
    CHECK((under >> 24) == 0xff, "shadow keeps the frame opaque: %08x", under);
    CHECK((edge & 0xff) > (under & 0xff), "shadow fades outwards");

    /* Shadows move with their pixmap and leave nothing behind */
    twin_pixmap_move(p, 10, 10);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "shadow damage covers its extent");
    CHECK(th_pixel(t, 82, 82) == 0xffffffff, "old shadow erased");

    twin_pixmap_set_shadow(p, 0, 0, 0, 0);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "removing shadow damages it");
    twin_pixmap_destroy(p);
    th_destroy(t);
}
#endif

static void test_rgb16_and_opacity(void)
{
    th_t *t = th_create(W, H);
    t->screen->background_color = 0xff000000;
    twin_pixmap_t *p = solid(TWIN_RGB16, 20, 20, 0xffff0000);
    twin_pixmap_show(p, t->screen, NULL);
    th_frame(t);
    CHECK(th_pixel(t, 5, 5) == 0xffff0000, "RGB16 red scans out red: %08x",
          th_pixel(t, 5, 5));
    CHECK(th_pixel(t, 19, 19) == 0xffff0000, "every RGB16 pixel converted");

    twin_pixmap_set_opacity(p, 0x80);
    th_frame(t);
    twin_argb32_t v = th_pixel(t, 5, 5);
    CHECK(((v >> 16) & 0xff) > 0x70 && ((v >> 16) & 0xff) < 0x90,
          "half opacity over black is half red: %08x", v);
    twin_pixmap_destroy(p);
    th_destroy(t);
}

static void test_stub(void)
{
    twin_pixmap_t *s = _twin_pixmap_create_stub(TWIN_ARGB32, 50, 50);
    twin_path_t *path = twin_path_create();
    twin_path_circle(path, 25 << 16, 25 << 16, 20 << 16);
    twin_paint_path(s, 0xffff0000, path);
    twin_fill(s, 0xff00ff00, TWIN_SOURCE, 0, 0, 50, 50);
    twin_text_draw(s, NULL, 12 << 16, 0, 20 << 16, 0xff000000, "stub");
    twin_pixmap_reset_clip(s);
    CHECK(s->clip.right == 0, "stub clip stays empty after reset");
    twin_path_destroy(path);
    twin_pixmap_destroy(s);
}

/* Draw a mix of shapes and text; used for cache equivalence */
static void draw_widgets(twin_pixmap_t *p, twin_coord_t ox, twin_coord_t oy)
{
    twin_path_t *path = twin_path_create();
    twin_fill(p, 0xffffffff, TWIN_SOURCE, 0, 0, p->width, p->height);
    for (int i = 0; i < 3; i++) {
        twin_path_empty(path);
        twin_path_rounded_rectangle(
            path, twin_int_to_fixed(ox + 4 + i * 40), twin_int_to_fixed(oy + 4),
            twin_int_to_fixed(36), twin_int_to_fixed(36), twin_int_to_fixed(9),
            twin_int_to_fixed(9));
        twin_paint_path(p, 0xff3080c0, path);
    }
    twin_path_empty(path);
    twin_path_circle(path, twin_int_to_fixed(ox + 60),
                     twin_int_to_fixed(oy + 70), twin_int_to_fixed(14));
    twin_paint_path_linear(p, path, 0, twin_int_to_fixed(oy + 56), 0xffff8000,
                           0, twin_int_to_fixed(oy + 84), 0xff800000);
#if defined(CONFIG_FONT_MONTSERRAT)
    twin_text_draw(p, &twin_font_montserrat, twin_int_to_fixed(14),
                   twin_int_to_fixed(ox + 4), twin_int_to_fixed(oy + 100),
                   0xff202020, "Hello, Mado 0123");
#endif
    twin_text_draw(p, NULL, twin_int_to_fixed(12), twin_int_to_fixed(ox + 90),
                   twin_int_to_fixed(oy + 100), 0xff202020, "Roman");
    twin_path_destroy(path);
}

/* Independent of CONFIG_CACHE_SIZE, which may be 0 */
#define TEST_BUDGET (24 * 1024)

static void test_cache_equivalence(void)
{
    twin_pixmap_t *a = twin_pixmap_create(TWIN_ARGB32, W, H);
    twin_pixmap_t *b = twin_pixmap_create(TWIN_ARGB32, W, H);
    size_t bytes = (size_t) a->stride * H;
    twin_cache_stats_t st0, st1, st2;

    twin_cache_set_budget(0);
    draw_widgets(a, 0, 0);
    twin_cache_set_budget(TEST_BUDGET);
    twin_cache_flush();
    twin_cache_get_stats(&st0);
    draw_widgets(b, 0, 0); /* cold: rasterize and insert */
    twin_cache_get_stats(&st1);
    CHECK(!memcmp(a->p.v, b->p.v, bytes), "cold cache changes pixels");
    draw_widgets(b, 0, 0); /* warm: every mask from the cache */
    twin_cache_get_stats(&st2);
    CHECK(!memcmp(a->p.v, b->p.v, bytes), "warm cache changes pixels");
    CHECK(st1.misses > st0.misses, "cold pass misses");
    CHECK(st2.misses == st1.misses,
          "warm pass rasterizes nothing (%u new misses)",
          st2.misses - st1.misses);
    CHECK(st2.hits > st1.hits, "warm pass hits");
    CHECK(st2.bytes <= st2.budget, "cache within budget");

    /* Translation invariance: the same shapes 7 px right, 3 px down */
    twin_cache_set_budget(0);
    draw_widgets(a, 7, 3);
    twin_cache_set_budget(TEST_BUDGET);
    twin_cache_get_stats(&st1);
    draw_widgets(b, 7, 3);
    twin_cache_get_stats(&st2);
    CHECK(!memcmp(a->p.v, b->p.v, bytes),
          "translated cache hit changes pixels");
    CHECK(st2.hits - st1.hits >= 3, "translated rounded rects hit the cache");

    /* A tiny budget evicts but never corrupts */
    twin_cache_set_budget(600);
    draw_widgets(b, 0, 0);
    draw_widgets(a, 0, 0);
    CHECK(!memcmp(a->p.v, b->p.v, bytes), "eviction churn changes pixels");
    twin_cache_get_stats(&st2);
    CHECK(st2.bytes <= 600, "tiny budget honored (%zu)", st2.bytes);
    twin_cache_set_budget((size_t) CONFIG_CACHE_SIZE * 1024);

    twin_pixmap_destroy(a);
    twin_pixmap_destroy(b);
}

static void test_text(void)
{
    twin_pixmap_t *p = twin_pixmap_create(TWIN_ARGB32, W, 40);
    twin_font_t *font = NULL;
#if defined(CONFIG_FONT_MONTSERRAT)
    font = &twin_font_montserrat;
#endif
    const char *msg = "Messages 12:03";
    twin_fixed_t size = twin_int_to_fixed(14);
    twin_fixed_t w = twin_text_width(font, size, msg);
    twin_fill(p, 0xffffffff, TWIN_SOURCE, 0, 0, W, 40);
    twin_fixed_t adv = twin_text_draw(p, font, size, twin_int_to_fixed(2),
                                      twin_int_to_fixed(20), 0xff000000, msg);
    CHECK_EQ(adv, w, "draw advance equals measured width");

    /* The path API measures with the font a path selects */
    twin_path_t *path = twin_path_create();
    twin_path_set_font(path, font);
    twin_path_set_font_size(path, size);
    CHECK_EQ(twin_width_utf8(path, msg), w, "path font matches text width");
    twin_path_destroy(path);
    CHECK(w > twin_int_to_fixed(60) && w < twin_int_to_fixed(140),
          "plausible width %d", twin_fixed_to_int(w));

    int ink = 0;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < W; x++)
            if (*twin_pixmap_pointer(p, x, y).argb32 != 0xffffffff)
                ink++;
    CHECK(ink > 100, "text left ink (%d pixels)", ink);

    /* Vertically clipped out: no pixels, same advance */
    twin_pixmap_t *q = twin_pixmap_create(TWIN_ARGB32, W, 40);
    twin_fill(q, 0xffffffff, TWIN_SOURCE, 0, 0, W, 40);
    twin_pixmap_clip(q, 0, 30, W, 40);
    adv = twin_text_draw(q, font, size, 0, twin_int_to_fixed(10), 0xff000000,
                         msg);
    CHECK_EQ(adv, w, "clipped-out text still measures");

    /* Horizontal clip: pixels inside equal the unclipped rendering */
    twin_pixmap_reset_clip(q);
    twin_fill(q, 0xffffffff, TWIN_SOURCE, 0, 0, W, 40);
    twin_pixmap_clip(q, 20, 0, 50, 40);
    twin_text_draw(q, font, size, twin_int_to_fixed(2), twin_int_to_fixed(20),
                   0xff000000, msg);
    bool same = true;
    for (int y = 0; y < 40; y++)
        for (int x = 20; x < 50; x++)
            if (*twin_pixmap_pointer(p, x, y).argb32 !=
                *twin_pixmap_pointer(q, x, y).argb32)
                same = false;
    CHECK(same, "clipped text matches unclipped inside the clip");

    twin_fixed_t asc, desc;
    twin_font_extents(font, size, &asc, &desc);
    CHECK(asc > 0 && desc > 0, "font extents positive");
    /* Stroke fonts draw U+0000 from offset 0; TrueType tables reserve it */
    CHECK(twin_has_ucs4(g_twin_font, 0), "stroke font has U+0000");
#if defined(CONFIG_FONT_MONTSERRAT)
    CHECK(!twin_has_ucs4(&twin_font_montserrat, 0), "TTF lacks U+0000");

    /* TrueType bearings come from the outline: 'j' inks left of the pen */
    twin_path_t *mp = twin_path_create();
    twin_path_set_font(mp, &twin_font_montserrat);
    twin_path_set_font_size(mp, size);
    twin_text_metrics_t jm, om;
    twin_text_metrics_utf8(mp, "j", &jm);
    CHECK(jm.left_side_bearing < 0, "TTF 'j' has a negative left bearing");
    twin_text_metrics_utf8(mp, "o", &om);
    CHECK(om.left_side_bearing > 0 && om.right_side_bearing < om.width,
          "TTF 'o' ink lies inside its advance");
    twin_path_destroy(mp);
#endif
#if defined(CONFIG_FONT_ICONS)
    CHECK(!twin_has_ucs4(&twin_font_icons, 0), "icon font lacks U+0000");
    CHECK(twin_has_ucs4(&twin_font_icons, 0xf095), "icon font has phone");
    CHECK(!twin_has_ucs4(&twin_font_icons, 0xf096), "icon font lacks U+F096");
    CHECK(twin_text_width(&twin_font_icons, size, TWIN_SYM_PHONE) > 0,
          "icon advance");
#endif
    twin_pixmap_destroy(p);
    twin_pixmap_destroy(q);
}

static void test_gradient(void)
{
    twin_pixmap_t *p = twin_pixmap_create(TWIN_ARGB32, 64, 64);
    twin_path_t *path = twin_path_create();
    twin_path_rectangle(path, 0, 0, twin_int_to_fixed(64),
                        twin_int_to_fixed(64));

    twin_paint_path_linear(p, path, 0, 0, 0xffff0000, 0, twin_int_to_fixed(64),
                           0xff0000ff);
    twin_argb32_t top = *twin_pixmap_pointer(p, 32, 0).argb32;
    twin_argb32_t mid = *twin_pixmap_pointer(p, 32, 32).argb32;
    twin_argb32_t bot = *twin_pixmap_pointer(p, 32, 63).argb32;
    CHECK(((top >> 16) & 0xff) > 0xf0 && (top & 0xff) < 0x10, "top red %08x",
          top);
    CHECK(((bot >> 16) & 0xff) < 0x10 && (bot & 0xff) > 0xf0,
          "bottom blue %08x", bot);
    CHECK(((mid >> 16) & 0xff) > 0x60 && ((mid >> 16) & 0xff) < 0xa0,
          "middle blended %08x", mid);
    CHECK(*twin_pixmap_pointer(p, 0, 32).argb32 == mid,
          "vertical gradient constant along rows");

    twin_paint_path_linear(p, path, 0, 0, 0xff000000, twin_int_to_fixed(64), 0,
                           0xffffffff);
    twin_argb32_t l = *twin_pixmap_pointer(p, 0, 10).argb32;
    twin_argb32_t r = *twin_pixmap_pointer(p, 63, 10).argb32;
    CHECK((l & 0xff) < 0x10 && (r & 0xff) > 0xf0, "horizontal %08x -> %08x", l,
          r);
    CHECK(*twin_pixmap_pointer(p, 40, 0).argb32 ==
              *twin_pixmap_pointer(p, 40, 50).argb32,
          "horizontal gradient constant along columns");
    twin_path_destroy(path);
    twin_pixmap_destroy(p);
}

/* The per-pixel formula the stepped gradient must reproduce exactly */
static twin_argb32_t ref_gradient(twin_coord_t x,
                                  twin_coord_t y,
                                  twin_fixed_t x0,
                                  twin_fixed_t y0,
                                  twin_argb32_t c0,
                                  twin_fixed_t x1,
                                  twin_fixed_t y1,
                                  twin_argb32_t c1)
{
    int64_t gx = (int64_t) x1 - x0, gy = (int64_t) y1 - y0;
    int64_t len2 = (gx * gx + gy * gy) >> 16;
    twin_fixed_t cx = twin_int_to_fixed(x) + TWIN_FIXED_HALF;
    twin_fixed_t cy = twin_int_to_fixed(y) + TWIN_FIXED_HALF;
    int64_t dot = (((int64_t) cx - x0) * gx + ((int64_t) cy - y0) * gy) >> 16;
    int64_t t = (dot * 65536) / len2;
    if (t <= 0)
        return c0;
    if (t >= TWIN_FIXED_ONE)
        return c1;
    twin_argb32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        int32_t a = (c0 >> shift) & 0xff, b = (c1 >> shift) & 0xff;
        int32_t v = a + (int32_t) (((int64_t) (b - a) * t + 0x8000) >> 16);
        out |= (twin_argb32_t) (v & 0xff) << shift;
    }
    return out;
}

/* Diagonal, reversed, steep and off-canvas gradients match the formula at
 * every pixel */
static void test_gradient_exact(void)
{
    static const struct {
        int x0, y0, x1, y1;
    } dirs[] = {
        {0, 0, 63, 63},   {63, 0, 0, 63}, {50, 60, 3, 2},  {10, 0, 13, 64},
        {-40, 7, 90, 30}, {5, 5, 6, 5},   {32, 32, 31, 0}, {0, 70, 1, -20},
    };
    twin_pixmap_t *p = twin_pixmap_create(TWIN_ARGB32, 64, 64);
    twin_path_t *path = twin_path_create();
    twin_path_rectangle(path, 0, 0, twin_int_to_fixed(64),
                        twin_int_to_fixed(64));
    const twin_argb32_t c0 = 0xff10a0f0, c1 = 0xfff02010;
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        twin_fixed_t x0 = twin_int_to_fixed(dirs[d].x0) + 0x1234;
        twin_fixed_t y0 = twin_int_to_fixed(dirs[d].y0) - 0x0567;
        twin_fixed_t x1 = twin_int_to_fixed(dirs[d].x1);
        twin_fixed_t y1 = twin_int_to_fixed(dirs[d].y1) + 0x89ab;
        twin_paint_path_linear(p, path, x0, y0, c0, x1, y1, c1);
        int bad = 0;
        for (twin_coord_t y = 0; y < 64; y++)
            for (twin_coord_t x = 0; x < 64; x++)
                bad += *twin_pixmap_pointer(p, x, y).argb32 !=
                       ref_gradient(x, y, x0, y0, c0, x1, y1, c1);
        CHECK(!bad, "gradient %zu: %d pixels off the formula", d, bad);
    }
    twin_path_destroy(path);
    twin_pixmap_destroy(p);
}

typedef struct {
    int calls;
    twin_fixed_t last;
    bool monotonic;
    int stop_after;
} tween_probe_t;

static bool tween_probe(twin_fixed_t v, void *closure)
{
    tween_probe_t *pr = closure;
    if (pr->calls && v < pr->last)
        pr->monotonic = false;
    pr->last = v;
    pr->calls++;
    return !pr->stop_after || pr->calls < pr->stop_after;
}

typedef struct {
    twin_tween_t *self;
    int calls;
    twin_fixed_t last;
} finish_probe_t;

static bool finish_and_stop(twin_fixed_t v, void *closure)
{
    finish_probe_t *f = closure;
    f->last = v;
    if (++f->calls == 2) {
        twin_tween_finish(f->self);
        return false;
    }
    return true;
}

static void test_tween(void)
{
    th_t *t = th_create(W, H);
    tween_probe_t a = {.monotonic = true};
    twin_tween_start(200, TwinEaseOutCubic, tween_probe, &a);
    th_advance(t, 400);
    CHECK_EQ(a.last, TWIN_FIXED_ONE, "tween ends at one");
    CHECK(a.monotonic, "ease-out is monotonic");
    CHECK(a.calls >= 10 && a.calls <= 16, "about one call per frame (%d)",
          a.calls);

    tween_probe_t b = {.monotonic = true};
    twin_tween_t *tw = twin_tween_start(200, TwinEaseLinear, tween_probe, &b);
    th_advance(t, 48);
    int seen = b.calls;
    twin_tween_cancel(tw);
    th_advance(t, 400);
    CHECK_EQ(b.calls, seen, "cancelled tween stops");

    tween_probe_t c = {.monotonic = true};
    tw = twin_tween_start(1000, TwinEaseInOutCubic, tween_probe, &c);
    th_advance(t, 32);
    twin_tween_finish(tw);
    CHECK_EQ(c.last, TWIN_FIXED_ONE, "finish jumps to the end");
    int after = c.calls;
    th_advance(t, 200);
    CHECK_EQ(c.calls, after, "finished tween is gone");

    tween_probe_t d = {.monotonic = true, .stop_after = 3};
    twin_tween_start(1000, TwinEaseLinear, tween_probe, &d);
    th_advance(t, 400);
    CHECK_EQ(d.calls, 3, "returning false stops the tween");

    /* finish() from the step callback delivers the end value even when the
     * callback also returns false */
    finish_probe_t f = {0};
    f.self = twin_tween_start(1000, TwinEaseLinear, finish_and_stop, &f);
    th_advance(t, 100);
    CHECK_EQ(f.last, TWIN_FIXED_ONE, "finish-and-stop delivers the end value");
    CHECK_EQ(f.calls, 3, "end value delivered once, then stopped");
    th_advance(t, 200);
    CHECK_EQ(f.calls, 3, "finish-and-stop tween is gone");

    CHECK(twin_ease(TwinEaseOutBack, TWIN_FIXED_ONE * 3 / 4) > TWIN_FIXED_ONE,
          "ease-out-back overshoots");
    th_destroy(t);
}

static twin_time_t never(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure;
    return 1000;
}

static int fired;
static twin_time_t once(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure;
    fired++;
    return -1;
}

static void test_closure_churn(void)
{
    th_t *t = th_create(W, H);
    /* Far more distinct closures than the tracker holds at once */
    for (int i = 0; i < 4 * TWIN_CLOSURE_INLINE; i++) {
        void *owner = malloc(16);
        twin_timeout_t *to = twin_set_timeout(never, 1000, owner);
        CHECK(to != NULL, "timeout %d scheduled", i);
        twin_clear_timeout(to);
        free(owner);
        if (!to)
            break;
    }
    th_frame(t);
    CHECK_EQ(_twin_closure_tracker.count, 0, "tracker drains with its holders");

    /* Many live closures at once: the table grows rather than refusing */
    enum { LIVE = 8 * TWIN_CLOSURE_INLINE };
    static char owners[LIVE];
    twin_timeout_t *live[LIVE];
    for (int i = 0; i < LIVE; i++)
        live[i] = twin_set_timeout(never, 1000, &owners[i]);
    bool all = true;
    for (int i = 0; i < LIVE; i++)
        all = all && live[i] && _twin_closure_is_valid(&owners[i]);
    CHECK(all, "every live closure tracked past the inline slots");
    for (int i = 0; i < LIVE; i++)
        twin_clear_timeout(live[i]);
    CHECK(_twin_closure_tracker.entries == _twin_closure_tracker.inline_entries,
          "drained table returns to inline storage");

    /* A timeout whose owner went away is dropped, not retried forever */
    int owner;
    fired = 0;
    twin_set_timeout(once, 10, &owner);
    _twin_closure_mark_for_free(&owner);
    _twin_closure_unregister(&owner);
    th_advance(t, 50);
    CHECK_EQ(fired, 0, "dead closure never fires");
    CHECK(_twin_timeout_delay() < 0, "dead timeout removed from the queue");

    /* NULL closures run */
    fired = 0;
    twin_set_timeout(once, 5, NULL);
    th_advance(t, 20);
    CHECK_EQ(fired, 1, "timeout without closure fires");
    th_destroy(t);
}

typedef struct {
    int offset;
    int highlight; /* row index painted highlighted, -1 for none */
} scroll_state_t;

static void scroll_draw(twin_window_t *w)
{
    scroll_state_t *s = w->client_data;
    twin_pixmap_t *p = w->pixmap;
    for (int row = -1; row < 12; row++) {
        int y = row * 12 - (s->offset % 12);
        int idx = row + s->offset / 12;
        twin_argb32_t bg = (idx & 1) ? 0xffe0e0f0 : 0xffffffff;
        if (idx == s->highlight)
            bg = 0xffffc040;
        twin_fill(p, bg, TWIN_SOURCE, 0, y, 80, y + 12);
        char label[16];
        snprintf(label, sizeof(label), "row %d", idx);
        twin_text_draw(p, NULL, twin_int_to_fixed(9), twin_int_to_fixed(3),
                       twin_int_to_fixed(y + 10), 0xff000000, label);
    }
}

static void test_scroll(void)
{
    th_t *t = th_create(W, H);
    scroll_state_t st = {0, -1};
    twin_window_t *w = twin_window_create(t->screen, TWIN_ARGB32,
                                          TwinWindowPlain, 10, 10, 80, 96);
    w->client_data = &st;
    w->draw = scroll_draw;
    twin_window_show(w);
    twin_window_draw(w);
    th_frame(t);

    for (int step = 0; step < 6; step++) {
        int d = 5 + step * 3;
        if (step == 5) {
            /* Change a row and queue its repaint without painting yet: the
             * scroll must not carry the stale pixels away from the damage
             * (last step, so the moved pixels stay in view) */
            st.highlight = 1 + st.offset / 12 + 2;
            int y = 2 * 12 - (st.offset % 12) + 12;
            twin_window_damage(w, w->client.left, w->client.top + y,
                               w->client.right, w->client.top + y + 12);
            twin_window_queue_paint(w);
        }
        st.offset += d;
        twin_window_scroll(w, (twin_rect_t) {0, 80, 0, 96}, 0, -d);
        th_frame(t);
    }
    size_t n = (size_t) w->pixmap->stride * w->pixmap->height;
    uint8_t *scrolled = malloc(n);
    memcpy(scrolled, w->pixmap->p.v, n);

    twin_window_damage(w, 0, 0, 80, 96);
    twin_window_draw(w);
    CHECK(!memcmp(scrolled, w->pixmap->p.v, n),
          "blit scrolling equals a full repaint");
    free(scrolled);
    CHECK_EQ(th_damage_check(t), 0, "scroll damage consistent");
    twin_window_destroy(w);
    th_destroy(t);
}

typedef struct {
    int calls;
    twin_area_t area; /* sum of clip areas seen by draw() */
} damage_probe_t;

static void probe_draw(twin_window_t *w)
{
    damage_probe_t *p = w->client_data;
    twin_rect_t c = twin_pixmap_get_clip(w->pixmap);
    p->calls++;
    p->area += (twin_area_t) (c.right - c.left) * (c.bottom - c.top);
    twin_fill(w->pixmap, 0xff808080, TWIN_SOURCE, c.left, c.top, c.right,
              c.bottom);
}

static void test_window_damage_rects(void)
{
    th_t *t = th_create(W, H);
    damage_probe_t probe = {0};
    twin_window_t *w =
        twin_window_create(t->screen, TWIN_ARGB32, TwinWindowPlain, 0, 0, W, H);
    w->client_data = &probe;
    w->draw = probe_draw;
    twin_window_show(w);
    twin_window_draw(w);
    th_frame(t);

    probe = (damage_probe_t) {0};
    twin_window_damage(w, 2, 2, 12, 12);
    twin_window_damage(w, W - 12, H - 12, W - 2, H - 2);
    twin_window_draw(w);
    CHECK_EQ(probe.calls, 2, "distant damage paints two rectangles");
    CHECK_EQ(probe.area, 200, "only the damaged pixels are repainted");

    probe = (damage_probe_t) {0};
    twin_window_damage(w, 10, 10, 20, 20);
    twin_window_damage(w, 20, 10, 30, 20); /* adjacent: merges for free */
    twin_window_draw(w);
    CHECK_EQ(probe.calls, 1, "adjacent damage merges");

    probe = (damage_probe_t) {0};
    for (int i = 0; i < 10; i++)
        twin_window_damage(w, i * 15, i * 11, i * 15 + 4, i * 11 + 4);
    twin_window_draw(w);
    CHECK(probe.calls <= TWIN_WINDOW_DAMAGE_RECTS, "damage list bounded (%d)",
          probe.calls);
    th_frame(t);
    CHECK_EQ(th_damage_check(t), 0, "multi-rect window damage consistent");
    twin_window_destroy(w);
    th_destroy(t);
}

/* Scaled composites must sample the same source pixel whether or not the
 * destination clip cuts the operation short */
static void test_scaled_clip(void)
{
    twin_pixmap_t *src = twin_pixmap_create(TWIN_ARGB32, 16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            *twin_pixmap_pointer(src, x, y).argb32 =
                0xff000000 | (x * 16) << 16 | (y * 16) << 8;
    /* Destination pixel = source pixel / 2: a 2x magnification */
    twin_matrix_scale(&src->transform, TWIN_FIXED_HALF, TWIN_FIXED_HALF);
    twin_operand_t op = {.source_kind = TWIN_PIXMAP, .u.pixmap = src};

    twin_pixmap_t *a = twin_pixmap_create(TWIN_ARGB32, 64, 64);
    twin_pixmap_t *b = twin_pixmap_create(TWIN_ARGB32, 64, 64);
    twin_composite(a, 10, 10, &op, 0, 0, NULL, 0, 0, TWIN_SOURCE, 32, 32);
    twin_pixmap_clip(b, 25, 20, 40, 38);
    twin_composite(b, 10, 10, &op, 0, 0, NULL, 0, 0, TWIN_SOURCE, 32, 32);

    bool same = true;
    for (int y = 20; y < 38; y++)
        for (int x = 25; x < 40; x++)
            if (*twin_pixmap_pointer(a, x, y).argb32 !=
                *twin_pixmap_pointer(b, x, y).argb32)
                same = false;
    CHECK(same, "clipped scaled composite samples the right source pixels");
    twin_pixmap_destroy(src);
    twin_pixmap_destroy(a);
    twin_pixmap_destroy(b);
}

/* Every pixel of two same-size ARGB32 pixmaps matches */
static bool same_argb32(twin_pixmap_t *a, twin_pixmap_t *b)
{
    for (twin_coord_t y = 0; y < twin_pixmap_height(a); y++)
        if (memcmp(twin_pixmap_pointer(a, 0, y).v,
                   twin_pixmap_pointer(b, 0, y).v,
                   (size_t) twin_pixmap_width(a) * 4))
            return false;
    return true;
}

static void test_rgb16_mask(void)
{
    /* RGB16 has no alpha: masking with it equals masking with nothing */
    twin_pixmap_t *src = solid(TWIN_ARGB32, 8, 8, 0x80ff4020);
    twin_pixmap_t *msk = solid(TWIN_RGB16, 8, 8, 0xff000000);
    twin_pixmap_t *a = solid(TWIN_ARGB32, 8, 8, 0xff102030);
    twin_pixmap_t *b = solid(TWIN_ARGB32, 8, 8, 0xff102030);
    twin_operand_t sop = {.source_kind = TWIN_PIXMAP, .u.pixmap = src};
    twin_operand_t mop = {.source_kind = TWIN_PIXMAP, .u.pixmap = msk};
    twin_composite(a, 0, 0, &sop, 0, 0, &mop, 0, 0, TWIN_OVER, 8, 8);
    twin_composite(b, 0, 0, &sop, 0, 0, NULL, 0, 0, TWIN_OVER, 8, 8);
    CHECK(same_argb32(a, b), "RGB16 mask is opaque");
    twin_pixmap_destroy(src);
    twin_pixmap_destroy(msk);
    twin_pixmap_destroy(a);
    twin_pixmap_destroy(b);
}

/* A transformed RGB16 source reads the same pixels as its ARGB32 copy, and
 * transparency outside it, all the way down the pixmap */
static void test_rgb16_xform(void)
{
    twin_pixmap_t *s16 = twin_pixmap_create(TWIN_RGB16, 16, 16);
    twin_pixmap_t *s32 = twin_pixmap_create(TWIN_ARGB32, 16, 16);
    for (int y = 0; y < 16; y++) {
        twin_argb32_t c = 0xff000000 | (twin_argb32_t) (y * 16) << 16 |
                          (twin_argb32_t) (255 - y * 16);
        twin_fill(s16, c, TWIN_SOURCE, 0, y, 16, y + 1);
    }
    /* The ARGB32 copy holds the exact RGB565 values, so only the read path
     * differs */
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            *twin_pixmap_pointer(s32, x, y).argb32 =
                twin_rgb16_to_argb32(*twin_pixmap_pointer(s16, x, y).rgb16);
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_translate(&m, twin_int_to_fixed(4), twin_int_to_fixed(4));
    twin_pixmap_set_transform(s16, &m);
    twin_pixmap_set_transform(s32, &m);
    twin_operand_t o16 = {.source_kind = TWIN_PIXMAP, .u.pixmap = s16};
    twin_operand_t o32 = {.source_kind = TWIN_PIXMAP, .u.pixmap = s32};
    twin_pixmap_t *a = solid(TWIN_ARGB32, 16, 16, 0xff808080);
    twin_pixmap_t *b = solid(TWIN_ARGB32, 16, 16, 0xff808080);
    twin_composite(a, 0, 0, &o16, 0, 0, NULL, 0, 0, TWIN_OVER, 16, 16);
    twin_composite(b, 0, 0, &o32, 0, 0, NULL, 0, 0, TWIN_OVER, 16, 16);
    CHECK(same_argb32(a, b),
          "transformed RGB16 source matches its ARGB32 copy");
    twin_pixmap_destroy(s16);
    twin_pixmap_destroy(s32);
    twin_pixmap_destroy(a);
    twin_pixmap_destroy(b);
}

/* A translated source transform shifts which source pixels land where,
 * with every renderer (pixman used to drop the translation) */
static void test_translated_source(void)
{
    twin_pixmap_t *src = twin_pixmap_create(TWIN_ARGB32, 16, 16);
    twin_fill(src, 0x00000000, TWIN_SOURCE, 0, 0, 16, 16);
    twin_fill(src, 0xffff0000, TWIN_SOURCE, 8, 8, 16, 16);
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_translate(&m, twin_int_to_fixed(8), twin_int_to_fixed(8));
    twin_pixmap_set_transform(src, &m);
    twin_operand_t op = {.source_kind = TWIN_PIXMAP, .u.pixmap = src};

    twin_pixmap_t *dst = twin_pixmap_create(TWIN_ARGB32, 16, 16);
    twin_fill(dst, 0xff000000, TWIN_SOURCE, 0, 0, 16, 16);
    twin_composite(dst, 0, 0, &op, 0, 0, NULL, 0, 0, TWIN_SOURCE, 6, 6);
    CHECK_EQ(*twin_pixmap_pointer(dst, 2, 2).argb32, 0xffff0000,
             "translated source: (2,2) reads source (10,10)");
    twin_pixmap_destroy(src);
    twin_pixmap_destroy(dst);
}

static void test_leaks(void)
{
    th_t *t = th_create(W, H);
    th_frame(t);
    size_t base = th_heap_net(t);
    for (int i = 0; i < 20; i++) {
        twin_window_t *w = twin_window_create(
            t->screen, TWIN_ARGB32, TwinWindowApplication, 0, 0, 50, 50);
        twin_window_show(w);
        twin_window_queue_paint(w);
        twin_tween_start(100, TwinEaseLinear, tween_probe,
                         &(tween_probe_t) {0});
        th_advance(t, 20);
        twin_window_destroy(w); /* with a paint still queued */
        th_advance(t, 200);
    }
    th_frame(t);
    CHECK(th_heap_net(t) <= base, "heap back to baseline (%zu > %zu)",
          th_heap_net(t), base);

#if defined(CONFIG_MEMORY_STATS)
    /* Injected failures reach realloc too, leaving the block intact */
    char *buf = twin_malloc(16);
    CHECK(buf, "malloc");
    if (buf) {
        buf[0] = 'x';
        _twin_alloc_fail_next = 1;
        CHECK(!twin_realloc(buf, 64), "injected realloc failure");
        CHECK_EQ(_twin_alloc_fail_next, 0, "realloc consumed the failure");
        _twin_alloc_fail_next = 0;
        CHECK_EQ(buf[0], 'x', "block kept after a failed realloc");
        twin_free(buf);
    }
    CHECK(th_heap_net(t) <= base, "failed realloc leaks (%zu > %zu)",
          th_heap_net(t), base);
#endif
    th_destroy(t);
}

#if defined(CONFIG_WIDGETS)
static int destroyed_callbacks;

static twin_dispatch_result_t count_destroy(twin_widget_t *widget,
                                            twin_event_t *event,
                                            void *data)
{
    (void) widget;
    (void) data;
    if (event->kind == TwinEventDestroy)
        destroyed_callbacks++;
    return TwinDispatchContinue;
}

static twin_dispatch_result_t custom_handler(twin_custom_widget_t *custom,
                                             twin_event_t *event)
{
    (void) custom;
    (void) event;
    return TwinDispatchContinue;
}

/* Destroying a toplevel releases every widget the toolkit created for it */
static void test_widget_teardown(void)
{
    th_t *t = th_create(W, H);
    th_frame(t);
    size_t base = th_heap_net(t);
    for (int round = 0; round < 3; round++) {
        twin_toplevel_t *top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
        twin_box_t *row = twin_box_create(&top->box, TwinBoxHorz);
        twin_label_t *label = twin_label_create(
            row, "label", 0xff000000, twin_int_to_fixed(12), TwinStyleRoman);
        twin_button_t *button = twin_button_create(
            row, "button", 0xff000000, twin_int_to_fixed(12), TwinStyleRoman);
        twin_widget_set_callback(&button->label.widget, count_destroy, NULL);
        twin_custom_widget_create(&top->box, 0xffffffff, 10, 10, custom_handler,
                                  64);
        (void) label;
        twin_toplevel_show(top);
        th_frame(t);
        destroyed_callbacks = 0;
        twin_window_destroy(top->box.widget.window);
        th_frame(t);
        CHECK_EQ(destroyed_callbacks, 1, "callbacks see the widget's destroy");
    }
    CHECK(th_heap_net(t) <= base, "toolkit frees its widgets (%zu > %zu)",
          th_heap_net(t), base);
    th_destroy(t);
}
#endif

/* A timeout cleared by another one in the same pass must not run */
static twin_timeout_t *victim;
static int victim_runs;
static twin_time_t clear_victim(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure;
    twin_clear_timeout(victim);
    return -1;
}
static twin_time_t victim_proc(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure;
    victim_runs++;
    return -1;
}

static twin_tween_t *victim_tween;
static twin_time_t cancel_victim_tween(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure;
    twin_tween_cancel(victim_tween);
    victim_tween = NULL;
    return -1;
}

static void test_cleared_in_pass(void)
{
    th_t *t = th_create(W, H);
    int a, b;
    victim_runs = 0;
    twin_set_timeout(clear_victim, 0, &a);
    victim = twin_set_timeout(victim_proc, 5, &b);
    th_advance(t, 20);
    CHECK_EQ(victim_runs, 0, "timeout cleared earlier in the pass stays dead");

    /* Same for a tween cancelled by a timeout due in the same pass, just
     * ahead of the tween's own tick */
    tween_probe_t p = {.monotonic = true};
    victim_tween = twin_tween_start(100, TwinEaseLinear, tween_probe, &p);
    th_advance(t, 16); /* first tick ran; next one due 16 ms from now */
    int calls = p.calls;
    CHECK_EQ(calls, 1, "tween ticked once");
    twin_set_timeout(cancel_victim_tween, 15, &a);
    th_advance(t, 16); /* both due: the canceller runs first */
    CHECK(!victim_tween, "canceller ran");
    CHECK_EQ(p.calls, calls, "tween cancelled in the pass does not tick");
    th_advance(t, 50);
    CHECK_EQ(p.calls, calls, "cancelled tween never ticks again");
    th_destroy(t);
}

/* Clipped text must equal unclipped text inside the clip, including ink
 * that overshoots the font extents or starts left of the pen */
static void test_text_clip_overshoot(void)
{
    twin_font_t *fonts[] = {NULL,
#if defined(CONFIG_FONT_MONTSERRAT)
                            &twin_font_montserrat
#endif
    };
    for (size_t f = 0; f < sizeof(fonts) / sizeof(fonts[0]); f++) {
        /* Rows above the nominal ascent; ink left of a pen past the clip */
        static const struct {
            twin_rect_t clip;
            twin_fixed_t x;
            const char *text;
            int size;
        } cases[] = {{{0, 80, 0, 22}, 10, "|#j([", 24},
                     {{0, 30, 0, 60}, 31, "j", 32}};
        for (size_t c = 0; c < 2; c++) {
            twin_pixmap_t *a = twin_pixmap_create(TWIN_ARGB32, 80, 60);
            twin_pixmap_t *b = twin_pixmap_create(TWIN_ARGB32, 80, 60);
            twin_rect_t r = cases[c].clip;
            twin_pixmap_clip(b, r.left, r.top, r.right, r.bottom);
            for (twin_pixmap_t *p = a; p; p = p == a ? b : NULL)
                twin_text_draw(p, fonts[f], twin_int_to_fixed(cases[c].size),
                               twin_int_to_fixed(cases[c].x),
                               twin_int_to_fixed(40), 0xff000000,
                               cases[c].text);
            bool same = true;
            for (int y = r.top; y < r.bottom; y++)
                for (int x = r.left; x < r.right; x++)
                    if (*twin_pixmap_pointer(a, x, y).argb32 !=
                        *twin_pixmap_pointer(b, x, y).argb32)
                        same = false;
            CHECK(same, "font %zu case %zu: clipped text matches", f, c);
            twin_pixmap_destroy(a);
            twin_pixmap_destroy(b);
        }
    }
}

/* A destroyed window's queued repaint must not run, even if its address
 * is registered again by an unrelated owner (address reuse) */
static int stale_draws;
static void stale_draw(twin_window_t *w)
{
    (void) w;
    stale_draws++;
}

static void test_destroyed_paint(void)
{
    th_t *t = th_create(W, H);
    twin_window_t *w = twin_window_create(t->screen, TWIN_ARGB32,
                                          TwinWindowPlain, 0, 0, 50, 50);
    w->draw = stale_draw;
    stale_draws = 0;
    twin_window_queue_paint(w);
    void *addr = w;
    twin_window_destroy(w);
    _twin_closure_register(addr); /* as if a new object took the address */
    th_frame(t);
    CHECK_EQ(stale_draws, 0, "queued repaint of a destroyed window never runs");
    _twin_closure_unregister(addr);
    th_destroy(t);
}

/* The public accessors report the fields they stand for */
static bool on_draw_called;
static void on_draw(twin_window_t *window)
{
    (void) window;
    on_draw_called = true;
}

static void test_accessors(void)
{
    th_t *t = th_create(W, H);
    CHECK(
        twin_screen_width(t->screen) == W && twin_screen_height(t->screen) == H,
        "screen size");
    CHECK(twin_screen_stats(t->screen) == &t->screen->stats, "screen stats");

    CHECK(!twin_pixmap_create(TWIN_ARGB32, 1, -1) &&
              !twin_pixmap_create(TWIN_ARGB32, -1, 1),
          "negative pixmap sizes rejected");

    twin_pixmap_t *p = twin_pixmap_create(TWIN_RGB16, 7, 5);
    twin_coord_t stride = 0;
    CHECK(twin_pixmap_width(p) == 7 && twin_pixmap_height(p) == 5 &&
              twin_pixmap_format(p) == TWIN_RGB16,
          "pixmap size and format");
    CHECK(twin_pixmap_pixels(p, &stride) == p->p.v && stride == p->stride,
          "pixmap pixels and stride");
    CHECK(twin_pixmap_pixels(p, NULL) == p->p.v, "stride is optional");
    CHECK(!twin_pixmap_animation(p), "still pixmap has no animation");
    twin_matrix_t m;
    twin_matrix_identity(&m);
    twin_matrix_scale(&m, TWIN_FIXED_HALF, TWIN_FIXED_ONE * 2);
    twin_pixmap_set_transform(p, &m);
    CHECK(!memcmp(&p->transform, &m, sizeof(m)), "transform set");
    twin_pixmap_set_transform(p, NULL);
    CHECK(twin_matrix_is_identity(&p->transform), "NULL resets the transform");
    twin_pixmap_destroy(p);

    twin_window_t *w = twin_window_create(t->screen, TWIN_ARGB32,
                                          TwinWindowPlain, 3, 4, 20, 10);
    CHECK(twin_window_pixmap(w) == w->pixmap &&
              twin_window_style(w) == TwinWindowPlain,
          "window pixmap and style");
    twin_rect_t c = twin_window_client(w);
    CHECK(!memcmp(&c, &w->client, sizeof(c)), "window client area");
    CHECK(!twin_window_is_mapped(w), "new window is unmapped");
    int data;
    twin_window_set_client_data(w, &data);
    CHECK(twin_window_client_data(w) == &data, "client data round-trips");
    twin_window_set_callbacks(w, on_draw, NULL, NULL);
    on_draw_called = false;
    twin_window_draw(w);
    twin_window_show(w);
    th_frame(t);
    CHECK(twin_window_is_mapped(w) && on_draw_called,
          "shown window is mapped and drawn through the callback");
    twin_window_destroy(w);

#if defined(CONFIG_WIDGETS)
    twin_toplevel_t *top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
    CHECK(twin_toplevel_box(top) == &top->box, "toplevel box");
    twin_label_t *label =
        twin_label_create(twin_toplevel_box(top), "l", 0xff000000,
                          twin_int_to_fixed(12), TwinStyleRoman);
    twin_button_t *button =
        twin_button_create(twin_toplevel_box(top), "b", 0xff000000,
                           twin_int_to_fixed(12), TwinStyleRoman);
    CHECK(twin_label_widget(label) == &label->widget &&
              twin_button_widget(button) == &button->label.widget,
          "label and button widgets");
    twin_label_set_align(label, TwinAlignRight);
    twin_widget_set_shape(twin_label_widget(label), TwinShapeEllipse);
    twin_widget_match_size(twin_button_widget(button),
                           twin_label_widget(label));
    CHECK(label->align == TwinAlignRight &&
              label->widget.shape == TwinShapeEllipse &&
              button->label.widget.copy_geom == &label->widget,
          "widget setters");
    twin_toplevel_show(top);
    th_frame(t);
    CHECK(
        button->label.widget.preferred.width == label->widget.preferred.width &&
            button->label.widget.preferred.height ==
                label->widget.preferred.height,
        "matched widget takes the source's size");
    twin_window_destroy(top->box.widget.window);
#endif
    th_destroy(t);
}

#if defined(CONFIG_WIDGETS)
static twin_argb32_t client_pixel(twin_window_t *w,
                                  twin_coord_t x,
                                  twin_coord_t y)
{
    twin_rect_t c = twin_window_client(w);
    return *twin_pixmap_pointer(w->pixmap, c.left + x, c.top + y).argb32;
}

/* A toplevel paints every client pixel, so the compositor can treat it as
 * opaque; children repaint over their ancestors' background instead of
 * punching through it */
static void test_toplevel_background(void)
{
    th_t *t = th_create(W, H);
    twin_toplevel_t *top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
    twin_window_t *w = top->box.widget.window;
    twin_label_t *label =
        twin_label_create(twin_toplevel_box(top), " ", 0xff000000,
                          twin_int_to_fixed(12), TwinStyleRoman);
    twin_label_t *oval =
        twin_label_create(twin_toplevel_box(top), " ", 0xff000000,
                          twin_int_to_fixed(12), TwinStyleRoman);
    twin_widget_t *ow = twin_label_widget(oval);
    twin_widget_set(ow, 0x80000000);
    twin_widget_set_shape(ow, TwinShapeEllipse);
    twin_toplevel_show(top);
    th_frame(t);
    CHECK(w->opaque, "a toplevel is opaque");

    twin_rect_t e = label->widget.extents;
    CHECK_EQ(client_pixel(w, e.left + 1, e.top + 1), 0xffffffff,
             "the root's background under a transparent child");
    twin_label_set(label, "  ");
    th_frame(t);
    CHECK_EQ(client_pixel(w, e.left + 1, e.top + 1), 0xffffffff,
             "a repainted child keeps it");

    /* A translucent shape repaints to the same pixels, not darker */
    twin_rect_t o = ow->extents;
    twin_coord_t cx = (o.left + o.right) / 2, cy = (o.top + o.bottom) / 2;
    twin_argb32_t center = client_pixel(w, cx, cy);
    CHECK(center != 0xffffffff, "the oval is painted");
    for (int i = 0; i < 3; i++) {
        twin_label_set(oval, i & 1 ? " " : "  ");
        th_frame(t);
    }
    CHECK_EQ(client_pixel(w, cx, cy), center, "oval repainted");
    CHECK_EQ(client_pixel(w, o.left, o.top), 0xffffffff,
             "the root's background outside the oval");

    twin_widget_set(twin_box_widget(twin_toplevel_box(top)), 0x80000000);
    CHECK(!w->opaque, "a translucent background makes it translucent");
    twin_widget_set(twin_box_widget(twin_toplevel_box(top)), 0xff102030);
    CHECK(w->opaque, "and an opaque one opaque again");
    th_frame(t);
    CHECK_EQ(client_pixel(w, e.left + 1, e.top + 1), 0xff102030,
             "children repaint over the new background");
    CHECK_EQ(th_damage_check(t), 0, "toplevel background");
    twin_toplevel_destroy(top);
    th_destroy(t);
}

/* Boxes paint their own background in the gaps their children leave, a
 * background or shape change repaints everything over it, and a shaped
 * root leaves the rest of the window transparent */
static void test_box_background(void)
{
    th_t *t = th_create(W, H);
    twin_toplevel_t *top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
    twin_window_t *w = top->box.widget.window;
    twin_widget_t *root = twin_box_widget(twin_toplevel_box(top));
    twin_toplevel_show(top);
    th_frame(t);
    CHECK_EQ(client_pixel(w, 5, 5), 0xffffffff, "an empty toplevel is white");
    twin_widget_set(root, 0xff336699);
    th_frame(t);
    CHECK_EQ(client_pixel(w, 5, 5), 0xff336699,
             "a new background reaches pixels no child covers");
    twin_widget_set_shape(root, TwinShapeEllipse);
    th_frame(t);
    CHECK(!w->opaque, "a shaped root is translucent");
    CHECK_EQ(client_pixel(w, 0, 0), 0, "outside the root's shape is clear");
    CHECK_EQ(th_damage_check(t), 0, "shaped root");
    twin_toplevel_destroy(top);

    /* A child fills over its box's background, and a shape change on the
     * box repaints the child */
    top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
    w = top->box.widget.window;
    /* root > outer > (box > label, sibling) */
    twin_box_t *outer = twin_box_create(twin_toplevel_box(top), TwinBoxVert);
    twin_box_t *box = twin_box_create(outer, TwinBoxVert);
    twin_label_t *label = twin_label_create(
        box, " ", 0xff000000, twin_int_to_fixed(12), TwinStyleRoman);
    twin_label_t *sibling = twin_label_create(
        outer, " ", 0xff000000, twin_int_to_fixed(12), TwinStyleRoman);
    twin_widget_set(twin_box_widget(box), 0xff00ff00);
    twin_widget_set(twin_label_widget(sibling), 0xffff0000);
    twin_toplevel_show(top);
    th_frame(t);
    /* Repainting deep in the tree leaves the siblings alone */
    twin_rect_t o = twin_box_widget(outer)->extents;
    twin_rect_t s = twin_label_widget(sibling)->extents;
    s.left += o.left, s.top += o.top;
    twin_label_set(label, "  ");
    th_frame(t);
    CHECK_EQ(client_pixel(w, s.left + 1, s.top + 1), 0xffff0000,
             "a nested repaint keeps the siblings");
    twin_rect_t b = twin_box_widget(box)->extents;
    b.left += o.left, b.top += o.top;
    twin_rect_t l = twin_label_widget(label)->extents;
    CHECK(l.right > l.left + 2 && l.bottom > l.top + 2, "label laid out");
    CHECK_EQ(client_pixel(w, b.left + l.left + 1, b.top + l.top + 1),
             0xff00ff00, "a child fills over its box's background");
    twin_widget_set_shape(twin_box_widget(box), TwinShapeEllipse);
    th_frame(t);
    CHECK_EQ(client_pixel(w, b.left + l.left + 1, b.top + l.top + 1),
             0xffffffff, "a shape change repaints the children");
    CHECK_EQ(th_damage_check(t), 0, "box background");
    twin_toplevel_destroy(top);
    th_destroy(t);
}
#endif

#if defined(CONFIG_LOADER_GIF) || defined(CONFIG_LOADER_TVG) || \
    defined(CONFIG_LOADER_PNG)
/* Write @len bytes to a temporary file named in @path ("...XXXXXX"); a
 * fixture that cannot be written fails the run rather than passing as a
 * loader's rejection */
static bool write_fixture(char *path, const uint8_t *bytes, size_t len)
{
    int fd = mkstemp(path);
    CHECK(fd >= 0, "temporary file created");
    if (fd < 0)
        return false;
    bool ok = write(fd, bytes, len) == (ssize_t) len;
    close(fd);
    CHECK(ok, "temporary file written");
    if (!ok)
        unlink(path);
    return ok;
}
#endif

#if defined(CONFIG_LOADER_TVG)
/* A line of width zero is drawn with a hairline pen, not dropped: 0.01
 * units, one pixel here at 100x */
static void test_tvg_hairline(void)
{
    const uint8_t doc[] = {
        0x72, 0x56, 1,           /* magic, version 1 */
        0x44,                    /* scale 4 (1/16 unit), RGBA8888, u8 coords */
        2,    2,                 /* 2 x 2 units */
        1,    0,    0,  0,  255, /* one color: opaque black */
        0x04, 0,    0,  0,       /* draw 1 line, color 0, width 0 */
        2,    16,   30, 16,      /* (0.125, 1) to (1.875, 1) */
        0x00,                    /* end */
    };
    char path[] = "/tmp/mado-tvg-XXXXXX";
    if (!write_fixture(path, doc, sizeof(doc)))
        return;
    twin_pixmap_t *p = twin_tvg_to_pixmap(path, 200, 200);
    unlink(path);
    CHECK(p != NULL, "TVG with a zero-width line loads");
    if (!p)
        return;
    int inked = 0;
    for (int y = 0; y < twin_pixmap_height(p); y++)
        for (int x = 0; x < twin_pixmap_width(p); x++)
            inked += (*twin_pixmap_pointer(p, x, y).argb32 >> 24) != 0;
    CHECK(inked > 100, "zero-width line drawn as a hairline (%d pixels)",
          inked);
    twin_pixmap_destroy(p);
}

/* A palette index past the color table is rejected, not read out of
 * bounds; so is one equal to the table size */
static void test_tvg_color_index(void)
{
    uint8_t doc[] = {
        0x72, 0x56, 1,  0x44, 2,    2, 1, 0, 0, 0, 255, /* as above */
        0x04, 0,    1,  0,                              /* color 1 of 1 */
        2,    16,   30, 16,   0x00,
    };
    char path[] = "/tmp/mado-tvg-XXXXXX";
    if (!write_fixture(path, doc, sizeof(doc)))
        return;
    twin_pixmap_t *p = twin_tvg_to_pixmap(path, 20, 20);
    unlink(path);
    CHECK(!p, "TVG color index past the table rejected");
    if (p)
        twin_pixmap_destroy(p);

    /* A varuint's fifth byte carries bits 28 to 31 only: a color count of
     * 1 + 2^32 must not alias to one color */
    uint8_t wide[] = {
        0x72, 0x56, 1,    0x44, 2,    2, /* header, 2x2 */
        0x81, 0x80, 0x80, 0x80, 0x10,    /* 1 + 2^32 colors */
        0,    0,    0,    255,  0x00,    /* one color, end */
    };
    char wide_path[] = "/tmp/mado-tvg-XXXXXX";
    if (!write_fixture(wide_path, wide, sizeof(wide)))
        return;
    p = twin_tvg_to_pixmap(wide_path, 20, 20);
    unlink(wide_path);
    CHECK(!p, "TVG varuint past 32 bits rejected");
    if (p)
        twin_pixmap_destroy(p);
}

#endif

#if defined(CONFIG_LOADER_GIF) || defined(CONFIG_LOADER_PNG)
/* Load @len bytes as a file */
static twin_pixmap_t *load_bytes(const uint8_t *bytes, size_t len)
{
    char path[] = "/tmp/mado-img-XXXXXX";
    if (!write_fixture(path, bytes, len))
        return NULL;
    twin_pixmap_t *p = twin_pixmap_from_file(path);
    unlink(path);
    return p;
}

static twin_argb32_t pixel_at(twin_pixmap_t *p, int x, int y)
{
    return *twin_pixmap_pointer(p, x, y).argb32;
}
#endif

#if defined(CONFIG_LOADER_PNG)
/* 16-bit RGB decodes (it was rejected before the transforms were checked),
 * in ARGB order on every host */
static void test_png_16bit(void)
{
    /* 2x1, 16 bits per channel: (253, 161, 6) and (200, 200, 200) */
    const uint8_t png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x01, 0x10, 0x02, 0x00, 0x00, 0x00, 0x2b, 0xd0, 0x34, 0x9e,
        0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63,
        0xf8, 0xfb, 0x77, 0xe1, 0x42, 0x36, 0xb6, 0x13, 0x60, 0x00, 0x00,
        0x33, 0x7d, 0x07, 0xf9, 0x12, 0xbf, 0xb4, 0x11, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    };
    twin_pixmap_t *p = load_bytes(png, sizeof(png));
    CHECK(p != NULL, "16-bit PNG loads");
    if (!p)
        return;
    CHECK_EQ(pixel_at(p, 0, 0), 0xfffda106, "16-bit PNG pixel 0");
    CHECK_EQ(pixel_at(p, 1, 0), 0xffc8c8c8, "16-bit PNG pixel 1");
    twin_pixmap_destroy(p);
}
#endif

#if defined(CONFIG_LOADER_GIF)
/* Frames draw into the pixmap itself: transparent pixels stay clear, a
 * two-row interlaced frame lands on its own rows, and looping restarts
 * from a clear canvas */
static void test_gif_frames(void)
{
    /* 1x2, palette black/white, every frame transparent on index 0 */
    const uint8_t gif[] = {
        'G',  'I',  'F',  '8',  '9',  'a',  1, 0, 2, 0,
        0x80, 0,    0,                                     /* screen */
        0,    0,    0,    0xff, 0xff, 0xff,                /* GCT */
        '!',  0xf9, 4,    0x01, 1,    0,    0, 0,          /* GCE */
        ',',  0,    0,    0,    0,    1,    0, 2, 0, 0x40, /* interlaced 1x2 */
        2,    2,    0x44, 0x0a, 0,                         /* rows: 0, 1 */
        '!',  0xf9, 4,    0x01, 1,    0,    0, 0,          /* GCE */
        ',',  0,    0,    0,    0,    1,    0, 2, 0, 0,    /* 1x2 */
        2,    2,    0x4c, 0x0a, 0,                         /* rows: 1, 1 */
        ';',
    };
    twin_pixmap_t *p = load_bytes(gif, sizeof(gif));
    CHECK(p != NULL, "animated GIF loads");
    if (!p)
        return;
    twin_animation_t *a = twin_pixmap_animation(p);
    CHECK(a && twin_animation_get_current_frame(a) == p,
          "the pixmap shows the current frame");
    CHECK_EQ(pixel_at(p, 0, 0), 0, "transparent index left clear");
    CHECK_EQ(pixel_at(p, 0, 1), 0xffffffff, "interlaced row 1 in place");
    CHECK_EQ(twin_animation_get_current_delay(a), 100,
             "1/100 s plays at 100 ms, as browsers do");
    twin_animation_advance_frame(a);
    CHECK_EQ(pixel_at(p, 0, 0), 0xffffffff, "second frame drawn over");
    twin_animation_advance_frame(a);
    CHECK_EQ(pixel_at(p, 0, 0), 0, "loop restarts from a clear canvas");
    twin_pixmap_destroy(p);

    /* A restore-to-previous first frame with a bad code size: rejected,
     * and the saved canvas it allocated is freed with the decoder */
    uint8_t bad[sizeof(gif)];
    memcpy(bad, gif, sizeof(gif));
    bad[22] = 0x0c; /* GCE: disposal 3 */
    bad[37] = 0;    /* LZW minimum code size 0 */
    size_t before = th_heap_raw();
    p = load_bytes(bad, sizeof(bad));
    CHECK(!p, "undecodable restore-to-previous frame rejected");
    CHECK_EQ(th_heap_raw(), before, "failed GIF open frees its decoder");
    if (p)
        twin_pixmap_destroy(p);

    /* NETSCAPE count 1: one repeat after the first play, as browsers
     * count, then the last frame stays */
    uint8_t twice[sizeof(gif) + 19];
    memcpy(twice, gif, 19); /* screen and global color table */
    memcpy(twice + 19,
           (const uint8_t[]) {'!', 0xff, 11, 'N', 'E', 'T', 'S', 'C', 'A', 'P',
                              'E', '2', '.', '0', 3, 1, 1, 0, 0},
           19);
    memcpy(twice + 38, gif + 19, sizeof(gif) - 19);
    p = load_bytes(twice, sizeof(twice));
    CHECK(p != NULL, "counted GIF loads");
    if (p) {
        a = twin_pixmap_animation(p);
        twin_argb32_t seen[6];
        for (int i = 0; i < 6; i++) {
            seen[i] = pixel_at(p, 0, 0);
            twin_animation_advance_frame(a);
        }
        CHECK(!seen[0] && seen[1] && !seen[2] && seen[3] && seen[4] && seen[5],
              "count 1 plays twice, then holds the last frame");

        /* Played out, it draws nothing more, so a shown canvas is not
         * repainted on every tick */
        th_t *t = th_create(16, 16);
        twin_pixmap_show(p, t->screen, NULL);
        th_frame(t);
        twin_animation_advance_frame(a);
        CHECK(!twin_screen_damaged(t->screen),
              "a finished animation damages nothing");
        twin_pixmap_destroy(p);
        th_destroy(t);
    }

    /* 8x1, four colors, an encoder that clears every two codes, right at
     * the code-size bump: the clear cancels the bump */
    const uint8_t early[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x08, 0x00, 0x01, 0x00, 0x81,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01,
        0x00, 0x00, 0x02, 0x05, 0x44, 0xa8, 0x71, 0x62, 0x50, 0x00, 0x3b,
    };
    p = load_bytes(early, sizeof(early));
    CHECK(p != NULL, "early-clear GIF loads");
    if (p) {
        static const twin_argb32_t row[] = {
            0xff000000, 0xffffffff, 0xffff0000, 0xff0000ff,
            0xff0000ff, 0xffff0000, 0xffffffff, 0xff000000,
        };
        bool same = true;
        for (int x = 0; x < 8; x++)
            same &= pixel_at(p, x, 0) == row[x];
        CHECK(same, "codes after an early clear keep their size");
        twin_pixmap_destroy(p);
    }

    /* Play once; frame 2 asks for disposal 2 but sits at x=100, off the
     * canvas: it draws nothing, disposes of nothing (its rectangle once
     * made the next frame write outside the canvas), and its data is
     * skipped so frame 3 still decodes */
    /* clang-format off */
    const uint8_t once[] = {
        'G', 'I', 'F', '8', '9', 'a', 1, 0, 2, 0, 0x80, 0, 0, /* screen */
        0, 0, 0, 0xff, 0xff, 0xff,                            /* GCT */
        '!', 0xff, 11, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
        3, 1, 1, 0, 0,                                 /* loop count 1 */
        '!', 0xf9, 4, 0x01, 1, 0, 0, 0,                /* GCE */
        ',', 0, 0, 0, 0, 1, 0, 2, 0, 0x40,             /* frame 1 */
        2, 2, 0x44, 0x0a, 0,
        '!', 0xf9, 4, 0x08, 1, 0, 0, 0,                /* GCE: disposal 2 */
        ',', 100, 0, 0, 0, 1, 0, 2, 0, 0,              /* x=100: off canvas */
        2, 2, 0x4c, 0x0a, 0,
        ',', 0, 0, 0, 0, 1, 0, 2, 0, 0,                /* frame 3 */
        2, 2, 0x4c, 0x0a, 0,
        ';',
    };
    /* clang-format on */
    p = load_bytes(once, sizeof(once));
    CHECK(p != NULL, "play-once GIF loads");
    if (!p)
        return;
    a = twin_pixmap_animation(p);
    twin_animation_advance_frame(a); /* the off-canvas frame */
    CHECK(pixel_at(p, 0, 0) == 0 && pixel_at(p, 0, 1) == 0xffffffff,
          "off-canvas frame leaves the canvas alone");
    twin_animation_advance_frame(a);
    CHECK_EQ(pixel_at(p, 0, 0), 0xffffffff, "frame after it drawn");
    twin_pixmap_destroy(p);
}

/* Malformed GIFs fail to load instead of hanging or writing out of
 * bounds: a frame that cannot be decoded and no trailer, a zero size, and
 * a size that does not fit a pixmap */
static void test_gif_malformed(void)
{
    /* 4x4, black/white palette, a 2x2 image placed at x=100, then EOF */
    uint8_t bad[] = {'G',  'I', 'F', '8', '9', 'a', 4,    0,    4,    0,
                     0x80, 0,   0,   0,   0,   0,   0xff, 0xff, 0xff, ',',
                     100,  0,   0,   0,   2,   0,   2,    0,    0};
    CHECK(!load_bytes(bad, sizeof(bad)), "undecodable frame rejected");

    uint8_t zero[sizeof(bad)];
    memcpy(zero, bad, sizeof(bad));
    zero[6] = zero[7] = 0;
    CHECK(!load_bytes(zero, sizeof(zero)), "zero width rejected");

    uint8_t huge[sizeof(bad)];
    memcpy(huge, bad, sizeof(bad));
    huge[6] = huge[7] = huge[8] = huge[9] = 0xff;
    CHECK(!load_bytes(huge, sizeof(huge)), "65535x65535 rejected");

    /* Every truncation of a small image returns: a cut inside the data
     * sub-blocks used to leave the block size unread and loop forever */
    const uint8_t good[] = {'G',  'I',  'F', '8', '9',  'a',  2, 0,    2,
                            0,    0x80, 0,   0,   0,    0,    0, 0xff, 0xff,
                            0xff, ',',  0,   0,   0,    0,    2, 0,    2,
                            0,    0,    2,   2,   0x44, 0x01, 0, ';'};
    for (size_t cut = 0; cut <= sizeof(good); cut++) {
        twin_pixmap_t *p = load_bytes(good, cut);
        if (p)
            twin_pixmap_destroy(p);
    }
    /* Reaching here is the check: every truncated GIF returned */
}
#endif

int main(void)
{
    test_damage_merge();
    test_occlusion_differential();
    test_row_fallback();
#if defined(CONFIG_DROP_SHADOW)
    test_shadow();
    test_shadow_mask();
#endif
    test_rgb16_and_opacity();
    test_stub();
    test_cache_equivalence();
    test_text();
    test_gradient();
    test_gradient_exact();
    test_tween();
    test_closure_churn();
    test_scroll();
    test_window_damage_rects();
    test_scaled_clip();
    test_rgb16_mask();
    test_rgb16_xform();
    test_translated_source();
    test_cleared_in_pass();
    test_text_clip_overshoot();
    test_destroyed_paint();
    test_accessors();
#if defined(CONFIG_WIDGETS)
    test_widget_teardown();
    test_toplevel_background();
    test_box_background();
#endif
#if defined(CONFIG_LOADER_GIF)
    test_gif_malformed();
    test_gif_frames();
#endif
#if defined(CONFIG_LOADER_TVG)
    test_tvg_hairline();
    test_tvg_color_index();
#endif
#if defined(CONFIG_LOADER_PNG)
    test_png_16bit();
#endif
    test_leaks();
    twin_cache_flush();
    return th_report("core");
}
