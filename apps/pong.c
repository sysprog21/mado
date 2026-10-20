/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Arcade: Pong (see games.c) */

#include <string.h>

#include "games.h"

/*
 * Pong: the ball moves in subpixels and speeds up on every hit; paddles
 * chase the predicted crossing point with a capped speed and a random
 * aiming error, so fast rallies eventually end in a miss.
 */

static ui_rect_t pong_field(void)
{
    return ui_inset(game_panel_rect(), PG_INSET);
}

static twin_coord_t pong_paddle_x(int side)
{
    ui_rect_t f = pong_field();
    return side ? f.right - 4 - PG_PW : f.left + 4;
}

static ui_rect_t pong_paddle_rect(const pong_t *p, int side)
{
    return ui_rect(pong_paddle_x(side), p->py[side] / PG_S, PG_PW, PG_PH);
}

static ui_rect_t pong_ball_rect(const pong_t *p)
{
    return ui_rect(p->bx / PG_S, p->by / PG_S, PG_BALL, PG_BALL);
}

static void pong_center_ball(pong_t *p)
{
    ui_rect_t f = pong_field();
    p->bx = ((f.left + f.right) / 2 - PG_BALL / 2) * PG_S;
    p->by = ((f.top + f.bottom) / 2 - PG_BALL / 2) * PG_S;
    p->vx = p->vy = 0;
    p->serve = PG_SERVE_TICKS;
}

static void pong_init(game_t *g)
{
    pong_t *p = &g->u.pong;
    ui_rect_t f = pong_field();
    memset(p, 0, sizeof(*p));
    p->py[0] = p->py[1] = ((f.top + f.bottom) / 2 - PG_PH / 2) * PG_S;
    p->serve_right = game_rnd(g) & 1;
    pong_center_ball(p);
}

/* Ball center y when it reaches the face of paddle @side */
static int32_t pong_predict(const pong_t *p, int side)
{
    ui_rect_t f = pong_field();
    int32_t face = side ? (pong_paddle_x(1) - PG_BALL) * PG_S
                        : (pong_paddle_x(0) + PG_PW) * PG_S;
    int32_t vx = p->vx < 0 ? -p->vx : p->vx;
    int32_t dx = face - p->bx;
    if (dx < 0)
        dx = -dx;
    int32_t t = vx ? dx / vx : 0;
    int32_t range = (ui_h(f) - PG_BALL) * PG_S;
    int32_t y = p->by - f.top * PG_S + p->vy * t;
    /* Fold the straight-line path back into the field (wall bounces) */
    y %= 2 * range;
    if (y < 0)
        y += 2 * range;
    if (y > range)
        y = 2 * range - y;
    return y + f.top * PG_S + PG_BALL * PG_S / 2;
}

static void pong_paddle(game_t *g, int side)
{
    pong_t *p = &g->u.pong;
    ui_rect_t f = pong_field();
    bool coming = p->serve == 0 && (side ? p->vx > 0 : p->vx < 0);
    int32_t target = coming ? pong_predict(p, side) + p->aim[side] * PG_S
                            : (f.top + f.bottom) / 2 * PG_S;
    int32_t want = target - PG_PH * PG_S / 2;
    int32_t speed = coming ? PG_PSPEED : PG_PSPEED / 2;
    int32_t d = want - p->py[side];
    if (d > speed)
        d = speed;
    if (d < -speed)
        d = -speed;
    int32_t y = p->py[side] + d;
    if (y < f.top * PG_S)
        y = f.top * PG_S;
    if (y > (f.bottom - PG_PH) * PG_S)
        y = (f.bottom - PG_PH) * PG_S;
    if (y == p->py[side])
        return;
    ui_rect_t old = pong_paddle_rect(p, side);
    p->py[side] = y;
    game_mark(g, ui_rect_union(old, pong_paddle_rect(p, side)));
}

