/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Arcade: Snake (see games.c) */

#include <string.h>

#include "games-internal.h"

/*
 * Snake: steer by breadth-first search to the fruit, but only along moves
 * that leave at least the body's length of open cells reachable; failing
 * that, take the move with the most room.
 */

static const int8_t dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

static void snake_place_fruit(game_t *g)
{
    snake_t *s = &g->u.snake;
    int free_cells = SN_N - s->len;
    s->fx = -1;
    if (free_cells <= 0)
        return;
    int k = game_rnd_below(g, free_cells);
    for (int r = 0; r < SN_ROWS; r++)
        for (int c = 0; c < SN_COLS; c++)
            if (!s->occ[r][c] && k-- == 0) {
                s->fx = c;
                s->fy = r;
                game_mark_cell(g, c, r);
                return;
            }
}

static void snake_init(game_t *g)
{
    snake_t *s = &g->u.snake;
    memset(s, 0, sizeof(*s));
    s->len = SN_START;
    s->dx = 1;
    for (int i = 0; i < SN_START; i++) {
        s->head = i;
        s->bx[i] = (uint8_t) (SN_COLS / 2 - SN_START + 1 + i);
        s->by[i] = SN_ROWS / 2;
        s->occ[s->by[i]][s->bx[i]] = 1;
    }
    snake_place_fruit(g);
}

static int snake_tail(const snake_t *s)
{
    return (s->head - s->len + 1 + SN_N) % SN_N;
}

/* Flood fill from (c, r) with @blocked; returns the reachable cell count
 * and stores the distance to the fruit (-1 if unreachable) in *dist */
static int snake_flood(const snake_t *s,
                       uint8_t blocked[SN_ROWS][SN_COLS],
                       int c,
                       int r,
                       int *dist)
{
    int16_t queue[SN_N];
    int16_t depth[SN_N];
    int n = 0, head = 0;
    *dist = -1;
    queue[n] = (int16_t) (r * SN_COLS + c);
    depth[n++] = 0;
    blocked[r][c] = 2;
    while (head < n) {
        int at = queue[head], d = depth[head++];
        int x = at % SN_COLS, y = at / SN_COLS;
        if (x == s->fx && y == s->fy && *dist < 0)
            *dist = d;
        for (int i = 0; i < 4; i++) {
            int nx = x + dirs[i][0], ny = y + dirs[i][1];
            if (nx < 0 || ny < 0 || nx >= SN_COLS || ny >= SN_ROWS ||
                blocked[ny][nx])
                continue;
            blocked[ny][nx] = 2;
            queue[n] = (int16_t) (ny * SN_COLS + nx);
            depth[n++] = (int16_t) (d + 1);
        }
    }
    return n;
}

static void snake_tick(game_t *g)
{
    snake_t *s = &g->u.snake;
    int hx = s->bx[s->head], hy = s->by[s->head];
    int tail = snake_tail(s);
    int best = -1, best_key = 0;

    /* Try the current heading first so ties keep the snake going straight */
    int first = 0;
    for (int i = 0; i < 4; i++)
        if (dirs[i][0] == s->dx && dirs[i][1] == s->dy)
            first = i;
    for (int k = 0; k < 4; k++) {
        int i = (first + k) % 4;
        int nx = hx + dirs[i][0], ny = hy + dirs[i][1];
        if (nx < 0 || ny < 0 || nx >= SN_COLS || ny >= SN_ROWS)
            continue;
        bool eat = nx == s->fx && ny == s->fy;
        bool frees_tail = !eat && nx == s->bx[tail] && ny == s->by[tail];
        if (s->occ[ny][nx] && !frees_tail)
            continue;
        uint8_t blocked[SN_ROWS][SN_COLS];
        memcpy(blocked, s->occ, sizeof(blocked));
        if (!eat)
            blocked[s->by[tail]][s->bx[tail]] = 0;
        int dist;
        int room = snake_flood(s, blocked, nx, ny, &dist);
        int key = (dist >= 0 && room >= s->len) ? dist : 10000 - room;
        if (best < 0 || key < best_key) {
            best = i;
            best_key = key;
        }
    }
    if (best < 0) {
        game_restart(g);
        return;
    }

    s->dx = dirs[best][0];
    s->dy = dirs[best][1];
    int nx = hx + s->dx, ny = hy + s->dy;
    bool eat = nx == s->fx && ny == s->fy;
    if (!eat) {
        s->occ[s->by[tail]][s->bx[tail]] = 0;
        game_mark_cell(g, s->bx[tail], s->by[tail]);
    } else {
        s->len++;
    }
    s->head = (s->head + 1) % SN_N;
    s->bx[s->head] = (uint8_t) nx;
    s->by[s->head] = (uint8_t) ny;
    s->occ[ny][nx] = 1;
    game_mark_cell(g, hx, hy); /* old head turns into body */
    game_mark_cell(g, nx, ny);
    if (eat) {
        g->events++;
        game_set_score(g, g->score + 1);
        snake_place_fruit(g);
        if (s->fx < 0) /* board filled: a win, start over */
            game_restart(g);
    }
}

static void snake_paint(game_t *g, twin_pixmap_t *px, ui_rect_t clip)
{
    const snake_t *s = &g->u.snake;
    int c0, c1, r0, r1;
    int hx = s->bx[s->head], hy = s->by[s->head];
    game_cell_span(g, clip, &c0, &c1, &r0, &r1);
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            ui_rect_t cr = game_cell_rect(g, c, r);
            ui_fill(px, cr, ((c + r) & 1) ? C_CELL_A : C_CELL_B);
            if (c == s->fx && r == s->fy) {
                ui_circle(px, (cr.left + cr.right) / 2,
                          (cr.top + cr.bottom) / 2, SN_CELL / 2 - 1,
                          UI_RGB(0xe53935));
                ui_circle(px, cr.left + 5, cr.top + 4, 1,
                          ui_alpha(UI_RGB(0xffffff), 0x90));
            } else if (c == hx && r == hy) {
                ui_rrect(px, ui_inset(cr, 1), 4, UI_RGB(0xc5e1a5));
                /* Eyes look the way the snake is heading */
                int ex = (cr.left + cr.right) / 2 + s->dx * 3;
                int ey = (cr.top + cr.bottom) / 2 + s->dy * 3;
                ui_circle(px, ex - s->dy * 3, ey - s->dx * 3, 1,
                          UI_RGB(0x1b3a1d));
                ui_circle(px, ex + s->dy * 3, ey + s->dx * 3, 1,
                          UI_RGB(0x1b3a1d));
            } else if (s->occ[r][c]) {
                ui_rrect(px, ui_inset(cr, 1), 3, UI_RGB(0x66bb6a));
            }
        }
    }
}

const game_ops_t game_snake_ops = {
    "Snake", 130,         SN_COLS,    SN_ROWS,    SN_CELL,
    INNER_X, INNER_Y + 1, snake_init, snake_tick, snake_paint,
};
