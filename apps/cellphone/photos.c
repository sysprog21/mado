/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Photos: a 3x2 thumbnail grid and a swipeable single-photo viewer.
 *
 * Memory is bounded on purpose: the six 74x74 thumbnails are decoded once
 * into a gallery shared (by reference count) between the grid and the
 * viewer, and the viewer holds at most one full-size picture.  That one is
 * decoded only after the push animation has settled, so opening a photo
 * never stalls the transition; until then the thumbnail is shown scaled up.
 *
 * Without a JPEG loader, or when a file is missing, every picture is
 * replaced by procedural art rendered into a pixmap of the same size, so
 * the rest of the code only ever deals with pixmaps.
 */

#include <stdio.h>

#include "phone.h"

#define N_PHOTOS 6
#define COLS 3
#define THUMB 74 /* thumbnail cell (and the size of the thumb JPEGs) */
#define GAP 4
#define GRID_LEFT ((PHONE_W - COLS * THUMB - (COLS - 1) * GAP) / 2)
#define GRID_TOP (PHONE_HEADER_H + PHONE_SECTION_H + 4)
#define CAPTION_H 18
#define THUMB_RADIUS 6
#define CAPTION_FONT 11 /* fits "Constitution" in a thumbnail */

#define BAR_H 26        /* viewer chrome bars */
#define DEFER_MS 260    /* push animation length plus a little slack */
#define SETTLE_MS 220   /* swipe settle animation */
#define SWIPE_FLING 300 /* px/s that flips a photo regardless of distance */
#define CHROME UI_RGB(0x1a1a1a)

typedef struct {
    const char *title;
    const char *stem;
} photo_t;

static const photo_t photos[N_PHOTOS] = {
    {"Portrait", "bob_kerrey_portrait"},
    {"Landscape", "landscape_1677"},
    {"Constitution", "us_constitution"},
    {"Solar Loops", "solar_magnetic_loops"},
    {"Apollo 16", "apollo16_jump"},
    {"Cupola", "iss_window"},
};


/*
 * Pixmap helpers
 */


/*
 * Draw @src into @r, scaled to cover it (centered, cropping the overflow).
 * The core's transformed composite maps destination pixels relative to the
 * clipped rectangle, so clip here first and fold the offset into the
 * matrix; otherwise a partial repaint would sample the wrong texels.
 */
static void blit_cover(twin_pixmap_t *dst, twin_pixmap_t *src, ui_rect_t r)
{
    twin_operand_t s = {.source_kind = TWIN_PIXMAP, .u.pixmap = src};
    twin_coord_t w = ui_w(r), h = ui_h(r);
    if (w <= 0 || h <= 0)
        return;
    twin_coord_t sw = twin_pixmap_width(src), sh = twin_pixmap_height(src);
    if (sw == w && sh == h) {
        twin_composite(dst, r.left, r.top, &s, 0, 0, NULL, 0, 0, TWIN_SOURCE, w,
                       h);
        return;
    }
    ui_rect_t c = ui_rect_intersect(r, twin_pixmap_get_clip(dst));
    if (c.left >= c.right || c.top >= c.bottom)
        return;
    /* Destination-to-source scale; the last texel row/column is excluded so
     * bilinear filtering never blends with the transparent outside */
    twin_fixed_t sx = twin_fixed_div(ui_fx(sw - 1), ui_fx(w));
    twin_fixed_t sy = twin_fixed_div(ui_fx(sh - 1), ui_fx(h));
    twin_fixed_t k = sx < sy ? sx : sy;
    twin_fixed_t x0 = (ui_fx(sw - 1) - k * w) / 2;
    twin_fixed_t y0 = (ui_fx(sh - 1) - k * h) / 2;
    twin_matrix_t m = {
        {{k, 0},
         {0, k},
         {x0 + k * (c.left - r.left), y0 + k * (c.top - r.top)}}};
    twin_pixmap_set_transform(src, &m);
    twin_composite(dst, c.left, c.top, &s, 0, 0, NULL, 0, 0, TWIN_SOURCE,
                   ui_w(c), ui_h(c));
    twin_pixmap_set_transform(src, NULL);
}