/* Bounce off paddle @side if the ball reached it this step */
static bool pong_hit(game_t *g, int side)
{
    pong_t *p = &g->u.pong;
    twin_coord_t px = pong_paddle_x(side);
    if (side ? p->vx <= 0 : p->vx >= 0)
        return false;
    int32_t face = side ? (px - PG_BALL) * PG_S : (px + PG_PW) * PG_S;
    if (side ? p->bx < face : p->bx > face)
        return false;
    /* Already behind the paddle: too late */
    if (side ? p->bx > (px + PG_PW) * PG_S : p->bx + PG_BALL * PG_S < px * PG_S)
        return false;
    int32_t ball_c = p->by + PG_BALL * PG_S / 2;
    int32_t pad_c = p->py[side] + PG_PH * PG_S / 2;
    int32_t off = (ball_c - pad_c) / PG_S; /* -21..21 when touching */
    if (off < -(PG_PH + PG_BALL) / 2 || off > (PG_PH + PG_BALL) / 2)
        return false;
    int32_t speed = (p->vx < 0 ? -p->vx : p->vx) + PG_S / 6;
    if (speed > PG_VMAX)
        speed = PG_VMAX;
    p->bx = face;
    p->vx = side ? -speed : speed;
    /* Steeper return off the paddle's ends */
    p->vy = off * PG_S / 4;
    /* The other paddle picks a fresh aiming error for this approach */
    p->aim[!side] = game_rnd_below(g, PG_PH - 6) - (PG_PH - 6) / 2;
    g->events++;
    game_set_score(g, g->score + 1);
    return true;
}

static void pong_tick(game_t *g)
{
    pong_t *p = &g->u.pong;
    ui_rect_t f = pong_field();
    ui_rect_t old = pong_ball_rect(p);

    if (p->serve > 0) {
        if (--p->serve == 0) {
            p->vx = p->serve_right ? PG_V0 : -PG_V0;
            p->vy = (game_rnd_below(g, 7) - 3) * PG_S / 2;
            p->aim[0] = p->aim[1] = 0;
            p->serve_right = !p->serve_right;
        }
    } else {
        p->bx += p->vx;
        p->by += p->vy;
        int32_t top = f.top * PG_S, bottom = (f.bottom - PG_BALL) * PG_S;
        if (p->by < top) {
            p->by = 2 * top - p->by;
            p->vy = -p->vy;
        } else if (p->by > bottom) {
            p->by = 2 * bottom - p->by;
            p->vy = -p->vy;
        }
        if (!pong_hit(g, 0))
            pong_hit(g, 1);
        if (p->bx < f.left * PG_S || p->bx + PG_BALL * PG_S > f.right * PG_S) {
            /* Missed: the rally is over, serve again from the center */
            game_set_score(g, 0);
            pong_center_ball(p);
        }
    }
    ui_rect_t now = pong_ball_rect(p);
    if (memcmp(&old, &now, sizeof(old)))
        game_mark(g, ui_rect_union(old, now));
    pong_paddle(g, 0);
    pong_paddle(g, 1);
}

static void pong_paint(game_t *g, twin_pixmap_t *px, ui_rect_t clip)
{
    const pong_t *p = &g->u.pong;
    ui_rect_t f = pong_field();
    twin_coord_t cx = (f.left + f.right) / 2;

    /* Dashed center line, only the dashes inside the clip */
    if (clip.left < cx + 1 && cx - 1 < clip.right) {
        int first = (clip.top - f.top) / 16;
        for (int i = first < 0 ? 0 : first;; i++) {
            ui_rect_t d = ui_rect(cx - 1, f.top + 4 + i * 16, 2, 8);
            if (d.top >= clip.bottom || d.bottom > f.bottom)
                break;
            ui_fill(px, d, C_DIM);
        }
    }
    for (int side = 0; side < 2; side++) {
        ui_rect_t r = pong_paddle_rect(p, side);
        if (ui_intersects(r, clip))
            ui_rrect(px, r, 3, UI_RGB(0xf0f4f2));
    }
    ui_rect_t b = pong_ball_rect(p);
    if (ui_intersects(b, clip))
        ui_circle(px, b.left + PG_BALL / 2, b.top + PG_BALL / 2, PG_BALL / 2,
                  UI_RGB(0x4dd0e1));
}

/* Pong steps faster: the ball moves continuously and a step damages only
 * two small strips and the ball */
static const game_ops_t game_pong_ops = {
    "Pong", 2, 50, 0, 0, 0, 0, 0, pong_init, pong_tick, pong_paint,
};

/* Launcher icon */
void app_pong_glyph(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg)
{
    int u = ui_w(p) / 8;
    ui_rrect(px, ui_rect(p.left + u + u / 2, p.top + 2 * u, u, 3 * u), u / 2,
             fg);
    ui_rrect(px, ui_rect(p.right - 2 * u - u / 2, p.top + 3 * u, u, 3 * u),
             u / 2, fg);
    ui_circle(px, (p.left + p.right) / 2, p.top + 3 * u + u / 2, u * 2 / 3 + 1,
              fg);
}

void app_pong_open(void)
{
    game_open(&game_pong_ops);
}

bool apps_pong_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    (void) w, (void) h; /* the board has a fixed size */
    return game_start(&game_pong_ops, screen, name, x, y);
}
