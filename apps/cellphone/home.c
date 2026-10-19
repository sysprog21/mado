/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Home screen: pages of app icons.
 *
 * Paging slides the icon grid by blitting the pixels already on screen and
 * painting only the exposed strip, so a swipe costs a memmove per frame
 * plus at most one column of icons, whose masks come from the coverage
 * cache.
 */

#include <stdlib.h>

#include "phone.h"

#define COLS 3
#define ROWS 3
#define PER_PAGE (COLS * ROWS)
#define CELL_W (PHONE_W / COLS)
#define CELL_H 74
#define PLATE 40
#define TILE_INSET 3 /* gap between tiles is twice this */
#define TILE_R 8
#define GRID_TOP 6
#define DOTS_Y (GRID_TOP + ROWS * CELL_H + 12)

typedef struct {
    ui_view_t view;
    int page;          /* settled page */
    twin_coord_t pan;  /* horizontal pan in px: page * PHONE_W - drag */
    twin_coord_t from; /* pan when an animation started */
    twin_coord_t to;
    twin_coord_t grab; /* pan a drag starts from */
    bool settling;     /* the settle tween is running */
    int pressed;       /* app index under the finger, -1 when none */
} home_t;

static int pages(void)
{
    return (phone_app_count() + PER_PAGE - 1) / PER_PAGE;
}

static ui_rect_t grid_rect(void)
{
    return ui_rect(0, GRID_TOP, PHONE_W, ROWS * CELL_H);
}

/* Cell of app @i on its page, in page-local coordinates */
static ui_rect_t cell_rect(int i)
{
    int k = i % PER_PAGE;
    return ui_rect((k % COLS) * CELL_W, GRID_TOP + (k / COLS) * CELL_H, CELL_W,
                   CELL_H);
}

/* Warm-white tile holding an app's icon and label: the touch target */
static ui_rect_t tile_rect(ui_rect_t cell)
{
    return ui_inset(cell, TILE_INSET);
}

static ui_rect_t plate_rect(ui_rect_t cell)
{
    return ui_rect(cell.left + (CELL_W - PLATE) / 2, cell.top + 9, PLATE,
                   PLATE);
}

static void home_paint(ui_view_t *v, twin_pixmap_t *px)
{
    home_t *h = (home_t *) v;
    ui_rect_t clip = ui_view_clip(v);
    const phone_theme_t *th = phone_theme();
    phone_paint_wallpaper(px, ui_view_bounds(v), phone_wallpaper());

    for (int i = 0; i < phone_app_count(); i++) {
        ui_rect_t cell = cell_rect(i);
        twin_coord_t shift = (i / PER_PAGE) * PHONE_W - h->pan;
        cell.left += shift;
        cell.right += shift;
        if (!ui_intersects(cell, clip))
            continue;
        ui_rect_t tile = tile_rect(cell), plate = plate_rect(cell);
        bool pressed = i == h->pressed;
        /* The ledge shows only along the rounded ends: rasterize those
         * bands, not the rows the tile covers */
        ui_rect_t ledge =
            ui_rect(tile.left, tile.top + 1, ui_w(tile), ui_h(tile));
        for (int band = 0; band < 2; band++) {
            twin_coord_t top = band ? tile.bottom - TILE_R : tile.top;
            twin_rect_t saved = ui_clip_push(
                px, ui_rect(tile.left, top, ui_w(tile), TILE_R + 1));
            ui_rrect(px, ledge, TILE_R, 0x1c000000);
            twin_pixmap_restore_clip(px, saved);
        }
        ui_rrect(px, tile, TILE_R,
                 pressed ? ui_mix(th->card, th->primary, 60) : th->card);
        phone_app_icon(px, i, plate, pressed);
        ui_text_fit(px, FONT_SM,
                    ui_rect(tile.left + 2, plate.bottom + 2, ui_w(tile) - 4,
                            tile.bottom - plate.bottom - 4),
                    UI_CENTER, th->text, phone_app_at(i)->name);
    }

    int current = (h->pan + PHONE_W / 2) / PHONE_W;
    ui_dots(px, PHONE_W / 2, DOTS_Y, pages(), current);
}