/*
 * Procedural stand-ins, one per photo, drawn in permille coordinates of
 * @r so the same art serves as a thumbnail and as a full-size picture.
 */
#define AX(v) (r.left + ui_w(r) * (v) / 1000)
#define AY(v) (r.top + ui_h(r) * (v) / 1000)
#define AS(v) (unit * (v) / 1000)

static void ellipse(twin_pixmap_t *px,
                    twin_coord_t cx,
                    twin_coord_t cy,
                    int rx,
                    int ry,
                    twin_argb32_t c)
{
    twin_path_t *p = ui_path();
    twin_path_ellipse(p, ui_fx(cx), ui_fx(cy), ui_fx(rx), ui_fx(ry));
    twin_paint_path(px, c, p);
    ui_path_done(p);
}

static void arc_stroke(twin_pixmap_t *px,
                       twin_coord_t cx,
                       twin_coord_t cy,
                       int rx,
                       int ry,
                       int width,
                       twin_argb32_t c)
{
    twin_path_t *p = ui_path();
    twin_path_move(p, ui_fx(cx - rx), ui_fx(cy));
    twin_path_arc(p, ui_fx(cx), ui_fx(cy), ui_fx(rx), ui_fx(ry), TWIN_ANGLE_180,
                  TWIN_ANGLE_180);
    twin_paint_stroke(px, c, p,
                      ui_fx(width) > 0 ? ui_fx(width) : TWIN_FIXED_HALF);
    ui_path_done(p);
}

