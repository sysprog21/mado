/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Arcade: Tetris (see games.c) */

#include <string.h>

#include "games-internal.h"

/*
 * Tetris: on spawn, score every rotation and column by the resting
 * position's cleared lines, aggregate height, holes and bumpiness, then
 * steer there one move per tick while the piece falls.
 */

static const twin_argb32_t tt_colors[7] = {
    UI_RGB(0x4dd0e1), UI_RGB(0xffd54f), UI_RGB(0xba68c8), UI_RGB(0x81c784),
    UI_RGB(0xe57373), UI_RGB(0xffb74d), UI_RGB(0x7986cb),
};

/* 4x4 bitmasks, row-major from the top bit: I O T S Z L J */
static const uint16_t tt_pieces[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, {0x6600, 0x6600, 0x6600, 0x6600},
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, {0x06C0, 0x8C40, 0x6C00, 0x4620},
    {0x0C60, 0x4C80, 0xC600, 0x2640}, {0x0E80, 0xC440, 0x2E00, 0x44C0},
    {0x0E20, 0x44C0, 0x8E00, 0xC880},
};

static bool tt_bit(int piece, int rot, int r, int c)
{
    return (tt_pieces[piece][rot] >> (15 - (r * 4 + c))) & 1;
}

static bool tt_collides(const tetris_t *t, int piece, int rot, int x, int y)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (!tt_bit(piece, rot, r, c))
                continue;
            int fx = x + c, fy = y + r;
            if (fx < 0 || fx >= TT_COLS || fy >= TT_ROWS)
                return true;
            if (fy >= 0 && t->field[fy][fx])
                return true;
        }
    return false;
}

static void tt_stamp(tetris_t *t, int piece, int rot, int x, int y, uint8_t v)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (tt_bit(piece, rot, r, c) && y + r >= 0)
                t->field[y + r][x + c] = v;
}

static int tt_full_rows(const tetris_t *t)
{
    int n = 0;
    for (int r = 0; r < TT_ROWS; r++) {
        int c = 0;
        while (c < TT_COLS && t->field[r][c])
            c++;
        n += c == TT_COLS;
    }
    return n;
}

static int tt_evaluate(const tetris_t *t)
{
    int heights = 0, holes = 0, bumps = 0, prev = -1;
    for (int c = 0; c < TT_COLS; c++) {
        int h = 0;
        for (int r = 0; r < TT_ROWS; r++) {
            if (t->field[r][c]) {
                if (!h)
                    h = TT_ROWS - r;
            } else if (h) {
                holes++;
            }
        }
        heights += h;
        if (prev >= 0)
            bumps += h > prev ? h - prev : prev - h;
        prev = h;
    }
    /* Weights after the well-known four-feature heuristic, scaled */
    return tt_full_rows(t) * 76 - heights * 51 - holes * 36 - bumps * 18;
}

static void tt_choose(tetris_t *t)
{
    int best = 0;
    bool found = false;
    t->target_x = t->x;
    t->target_rot = t->rot;
    for (int rot = 0; rot < 4; rot++) {
        for (int x = -2; x < TT_COLS; x++) {
            if (tt_collides(t, t->piece, rot, x, t->y))
                continue;
            int y = t->y;
            while (!tt_collides(t, t->piece, rot, x, y + 1))
                y++;
            tt_stamp(t, t->piece, rot, x, y, 1);
            int score = tt_evaluate(t);
            tt_stamp(t, t->piece, rot, x, y, 0);
            if (!found || score > best) {
                found = true;
                best = score;
                t->target_x = x;
                t->target_rot = rot;
            }
        }
    }
}

static ui_rect_t tt_preview_rect(void)
{
    ui_rect_t in = game_inner_rect();
    twin_coord_t cx = (TT_SIDE_X + in.right) / 2;
    return ui_rect(cx - 2 * TT_PREV_CELL - 4, TT_Y + 22, 4 * TT_PREV_CELL + 8,
                   4 * TT_PREV_CELL + 8);
}

/* Next piece in; returns false when it cannot enter (game over) */
static bool tt_spawn(game_t *g)
{
    tetris_t *t = &g->u.tetris;
    t->piece = t->next;
    t->next = game_rnd_below(g, 7);
    t->rot = 0;
    t->x = TT_COLS / 2 - 2;
    t->y = -1;
    game_mark(g, tt_preview_rect());
    if (tt_collides(t, t->piece, t->rot, t->x, t->y))
        return false;
    tt_choose(t);
    return true;
}

static void tetris_init(game_t *g)
{
    tetris_t *t = &g->u.tetris;
    memset(t, 0, sizeof(*t));
    t->next = game_rnd_below(g, 7);
    tt_spawn(g);
}

/* Damage the cells of @piece at (x, y); call after the state changed so
 * a batch painted early by game_mark() never shows a stale position */
static void tt_mark_piece(game_t *g, int piece, int rot, int x, int y)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (tt_bit(piece, rot, r, c))
                game_mark_cell(g, x + c, y + r);
}

