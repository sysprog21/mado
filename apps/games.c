/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Arcade: Snake, Pong and Tetris playing themselves.
 *
 * One view type (this file) serves all three games; everything game
 * specific sits in a const ops table (geometry, init, tick, paint) defined
 * by each game's own file, which also opens it, so adding a game is one file
 * and a launcher row.  The board lives in small fixed arrays in the view,
 * and a tick damages only what it changed: the snake's head and tail cells,
 * the ball and paddle strips, the falling piece's old and new cells.  The
 * whole board is repainted only on a line clear or a restart.
 *
 * Window damage is a single bounding box, so two far-apart cells (snake head
 * and tail) would coalesce into a large rectangle.  game_mark() batches nearby
 * rectangles and paints a batch right away when the next one is far from
 * it, keeping the composited area close to the sum of the changed cells.
 *
 * Each open seeds its own PRNG with a fixed value, so a game replays the
 * same way every time; tests rely on that.
 */

#include <stdio.h>
#include <string.h>

#include "games.h"

/*
 * Helpers
 */

uint32_t game_rnd(game_t *g)
{
    /* xorshift32: tiny, deterministic, good enough for game choices */
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x;
}

int game_rnd_below(game_t *g, int n)
{
    return (int) (game_rnd(g) % (uint32_t) n);
}

ui_rect_t game_panel_rect(void)
{
    return ui_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
}

ui_rect_t game_inner_rect(void)
{
    return ui_inset(game_panel_rect(), 8);
}

static ui_rect_t score_rect(void)
{
    return ui_rect(PHONE_W / 2 - 24, 0, PHONE_W / 2 + 24, HEADER_H - 1);
}

/* The hint under the panel */
static ui_rect_t footer_rect(const ui_view_t *v)
{
    twin_coord_t top = game_panel_rect().bottom;
    return ui_rect(0, top, PHONE_W, ui_view_height(v) - top);
}

static ui_rect_t overlay_rect(void)
{
    ui_rect_t p = game_panel_rect();
    return ui_rect((p.left + p.right) / 2 - 64, (p.top + p.bottom) / 2 - 28,
                   128, 56);
}

static int area(ui_rect_t r)
{
    return ui_w(r) * ui_h(r);
}


static void flush(game_t *g, bool now)
{
    if (ui_empty(g->pend))
        return;
    ui_view_invalidate(&g->view, g->pend);
    g->pend = (ui_rect_t) {0, 0, 0, 0};
    /* Paint the batch before the next far-away one widens the window's
     * single damage box; the screen keeps a list, so both stay small. */
    if (now)
        twin_window_draw(g->view.window);
}

/* Damage @r, batching it with nearby rectangles */
void game_mark(game_t *g, ui_rect_t r)
{
    if (ui_empty(r))
        return;
    if (ui_empty(g->pend)) {
        g->pend = r;
        return;
    }
    ui_rect_t u = ui_rect_union(g->pend, r);
    if (area(u) * 2 <= (area(g->pend) + area(r)) * 3) {
        g->pend = u;
        return;
    }
    flush(g, true);
    g->pend = r;
}

ui_rect_t game_cell_rect(const game_t *g, int c, int r)
{
    const game_ops_t *o = g->ops;
    return ui_rect(o->ox + c * o->cell, o->oy + r * o->cell, o->cell, o->cell);
}

/* Cells-changed hook shared by the grid games */
void game_mark_cell(game_t *g, int c, int r)
{
    const game_ops_t *o = g->ops;
    if (c >= 0 && r >= 0 && c < o->cols && r < o->rows)
        game_mark(g, game_cell_rect(g, c, r));
}

/* Range of cells of the current grid that intersect @clip */
void game_cell_span(const game_t *g,
                    ui_rect_t clip,
                    int *c0,
                    int *c1,
                    int *r0,
                    int *r1)
{
    const game_ops_t *o = g->ops;
    int a = (clip.left - o->ox) / o->cell, b = (clip.right - o->ox - 1);
    int c = (clip.top - o->oy) / o->cell, d = (clip.bottom - o->oy - 1);
    b = b < 0 ? -1 : b / o->cell;
    d = d < 0 ? -1 : d / o->cell;
    *c0 = a < 0 ? 0 : a;
    *r0 = c < 0 ? 0 : c;
    *c1 = b >= o->cols ? o->cols - 1 : b;
    *r1 = d >= o->rows ? o->rows - 1 : d;
}

void game_set_score(game_t *g, int score)
{
    if (score == g->score)
        return;
    g->score = score;
    if (score > g->best)
        g->best = score;
    game_mark(g, score_rect());
}

/* Start a new round and repaint the board */
void game_restart(game_t *g)
{
    game_set_score(g, 0);
    g->ops->init(g);
    g->full++;
    game_mark(g, game_inner_rect());
}

/*
 * View
 */