static void paint_art(twin_pixmap_t *px, ui_rect_t r, int idx)
{
    int w = ui_w(r), h = ui_h(r);
    int unit = w < h ? w : h;
    switch (idx) {
    case 0: /* Portrait: studio backdrop, suit and tie, head */
        ui_vgrad(px, r, UI_RGB(0x6f86a6), UI_RGB(0x2b3a52));
        ui_fill(px,
                ui_rect(AX(500) - AS(55), AY(480), 2 * AS(55), h * 15 / 100),
                UI_RGB(0xc99a7c));
        ellipse(px, AX(500), AY(1020), w * 50 / 100, h * 42 / 100,
                UI_RGB(0x1d2430));
        ellipse(px, AX(500), AY(640), AS(80), AS(80), UI_RGB(0xeeeeee));
        ellipse(px, AX(500), AY(720), AS(26), AS(120), UI_RGB(0x7a1f2b));
        ellipse(px, AX(500), AY(300), AS(150), AS(110), UI_RGB(0x3b2a20));
        ellipse(px, AX(500), AY(390), AS(125), AS(160), UI_RGB(0xd9a989));
        break;
    case 1: /* Landscape (1677): warm sky, hills, a tree */
        ui_vgrad(px, r, UI_RGB(0xb9c4c2), UI_RGB(0xead8a8));
        ellipse(px, AX(250), AY(760), w * 60 / 100, h * 22 / 100,
                UI_RGB(0x8d8c62));
        ellipse(px, AX(850), AY(820), w * 55 / 100, h * 24 / 100,
                UI_RGB(0x6b6a45));
        ui_fill(px, ui_rect(r.left, AY(850), w, h - h * 850 / 1000),
                UI_RGB(0x4a4428));
        ui_fill(px, ui_rect(AX(300) - AS(20), AY(470), AS(40), h * 40 / 100),
                UI_RGB(0x3a2c1c));
        ellipse(px, AX(300), AY(400), AS(210), AS(170), UI_RGB(0x3e4a27));
        ellipse(px, AX(220), AY(470), AS(130), AS(110), UI_RGB(0x4a5730));
        break;
    case 2: { /* Constitution: parchment with a heading and columns */
        ui_vgrad(px, r, UI_RGB(0xefe2c0), UI_RGB(0xd2bc88));
        int size = unit >= 160 ? FONT_LARGE : 0;
        int top = 180;
        if (size) {
            ui_text(px, size, AX(500), AY(170), UI_CENTER, UI_RGB(0x4a3b22),
                    "We the People");
            top = 240;
        } else {
            ui_fill(px, ui_rect(AX(150), AY(120), w * 70 / 100, AS(50)),
                    UI_RGB(0x5a4a2c));
        }
        twin_coord_t lh = h / (unit >= 160 ? 40 : 22);
        for (int col = 0; col < 3; col++) {
            twin_coord_t x = AX(80 + col * 290);
            for (twin_coord_t y = AY(top); y < AY(920); y += lh * 2) {
                int len = 200 + (int) ((y * 7 + col * 13) % 60);
                ui_fill(px, ui_rect(x, y, w * len / 1000, lh > 1 ? lh / 2 : 1),
                        UI_RGB(0x8a7650));
            }
        }
        break;
    }
    case 3: /* Solar loops: dark corona over a glowing limb */
        ui_vgrad(px, r, UI_RGB(0x140400), UI_RGB(0x4a1500));
        for (int i = 0; i < 4; i++)
            arc_stroke(px, AX(280 + i * 140), AY(700), AS(160 + i * 40),
                       AS(300 + i * 60), unit / 50,
                       ui_alpha(UI_RGB(0xffb347), 200 - i * 30));
        ellipse(px, AX(500), AY(1350), w * 95 / 100, h * 55 / 100,
                UI_RGB(0xe8590c));
        ellipse(px, AX(500), AY(1400), w * 85 / 100, h * 55 / 100,
                UI_RGB(0xff8c2a));
        break;
    case 4: /* Apollo 16: black sky, regolith, astronaut and flag */
        ui_fill(px, r, UI_RGB(0x05070a));
        ui_vgrad(px, ui_rect(r.left, AY(640), w, h - h * 640 / 1000),
                 UI_RGB(0x9a9a94), UI_RGB(0x55554f));
        ui_fill(px, ui_rect(AX(250), AY(250), AS(14) + 1, h * 45 / 100),
                UI_RGB(0xb0b0b0));
        ui_fill(px, ui_rect(AX(250), AY(250), AS(220), AS(140)),
                UI_RGB(0xb22234));
        ui_fill(px, ui_rect(AX(250), AY(250), AS(100), AS(80)),
                UI_RGB(0x3c3b6e));
        ui_rrect(px, ui_rect(AX(590), AY(400), AS(200), AS(260)), AS(60),
                 UI_RGB(0xf2f2ee));
        ellipse(px, AX(590) + AS(100), AY(400) - AS(40), AS(95), AS(95),
                UI_RGB(0xf2f2ee));
        ellipse(px, AX(590) + AS(100), AY(400) - AS(35), AS(65), AS(55),
                UI_RGB(0xc8901a));
        ellipse(px, AX(640), AY(1000) - AS(20), AS(240), AS(30),
                ui_alpha(UI_BLACK, 90));
        break;
    default: /* Cupola: earth below, window frame around */
        ui_fill(px, r, UI_RGB(0x0b0f1a));
        ellipse(px, AX(500), AY(1250), w * 90 / 100, h * 62 / 100,
                UI_RGB(0x8ec5f0));
        ellipse(px, AX(500), AY(1270), w * 88 / 100, h * 62 / 100,
                UI_RGB(0x2a6fb5));
        ellipse(px, AX(380), AY(820), AS(200), AS(50), ui_alpha(UI_WHITE, 170));
        ellipse(px, AX(680), AY(900), AS(160), AS(40), ui_alpha(UI_WHITE, 140));
        ui_rrect_stroke(px, ui_inset(r, AS(60)), AS(90), AS(70) + 1,
                        UI_RGB(0x8a8f99));
        break;
    }
}

#undef AX
#undef AY
#undef AS

/* Render the procedural stand-in for @idx at w x h */
static twin_pixmap_t *make_art(int idx, twin_coord_t w, twin_coord_t h)
{
    twin_pixmap_t *pix = twin_pixmap_create(TWIN_ARGB32, w, h);
    if (pix)
        paint_art(pix, ui_rect(0, 0, w, h), idx);
    return pix;
}

/*
 * Decode a photo derivative.  Paths are relative to the working directory;
 * the WebAssembly build mounts the assets at /assets.
 */
static twin_pixmap_t *load_jpeg(const char *dir, const char *stem)
{
#if defined(CONFIG_LOADER_JPEG)
    static const char *const roots[] = {"assets/photos", "/assets/photos"};
    char path[96];
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s/%s_%s.jpg", roots[i], dir, stem,
                 dir[0] == 't' ? "thumb" : "view");
        twin_pixmap_t *pix = twin_pixmap_from_file(path);
        if (pix)
            return pix;
    }
