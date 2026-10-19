/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Shared by the arcade's files: games.c (helpers and the view) and one file
 * per game (snake.c, pong.c, tetris.c).
 */

#ifndef _GAMES_INTERNAL_H_
#define _GAMES_INTERNAL_H_

#include "phone.h"

/* Layout, client coordinates */
#define HEADER_H PHONE_HEADER_H
#define PANEL_X 8
#define PANEL_Y 36
#define PANEL_W (PHONE_W - 2 * PANEL_X)
#define PANEL_H 200
#define PANEL_R 14
#define INNER_X (PANEL_X + 8)
#define INNER_Y (PANEL_Y + 8)

/* Board colors: a console look that stays dark under every theme */
#define C_PANEL UI_RGB(0x101614)
#define C_BORDER UI_RGB(0x5c746b)
#define C_CELL_A UI_RGB(0x141c19)
#define C_CELL_B UI_RGB(0x18221e)
#define C_DIM UI_RGB(0x34524a)
#define C_LABEL UI_RGB(0x8fa59c)

/* Snake: 16 x 14 cells of 13 px */
#define SN_COLS 16
#define SN_ROWS 14
#define SN_CELL 13
#define SN_N (SN_COLS * SN_ROWS)
#define SN_START 4

/* Pong: integer subpixels, 1/256 px */
#define PG_S 256
#define PG_BALL 8
#define PG_PW 6
#define PG_PH 34
#define PG_INSET 6
#define PG_V0 (4 * PG_S)     /* serve speed per tick */
#define PG_VMAX (9 * PG_S)   /* speed cap */
#define PG_PSPEED (3 * PG_S) /* paddle speed per tick */
#define PG_SERVE_TICKS 12

/* Tetris: 10 x 16 cells of 11 px */
#define TT_COLS 10
#define TT_ROWS 16
#define TT_CELL 11
#define TT_X (INNER_X + 10)
#define TT_Y (INNER_Y + 4)
#define TT_SIDE_X (TT_X + TT_COLS * TT_CELL)
#define TT_PREV_CELL 9

typedef enum {
    GAME_SNAKE,
    GAME_PONG,
    GAME_TETRIS,
    GAME_COUNT,
} game_kind_t;

typedef struct {
    uint8_t occ[SN_ROWS][SN_COLS]; /* 1 where the body is */
    uint8_t bx[SN_N], by[SN_N];    /* ring buffer, head at index head */
    int head, len;
    int dx, dy;
    int fx, fy; /* fruit */
} snake_t;

typedef struct {
    int32_t bx, by, vx, vy; /* ball top-left and velocity, subpixels */
    int32_t py[2];          /* paddle tops, subpixels: 0 left, 1 right */
    int aim[2];             /* per-approach aiming error, px */
    int serve;              /* ticks until the ball launches, 0 in play */
    bool serve_right;
} pong_t;

typedef struct {
    uint8_t field[TT_ROWS][TT_COLS]; /* 0 empty, else color index + 1 */
    int piece, rot, x, y;
    int next;
    int target_x, target_rot;
} tetris_t;

typedef struct {
    ui_view_t view;
    game_kind_t kind;
    uint32_t rng;
    bool paused;
    int score, best;
    int steps;      /* ticks played */
    int events;     /* meals, paddle hits, pieces placed */
    int full;       /* whole-board repaints (restarts, line clears) */
    ui_rect_t pend; /* damage batched by mark() */
    union {
        snake_t snake;
        pong_t pong;
        tetris_t tetris;
    } u;
} game_t;

typedef struct {
    const char *name;
    twin_time_t period; /* ms per step */
    /* Cell grid for mark_cell(); zero for games drawn in pixels */
    int cols, rows, cell;
    twin_coord_t ox, oy;
    void (*init)(game_t *g); /* new round; the caller repaints the board */
    void (*tick)(game_t *g); /* advance one step, mark() what changed */
    void (*paint)(game_t *g, twin_pixmap_t *px, ui_rect_t clip);
} game_ops_t;

/* Per-game tables, defined next to each game */
extern const game_ops_t game_snake_ops;
extern const game_ops_t game_pong_ops;
extern const game_ops_t game_tetris_ops;

/* Helpers (games.c) */
uint32_t game_rnd(game_t *g);
int game_rnd_below(game_t *g, int n);
ui_rect_t game_panel_rect(void);
ui_rect_t game_inner_rect(void);
/* Damage @r, batching it with nearby rectangles */
void game_mark(game_t *g, ui_rect_t r);
ui_rect_t game_cell_rect(const game_t *g, int c, int r);
/* Cells-changed hook shared by the grid games */
void game_mark_cell(game_t *g, int c, int r);
/* Range of cells of the current grid that intersect @clip */
void game_cell_span(const game_t *g,
                    ui_rect_t clip,
                    int *c0,
                    int *c1,
                    int *r0,
                    int *r1);
void game_set_score(game_t *g, int score);
/* Start a new round and repaint the board */
void game_restart(game_t *g);

#endif /* _GAMES_INTERNAL_H_ */