static int app_at(home_t *h, twin_coord_t x, twin_coord_t y)
{
    for (int i = h->page * PER_PAGE;
         i < phone_app_count() && i < (h->page + 1) * PER_PAGE; i++) {
        if (ui_hit(tile_rect(cell_rect(i)), x, y))
            return i;
    }
    return -1;
}

static void pan_to(home_t *h, twin_coord_t pan)
{
    twin_coord_t d = pan - h->pan;
    if (!d)
        return;
    h->pan = pan;
    ui_view_scroll(&h->view, grid_rect(), -d, 0);
    ui_view_invalidate(&h->view, ui_rect(0, DOTS_Y - 6, PHONE_W, 12));
}

static bool settle_step(ui_view_t *v, twin_fixed_t value)
{
    home_t *h = (home_t *) v;
    pan_to(h, ui_lerp(h->from, h->to, value));
    if (value == TWIN_FIXED_ONE)
        h->settling = false;
    return true;
}

/* Tile of @app; empty when the app's page is not shown */
static ui_rect_t shown_tile(ui_view_t *v, int app)
{
    if (app / PER_PAGE != ((home_t *) v)->page)
        return (ui_rect_t) {0, 0, 0, 0};
    return tile_rect(cell_rect(app));
}

static void set_pressed(home_t *h, int app)
{
    ui_view_set_pressed(&h->view, &h->pressed, app, shown_tile);
}

static bool home_event(ui_view_t *v, const ui_event_t *e)
{
    home_t *h = (home_t *) v;
    switch (e->kind) {
    case UI_PRESS:
        set_pressed(h, app_at(h, e->x, e->y));
        h->grab = h->pan;
        return true;
    case UI_DRAG: {
        set_pressed(h, -1);
        if (h->settling) {
            /* Catch the page mid-settle: continue from where it is */
            ui_view_stop_tweens(v);
            h->settling = false;
            h->grab = h->pan + e->dx;
        }
        twin_coord_t pan = h->grab - e->dx;
        twin_coord_t max = (pages() - 1) * PHONE_W;
        /* Rubber band past the first and last page */
        if (pan < 0)
            pan /= 3;
        else if (pan > max)
            pan = max + (pan - max) / 3;
        pan_to(h, pan);
        return true;
    }
    case UI_RELEASE: {
        int page = h->page;
        if ((e->dx < -PHONE_W / 4 || e->vx < -300) && page < pages() - 1)
            page++;
        else if ((e->dx > PHONE_W / 4 || e->vx > 300) && page > 0)
            page--;
        h->page = page;
        h->from = h->pan;
        h->to = page * PHONE_W;
        if (h->from != h->to) {
            h->settling = ui_view_tween(v, 220, TwinEaseOutCubic, settle_step);
            if (!h->settling)
                pan_to(h, h->to); /* no animation: settle at once */
        }
        return true;
    }
    case UI_TAP: {
        int app = app_at(h, e->x, e->y);
        set_pressed(h, -1);
        if (app >= 0)
            phone_open(app);
        return true;
    }
    case UI_CANCEL:
        set_pressed(h, -1);
        return true;
    default:
        return false;
    }
}

ui_view_t *home_create(void)
{
    home_t *h = ui_view_new(sizeof(*h), "Home", home_paint, home_event, NULL);
    if (!h)
        return NULL;
    h->pressed = -1;
    return &h->view;
}

/* Test hook: @app's tile in home-screen content coordinates; false unless
 * home is on top with that tile's page shown */
bool app_home_icon_rect(int app, ui_rect_t *out)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != home_paint || app < 0 || app >= phone_app_count())
        return false;
    *out = shown_tile(v, app);
    return out->right > out->left;
}