#else
    (void) dir;
    (void) stem;
#endif
    return NULL;
}

/* Photo @idx at exactly w x h: decoded and resized, or procedural */
static twin_pixmap_t *load_photo(int idx,
                                 const char *dir,
                                 twin_coord_t w,
                                 twin_coord_t h)
{
    twin_pixmap_t *pix = load_jpeg(dir, photos[idx].stem);
    if (pix && (twin_pixmap_width(pix) != w || twin_pixmap_height(pix) != h ||
                twin_pixmap_animation(pix))) {
        /* Normalize once here so painting never scales a full picture */
        twin_pixmap_t *fit = twin_pixmap_create(TWIN_ARGB32, w, h);
        if (fit)
            blit_cover(fit, pix, ui_rect(0, 0, w, h));
        twin_pixmap_destroy(pix);
        pix = fit;
    }
    return pix ? pix : make_art(idx, w, h);
}

/*
 * Gallery: thumbnails and the rounded-corner mask, shared by the grid and
 * the viewer and freed when the last of them closes.
 */
static struct {
    int refs;
    twin_pixmap_t *thumb[N_PHOTOS];
    twin_pixmap_t *mask; /* A8 rounded rectangle, THUMB x THUMB */
} gallery;

static void gallery_acquire(void)
{
    if (gallery.refs++)
        return;
    for (int i = 0; i < N_PHOTOS; i++)
        gallery.thumb[i] = load_photo(i, "thumbs", THUMB, THUMB);
    gallery.mask = twin_pixmap_create(TWIN_A8, THUMB, THUMB);
    if (gallery.mask) {
        twin_fill(gallery.mask, 0, TWIN_SOURCE, 0, 0, THUMB, THUMB);
        twin_path_t *p = ui_path();
        twin_path_rounded_rectangle(p, 0, 0, ui_fx(THUMB), ui_fx(THUMB),
                                    ui_fx(THUMB_RADIUS), ui_fx(THUMB_RADIUS));
        twin_paint_path(gallery.mask, UI_BLACK, p);
        ui_path_done(p);
    }
}

static void gallery_release(void)
{
    if (--gallery.refs)
        return;
    for (int i = 0; i < N_PHOTOS; i++) {
        if (gallery.thumb[i])
            twin_pixmap_destroy(gallery.thumb[i]);
        gallery.thumb[i] = NULL;
    }
    if (gallery.mask)
        twin_pixmap_destroy(gallery.mask);
    gallery.mask = NULL;
}

/* Fill the thumbnail-shaped area at (x, y) with @c (rounded if possible) */
static void masked_fill(twin_pixmap_t *px,
                        twin_coord_t x,
                        twin_coord_t y,
                        twin_coord_t my,
                        twin_coord_t h,
                        twin_argb32_t c)
{
    twin_operand_t s = {.source_kind = TWIN_SOLID, .u.argb = c};
    twin_operand_t m = {.source_kind = TWIN_PIXMAP, .u.pixmap = gallery.mask};
    twin_composite(px, x, y, &s, 0, 0, gallery.mask ? &m : NULL, 0, my,
                   TWIN_OVER, THUMB, h);
}

/*
 * Grid
 */
typedef struct {
    ui_view_t view;
    int pressed; /* thumbnail under the finger, -1 when none */
} grid_t;

static void viewer_open(int idx);

static ui_rect_t thumb_rect(int i)
{
    return ui_rect(GRID_LEFT + (i % COLS) * (THUMB + GAP),
                   GRID_TOP + (i / COLS) * (THUMB + GAP), THUMB, THUMB);
}

static int thumb_at(twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < N_PHOTOS; i++)
        if (ui_hit(thumb_rect(i), x, y))
            return i;
    return -1;
}