static void tetris_tick(game_t *g)
{
    tetris_t *t = &g->u.tetris;
    int piece = t->piece, rot = t->rot, x = t->x, y = t->y;
    if (t->rot != t->target_rot &&
        !tt_collides(t, t->piece, (t->rot + 1) % 4, t->x, t->y))
        t->rot = (t->rot + 1) % 4;
    int step = t->x < t->target_x ? 1 : t->x > t->target_x ? -1 : 0;
    if (step && !tt_collides(t, t->piece, t->rot, t->x + step, t->y))
        t->x += step;
    /* Soft drop once lined up: two rows per step */
    int fall = t->rot == t->target_rot && t->x == t->target_x ? 2 : 1;
    while (fall-- && !tt_collides(t, t->piece, t->rot, t->x, t->y + 1))
        t->y++;
    bool landed = t->y == y; /* rested a step without falling */
    tt_mark_piece(g, piece, rot, x, y);
    tt_mark_piece(g, t->piece, t->rot, t->x, t->y);
    if (!landed)
        return;

    /* Landed: lock it in, clear full rows, bring in the next piece */
    tt_stamp(t, t->piece, t->rot, t->x, t->y, (uint8_t) (t->piece + 1));
    g->events++;
    int cleared = 0;
    for (int r = TT_ROWS - 1; r >= 0; r--) {
        int c = 0;
        while (c < TT_COLS && t->field[r][c])
            c++;
        if (c < TT_COLS)
            continue;
        memmove(t->field[1], t->field[0], (size_t) r * TT_COLS);
        memset(t->field[0], 0, TT_COLS);
        cleared++;
        r++;
    }
    if (!tt_spawn(g)) {
        game_restart(g);
        return;
    }
    if (cleared) {
        game_set_score(g, g->score + cleared);
        g->full++;
        game_mark(g, game_inner_rect());
    } else {
        tt_mark_piece(g, t->piece, t->rot, t->x, t->y);
    }
}

static void tt_cell(twin_pixmap_t *px, ui_rect_t r, twin_argb32_t color)
{
    ui_rrect(px, ui_inset(r, 1), 2, color);
    /* Light top edge gives the blocks a little depth */
    ui_fill(px, ui_rect(r.left + 3, r.top + 2, ui_w(r) - 6, 1),
            ui_alpha(UI_RGB(0xffffff), 0x60));
}

static void tetris_paint(game_t *g, twin_pixmap_t *px, ui_rect_t clip)
{
    const tetris_t *t = &g->u.tetris;
    ui_rect_t well = ui_rect(TT_X, TT_Y, TT_COLS * TT_CELL, TT_ROWS * TT_CELL);
    if (!ui_rect_contains(well, clip) &&
        ui_intersects(clip, ui_inset(well, -2)))
        ui_rrect_stroke(px, ui_inset(well, -2), 3, 1, C_DIM);

    int c0, c1, r0, r1;
    game_cell_span(g, clip, &c0, &c1, &r0, &r1);
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            ui_rect_t cr = game_cell_rect(g, c, r);
            int pr = r - t->y, pc = c - t->x;
            bool in_piece = pr >= 0 && pr < 4 && pc >= 0 && pc < 4 &&
                            tt_bit(t->piece, t->rot, pr, pc);
            if (in_piece)
                tt_cell(px, cr, tt_colors[t->piece]);
            else if (t->field[r][c])
                tt_cell(px, cr, tt_colors[t->field[r][c] - 1]);
            else
                ui_fill(px, ui_inset(cr, 1), C_CELL_B);
        }
    }

    /* Side column: next piece */
    ui_rect_t in = game_inner_rect();
    twin_coord_t cx = (TT_SIDE_X + in.right) / 2;
    ui_rect_t label = ui_rect(TT_SIDE_X, TT_Y, in.right - TT_SIDE_X, 18);
    if (ui_intersects(clip, label))
        ui_text_in(px, FONT_SM, label, UI_CENTER, C_LABEL, "Next");
    ui_rect_t pv = tt_preview_rect();
    if (ui_intersects(clip, pv)) {
        ui_rrect(px, pv, 6, C_CELL_A);
        int minr = 4, maxr = -1, minc = 4, maxc = -1;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (tt_bit(t->next, 0, r, c)) {
                    minr = r < minr ? r : minr;
                    maxr = r > maxr ? r : maxr;
                    minc = c < minc ? c : minc;
                    maxc = c > maxc ? c : maxc;
                }
        /* Center the piece's bounding box in the preview */
        int w = (maxc - minc + 1) * TT_PREV_CELL;
        int h = (maxr - minr + 1) * TT_PREV_CELL;
        int x0 = cx - w / 2, y0 = (pv.top + pv.bottom) / 2 - h / 2;
        for (int r = minr; r <= maxr; r++)
            for (int c = minc; c <= maxc; c++)
                if (tt_bit(t->next, 0, r, c))
                    tt_cell(px,
                            ui_rect(x0 + (c - minc) * TT_PREV_CELL,
                                    y0 + (r - minr) * TT_PREV_CELL,
                                    TT_PREV_CELL, TT_PREV_CELL),
                            tt_colors[t->next]);
    }
}

const game_ops_t game_tetris_ops = {
    "Tetris", 110,  TT_COLS,     TT_ROWS,     TT_CELL,
    TT_X,     TT_Y, tetris_init, tetris_tick, tetris_paint,
};
