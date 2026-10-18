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
 * by each game's own file, so adding a game is one file and one row below.  The
 * board lives in small fixed arrays in the view, and a tick damages only what
 * it changed: the snake's head and tail cells, the ball and paddle strips, the
 * falling piece's old and new cells. The whole board is repainted only on a
 * line clear or a restart.
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

#include "games-internal.h"

static const game_ops_t *const ops[GAME_COUNT] = {
    [GAME_SNAKE] = &game_snake_ops,
    [GAME_PONG] = &game_pong_ops,
    [GAME_TETRIS] = &game_tetris_ops,
};

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
    const game_ops_t *o = ops[g->kind];
    return ui_rect(o->ox + c * o->cell, o->oy + r * o->cell, o->cell, o->cell);
}

/* Cells-changed hook shared by the grid games */
void game_mark_cell(game_t *g, int c, int r)
{
    const game_ops_t *o = ops[g->kind];
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
    const game_ops_t *o = ops[g->kind];
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
    ops[g->kind]->init(g);
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
        ui_header(px, ui_rect(0, 0, PHONE_W, HEADER_H), ops[g->kind]->name,
                  buf);
    }
    /* Ticks damage cells inside the panel: fill just the clip there */
    if (ui_rect_contains(in, clip)) {
        ui_fill(px, clip, C_PANEL);
    } else {
        ui_fill(px, ui_rect(0, HEADER_H, PHONE_W, ui_view_height(v) - HEADER_H),
                th->bg);
        ui_rrect(px, panel, PANEL_R, C_PANEL);
        ui_rrect_stroke(px, panel, PANEL_R, 1, C_BORDER);
        ui_text(px, FONT_SM, PHONE_W / 2,
                ui_baseline(FONT_SM, panel.bottom,
                            ui_view_height(v) - panel.bottom),
                UI_CENTER, th->text_sec, "Auto-play \xc2\xb7 tap to pause");
    }
    ops[g->kind]->paint(g, px, clip);

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
    const game_ops_t *o = ops[g->kind];
    /* Freeze while another view covers this one.  Only the mobile scene
     * stacks views exclusively: desktop windows may sit side by side. */
    if (g->paused || (phone_top() != v &&
                      twin_screen_get_wm(phone_screen()) != TwinWmDesktop))
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
    /* No timer while paused: nothing to wake up for */
    if (paused)
        ui_view_stop_timers(&g->view);
    else
        ui_view_every(&g->view, ops[g->kind]->period, game_tick);
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

static void game_open(game_kind_t kind)
{
    game_t *g =
        ui_view_new(sizeof(*g), ops[kind]->name, game_paint, game_event, NULL);
    if (!g)
        return;
    g->kind = kind;
    g->rng = 0x2545f491u * (uint32_t) (kind + 1);
    ops[kind]->init(g);
    g->pend = (ui_rect_t) {0, 0, 0, 0}; /* the first paint covers it all */
    if (!ui_view_every(&g->view, ops[kind]->period, game_tick)) {
        ui_view_close(&g->view);
        return;
    }
    ui_view_show(&g->view);
}

void app_snake_open(void)
{
    game_open(GAME_SNAKE);
}

void app_pong_open(void)
{
    game_open(GAME_PONG);
}

void app_tetris_open(void)
{
    game_open(GAME_TETRIS);
}

/*
 * Test hooks: state of the game on top, or -1 / false when none
 */
static game_t *top_game(void)
{
    ui_view_t *v = phone_top();
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
