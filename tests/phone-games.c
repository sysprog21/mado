/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Games: Snake, Pong and Tetris play themselves.  Checks that they advance,
 * pause and resume on tap, replay identically on every open, score, and
 * that a tick composites only the cells it changed.
 */

#include <string.h>

#include "phone-test.h"

int app_games_test_steps(void);
int app_games_test_score(void);
int app_games_test_best(void);
int app_games_test_events(void);
int app_games_test_full(void);
bool app_games_test_paused(void);

/* A full content-area frame; a tick must stay well below it */
#define FULL_FRAME (PHONE_W * PHONE_CONTENT_H)

typedef struct {
    int steps, score, best, events;
} snapshot_t;

static snapshot_t snap(void)
{
    return (snapshot_t) {app_games_test_steps(), app_games_test_score(),
                         app_games_test_best(), app_games_test_events()};
}

/* Pixels scanned out per tick over a second without full-board repaints */
static void check_pixels_per_tick(const char *name)
{
    for (int attempt = 0; attempt < 8; attempt++) {
        int full = app_games_test_full();
        int steps = app_games_test_steps();
        uint64_t pixels = phone_th->screen->stats.pixels;
        ph_wait(1000);
        int ticks = app_games_test_steps() - steps;
        if (app_games_test_full() != full)
            continue; /* a restart or line clear repaints the board */
        CHECK(ticks > 0, "%s ticks during the window", name);
        if (ticks <= 0)
            return;
        uint64_t per = (phone_th->screen->stats.pixels - pixels) / ticks;
        printf("  %s: %llu px/tick over %d ticks (%llu.%llu%% of a frame)\n",
               name, (unsigned long long) per, ticks,
               (unsigned long long) (per * 100 / FULL_FRAME),
               (unsigned long long) (per * 1000 / FULL_FRAME % 10));
        CHECK(per < FULL_FRAME / 4, "%s: %llu px per tick", name,
              (unsigned long long) per);
        return;
    }
    CHECK(0, "%s: no window without a full repaint", name);
}

static void check_pause(const char *name)
{
    ph_tap(PHONE_W / 2, 130);
    CHECK(app_games_test_paused(), "%s paused by tap", name);
    int steps = app_games_test_steps();
    ph_wait(1500);
    CHECK_EQ(app_games_test_steps(), steps, "pause freezes the game");
    if (!strcmp(name, "Snake"))
        ph_shot("games-paused");

    ph_tap(PHONE_W / 2, 130);
    CHECK(!app_games_test_paused(), "%s resumed by tap", name);
    CHECK(app_games_test_steps() > steps, "%s continues after resume", name);
}

static void play(const char *name, const char *shot)
{
    ph_open(name);
    CHECK_EQ(phone_depth(), 1, "game opened");
    int steps = app_games_test_steps();
    CHECK(steps > 0, "%s ticked while opening", name);
    ph_wait(2000);
    CHECK(app_games_test_steps() > steps, "%s advances", name);
    snapshot_t a = snap();

    check_pixels_per_tick(name);
    check_pause(name);
    ph_shot(shot);

    /* Reopen: the per-open seed replays the same game */
    ph_home();
    ph_open(name);
    ph_wait(2000);
    snapshot_t b = snap();
    CHECK(!memcmp(&a, &b, sizeof(a)),
          "%s replays: steps %d/%d score %d/%d events %d/%d", name, a.steps,
          b.steps, a.score, b.score, a.events, b.events);
}

void test_games(void)
{
    play("Snake", "games-snake");
    ph_wait(15000);
    CHECK(app_games_test_events() > 0, "snake ate");
    CHECK(app_games_test_best() > 0, "snake scored");
    ph_home();

    play("Pong", "games-pong");
    ph_wait(5000);
    CHECK(app_games_test_events() > 0, "pong paddles hit the ball");
    ph_home();

    play("Tetris", "games-tetris");
    ph_wait(30000);
    CHECK(app_games_test_events() > 10, "tetris placed pieces");
    CHECK(app_games_test_best() > 0, "tetris cleared lines");
    ph_shot("games-tetris-late");

    /* A covering view freezes the game; revealing it repaints from state */
    int steps = app_games_test_steps();
    ph_open("Snake");
    CHECK_EQ(phone_depth(), 2, "snake over tetris");
    phone_back();
    ph_wait(500);
    CHECK_EQ(phone_depth(), 1, "back to tetris");
    CHECK(app_games_test_steps() >= steps, "tetris state kept");
    ph_shot("games-tetris-revealed");

    ph_home();
}