static void game_paint(ui_view_t *v, twin_pixmap_t *px)
{
    game_t *g = (game_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t panel = game_panel_rect(), in = game_inner_rect();

    if (clip.top < HEADER_H) {
        char buf[40];
        snprintf(buf, sizeof(buf), "Score %d \xc2\xb7 Best %d", g->score,
                 g->best);
        ui_header(px, ui_rect(0, 0, PHONE_W, HEADER_H), NULL,
                  ui_view_titled(v) ? NULL : g->ops->name, buf);
    }
    /* Ticks damage cells inside the panel: fill just the clip there */
    if (ui_rect_contains(in, clip)) {
        ui_fill(px, clip, C_PANEL);
    } else {
        ui_fill(px, ui_rect(0, HEADER_H, PHONE_W, ui_view_height(v) - HEADER_H),
                th->bg);
        ui_rrect(px, panel, PANEL_R, C_PANEL);
        ui_rrect_stroke(px, panel, PANEL_R, 1, C_BORDER);
        ui_text_in(px, FONT_SM, footer_rect(v), UI_CENTER, th->text_sec,
                   g->paused ? "Paused \xc2\xb7 tap to play"
                             : "Auto-play \xc2\xb7 tap to pause");
    }
    g->ops->paint(g, px, clip);

    ui_rect_t o = overlay_rect();
    if (g->paused && ui_intersects(clip, o)) {
        ui_rrect(px, o, 12, ui_alpha(UI_RGB(0x000000), 0xc0));
        ui_text(px, FONT_HEADING, (o.left + o.right) / 2, o.top + 25, UI_CENTER,
                UI_RGB(0xffffff), "Paused");
        ui_text(px, FONT_SM, (o.left + o.right) / 2, o.top + 43, UI_CENTER,
                C_LABEL, "Tap to resume");
    }
}

static twin_time_t game_tick(ui_view_t *v)
{
    game_t *g = (game_t *) v;
    const game_ops_t *o = g->ops;
    /* Freeze while another window covers this one */
    if (g->paused || ui_view_covered(v))
        return o->period;
    o->tick(g);
    g->steps++;
    flush(g, false);
    return o->period;
}

static void set_paused(game_t *g, bool paused)
{
    if (g->paused == paused)
        return;
    g->paused = paused;
    ui_view_invalidate(&g->view, overlay_rect());
    ui_view_invalidate(&g->view, footer_rect(&g->view));
    /* No timer while paused: nothing to wake up for */
    if (paused)
        ui_view_stop_timers(&g->view);
    else
        ui_view_every(&g->view, g->ops->period, game_tick);
}

static bool game_event(ui_view_t *v, const ui_event_t *e)
{
    game_t *g = (game_t *) v;
    if (e->kind == UI_TAP ||
        (e->kind == UI_KEY && (e->ucs4 == ' ' || e->ucs4 == 'p'))) {
        set_paused(g, !g->paused);
        return true;
    }
    return false;
}

/* Set up @g, a fresh view (NULL when its allocation failed), and show it;
 * a @paused game waits for a tap before its first step */
static bool game_show(game_t *g, const game_ops_t *ops, bool paused)
{
    if (!g)
        return false;
    g->ops = ops;
    g->rng = 0x2545f491u * ops->seed;
    ops->init(g);
    g->pend = (ui_rect_t) {0, 0, 0, 0}; /* the first paint covers it all */
    g->paused = paused;
    if (!paused && !ui_view_every(&g->view, ops->period, game_tick)) {
        ui_view_close(&g->view);
        return false;
    }
    ui_view_show(&g->view);
    return true;
}

void game_open(const game_ops_t *ops)
{
    game_show(
        ui_view_new(sizeof(game_t), ops->name, game_paint, game_event, NULL),
        ops, false);
}

/* Opens paused: without the phone shell every registry app starts at boot,
 * and three games playing unasked would only burn cycles */
bool game_start(const game_ops_t *ops,
                twin_screen_t *screen,
                const char *name,
                int x,
                int y)
{
    return ui_init() &&
           game_show(ui_view_new_at(screen, x, y, sizeof(game_t), name,
                                    game_paint, game_event, NULL),
                     ops, true);
}

/*
 * Test hooks: state of the game on top, or -1 / false when none
 */
static game_t *top_game(void)
{
    ui_view_t *v = ui_view_of(ui_top_window());
    return v && v->paint == game_paint ? (game_t *) v : NULL;
}

int app_games_test_steps(void)
{
    game_t *g = top_game();
    return g ? g->steps : -1;
}

int app_games_test_score(void)
{
    game_t *g = top_game();
    return g ? g->score : -1;
}

int app_games_test_best(void)
{
    game_t *g = top_game();
    return g ? g->best : -1;
}

int app_games_test_events(void)
{
    game_t *g = top_game();
    return g ? g->events : -1;
}

int app_games_test_full(void)
{
    game_t *g = top_game();
    return g ? g->full : -1;
}

bool app_games_test_paused(void)
{
    game_t *g = top_game();
    return g && g->paused;
}