static void grid_paint(ui_view_t *v, twin_pixmap_t *px)
{
    grid_t *g = (grid_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t head = ui_rect(0, 0, PHONE_W, PHONE_HEADER_H);
    ui_rect_t sect = ui_rect(0, PHONE_HEADER_H, PHONE_W, PHONE_SECTION_H);

    ui_fill(px, ui_view_bounds(v), th->bg);
    if (ui_intersects(clip, head))
        ui_header(px, head, "Photos", "6 items");
    if (ui_intersects(clip, sect))
        ui_section(px, sect, "Recents");

    for (int i = 0; i < N_PHOTOS; i++) {
        ui_rect_t r = thumb_rect(i);
        if (!ui_intersects(clip, r))
            continue;
        twin_pixmap_t *t = gallery.thumb[i];
        if (t) {
            twin_operand_t s = {.source_kind = TWIN_PIXMAP, .u.pixmap = t};
            twin_operand_t m = {.source_kind = TWIN_PIXMAP,
                                .u.pixmap = gallery.mask};
            twin_composite(px, r.left, r.top, &s, 0, 0,
                           gallery.mask ? &m : NULL, 0, 0, TWIN_OVER, THUMB,
                           THUMB);
        } else {
            ui_rrect(px, r, THUMB_RADIUS, th->card);
        }
        /* Caption on a translucent shade along the bottom edge */
        masked_fill(px, r.left, r.bottom - CAPTION_H, THUMB - CAPTION_H,
                    CAPTION_H, ui_alpha(UI_BLACK, 120));
        ui_text_fit(
            px, CAPTION_FONT,
            ui_rect(r.left + 1, r.bottom - CAPTION_H, THUMB - 2, CAPTION_H),
            UI_LEFT, UI_WHITE, photos[i].title);
        if (i == g->pressed)
            masked_fill(px, r.left, r.top, 0, THUMB, ui_alpha(UI_BLACK, 80));
    }

    ui_rect_t foot =
        ui_rect(0, thumb_rect(N_PHOTOS - 1).bottom + 8, PHONE_W, 24);
    if (ui_intersects(clip, foot))
        ui_text_in(px, FONT_SM, foot, UI_CENTER, th->text_sec,
                   "Public domain photos");
}

static ui_rect_t grid_thumb(ui_view_t *v, int i)
{
    (void) v;
    return thumb_rect(i);
}

static void grid_set_pressed(grid_t *g, int i)
{
    ui_view_set_pressed(&g->view, &g->pressed, i, grid_thumb);
}

static bool grid_event(ui_view_t *v, const ui_event_t *e)
{
    grid_t *g = (grid_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        grid_set_pressed(g, thumb_at(e->x, e->y));
        return true;
    case UI_TAP: {
        int i = thumb_at(e->x, e->y);
        grid_set_pressed(g, -1);
        if (i >= 0)
            viewer_open(i);
        return true;
    }
    default:
        grid_set_pressed(g, -1);
        return false;
    }
}

static void grid_destroy(ui_view_t *v)
{
    (void) v;
    gallery_release();
}

void app_photos_open(void)
{
    grid_t *g =
        ui_view_new(sizeof(*g), "Photos", grid_paint, grid_event, grid_destroy);
    if (!g)
        return;
    g->pressed = -1;
    gallery_acquire();
    ui_view_show(&g->view);
}

/*
 * Viewer
 */
typedef struct {
    ui_view_t view;
    int idx;               /* settled photo */
    twin_coord_t pan;      /* horizontal offset: > 0 shows the next photo */
    twin_coord_t from, to; /* settle animation */
    bool settling;
    bool chrome;          /* title and dot bars visible */
    twin_pixmap_t *hires; /* full-size picture of hires_idx, or NULL */
    int hires_idx;
} viewer_t;

/* Full-size pictures alive across all viewers (test hook) */
static int hires_alive;

static ui_rect_t top_bar(void)
{
    return ui_rect(0, 0, PHONE_W, BAR_H);
}

static ui_rect_t bottom_bar(const viewer_t *vw)
{
    return ui_rect(0, ui_view_height(&vw->view) - BAR_H, PHONE_W, BAR_H);
}

/* Area whose pixels can be blitted while panning (chrome stays put) */
static ui_rect_t pan_area(viewer_t *vw)
{
    twin_coord_t h = ui_view_height(&vw->view);
    if (!vw->chrome)
        return ui_rect(0, 0, PHONE_W, h);
    return ui_rect(0, BAR_H, PHONE_W, h - 2 * BAR_H);
}

static void drop_hires(viewer_t *vw)
{
    if (!vw->hires)
        return;
    twin_pixmap_destroy(vw->hires);
    vw->hires = NULL;
    vw->hires_idx = -1;
    hires_alive--;
}

/* Photo shown under the dots while panning: the one covering the center */
static int shown_index(viewer_t *vw)
{
    int i = vw->idx + (vw->pan >= 0 ? (vw->pan + PHONE_W / 2) / PHONE_W
                                    : -((-vw->pan + PHONE_W / 2) / PHONE_W));
    return i < 0 ? 0 : i >= N_PHOTOS ? N_PHOTOS - 1 : i;
}

static void paint_dots(twin_pixmap_t *px, twin_coord_t cy, int current)
{
    twin_coord_t x = PHONE_W / 2 - (N_PHOTOS * 10 + 6) / 2;
    for (int i = 0; i < N_PHOTOS; i++) {
        if (i == current) {
            ui_rrect(px, ui_rect(x, cy - 3, 14, 6), 3, UI_WHITE);
            x += 18;
        } else {
            ui_circle(px, x + 3, cy, 3, ui_alpha(UI_WHITE, 110));
            x += 10;
        }
    }
}

static void viewer_paint(ui_view_t *v, twin_pixmap_t *px)
{
    viewer_t *vw = (viewer_t *) v;
    ui_rect_t clip = ui_view_clip(v);

    ui_fill(px, ui_view_bounds(v), UI_BLACK);
    for (int k = -1; k <= 1; k++) {
        int i = vw->idx + k;
        ui_rect_t r =
            ui_rect(k * PHONE_W - vw->pan, 0, PHONE_W, ui_view_height(v));
        if (i < 0 || i >= N_PHOTOS || !ui_intersects(clip, r))
            continue;
        if (vw->hires && vw->hires_idx == i)
            blit_cover(px, vw->hires, r);
        else if (gallery.thumb[i])
            blit_cover(px, gallery.thumb[i], r);
    }
    if (!vw->chrome)
        return;

    int shown = shown_index(vw);
    if (ui_intersects(clip, top_bar())) {
        char count[16];
        snprintf(count, sizeof(count), "%d / %d", shown + 1, N_PHOTOS);
        ui_fill(px, top_bar(), ui_alpha(CHROME, 200));
        ui_text_in(px, FONT_SM, ui_rect(4, 0, PHONE_W - 60, BAR_H), UI_LEFT,
                   UI_WHITE, photos[shown].title);
        ui_text_in(px, FONT_SM, ui_rect(PHONE_W - 60, 0, 56, BAR_H), UI_RIGHT,
                   UI_WHITE, count);
    }
    if (ui_intersects(clip, bottom_bar(vw))) {
        ui_fill(px, bottom_bar(vw), ui_alpha(CHROME, 200));
        paint_dots(px, ui_view_height(v) - BAR_H / 2, shown);
    }
}

/* Deferred decode of the full-size picture for the settled photo */
static twin_time_t viewer_load(ui_view_t *v)
{
    viewer_t *vw = (viewer_t *) v;
    /* Never decode under a moving finger or animation: try again shortly */
    if (v->pressed || vw->settling)
        return 60;
    if (vw->hires_idx == vw->idx)
        return -1;
    drop_hires(vw);
    vw->hires = load_photo(vw->idx, "view", PHONE_W, ui_view_height(v));
    if (vw->hires) {
        vw->hires_idx = vw->idx;
        hires_alive++;
        ui_view_invalidate(v, pan_area(vw));
        if (vw->chrome) {
            ui_view_invalidate(v, top_bar());
            ui_view_invalidate(v, bottom_bar(vw));
        }
    }
    return -1;
}

static void arm_load(viewer_t *vw)
{
    /* The viewer's only timer, so stopping all of them re-arms it */
    ui_view_stop_timers(&vw->view);
    ui_view_every(&vw->view, DEFER_MS, viewer_load);
}

static void pan_to(viewer_t *vw, twin_coord_t pan)
{
    twin_coord_t d = pan - vw->pan;
    if (!d)
        return;
    vw->pan = pan;
    ui_view_scroll(&vw->view, pan_area(vw), -d, 0);
    if (vw->chrome) {
        /* Bars sit over the moving photo, so they repaint every step */
        ui_view_invalidate(&vw->view, top_bar());
        ui_view_invalidate(&vw->view, bottom_bar(vw));
    }
}

static bool settle_step(ui_view_t *v, twin_fixed_t value)
{
    viewer_t *vw = (viewer_t *) v;
    pan_to(vw, ui_lerp(vw->from, vw->to, value));
    if (value < TWIN_FIXED_ONE)
        return true;
    vw->settling = false;
    if (vw->to) {
        /* Same pixels, new reference photo: no repaint needed */
        vw->idx += vw->to > 0 ? 1 : -1;
        vw->pan = 0;
        if (vw->hires_idx != vw->idx)
            drop_hires(vw);
        arm_load(vw);
    }
    return true;
}

static bool viewer_event(ui_view_t *v, const ui_event_t *e)
{
    viewer_t *vw = (viewer_t *) v;
    if (e->kind == UI_RESIZE) {
        /* The full-size picture was decoded for the old height */
        drop_hires(vw);
        arm_load(vw);
        return true;
    }
    if (vw->settling)
        return true; /* let the settle finish before the next gesture */
    switch (e->kind) {
    case UI_DRAG: {
        twin_coord_t pan = -e->dx;
        /* Rubber band past the first and last photo */
        if ((pan < 0 && vw->idx == 0) || (pan > 0 && vw->idx == N_PHOTOS - 1))
            pan /= 3;
        if (pan > PHONE_W)
            pan = PHONE_W;
        else if (pan < -PHONE_W)
            pan = -PHONE_W;
        pan_to(vw, pan);
        return true;
    }
    case UI_RELEASE:
    case UI_CANCEL: {
        twin_coord_t to = 0;
        if (e->kind == UI_RELEASE) {
            if ((e->dx < -PHONE_W / 4 || e->vx < -SWIPE_FLING) &&
                vw->idx < N_PHOTOS - 1)
                to = PHONE_W;
            else if ((e->dx > PHONE_W / 4 || e->vx > SWIPE_FLING) &&
                     vw->idx > 0)
                to = -PHONE_W;
        }
        vw->from = vw->pan;
        vw->to = to;
        if (vw->from != vw->to) {
            vw->settling = true;
            if (!ui_view_tween(v, SETTLE_MS, TwinEaseOutCubic, settle_step)) {
                vw->settling = false;
                settle_step(v, TWIN_FIXED_ONE);
            }
        }
        return true;
    }
    case UI_TAP:
        /* Tap toggles the chrome for an unobstructed look */
        vw->chrome = !vw->chrome;
        ui_view_invalidate_all(v);
        return true;
    default:
        return e->kind != UI_KEY;
    }
}

static void viewer_destroy(ui_view_t *v)
{
    drop_hires((viewer_t *) v);
    gallery_release();
}

static void viewer_open(int idx)
{
    viewer_t *vw = ui_view_new(sizeof(*vw), "Photo", viewer_paint, viewer_event,
                               viewer_destroy);
    if (!vw)
        return;
    vw->idx = idx;
    vw->chrome = true;
    vw->hires_idx = -1;
    gallery_acquire();
    ui_view_show(&vw->view);
    arm_load(vw);
}

/*
 * Test hooks
 */
static viewer_t *top_viewer(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == viewer_paint ? (viewer_t *) v : NULL;
}

/* Photo index in the viewer on top, -1 when the viewer is not on top */
int app_photos_test_index(void)
{
    viewer_t *vw = top_viewer();
    return vw ? vw->idx : -1;
}

/* Whether the viewer on top shows the full-size picture */
bool app_photos_test_hires(void)
{
    viewer_t *vw = top_viewer();
    return vw && vw->hires && vw->hires_idx == vw->idx;
}

/* Full-size pictures currently alive */
int app_photos_test_hires_count(void)
{
    return hires_alive;
}

/* Whether the thumbnail grid is on top */
bool app_photos_test_grid_open(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == grid_paint;
}
