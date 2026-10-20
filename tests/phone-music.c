/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Music: track list, now playing, simulated playback, seeking, track
 * roll-over, shuffle/repeat, and the cost of a playback tick.
 */

#include <stdlib.h>

#include "data.h"
#include "phone-test.h"

#if defined(CONFIG_APP_PHONE_MUSIC)

int app_music_test_elapsed(void);
int app_music_test_track(void);
bool app_music_test_playing(void);
int app_music_test_scroll(void);

/* Now-playing layout (content coordinates) */
#define CTL_Y 230
#define X_SHUFFLE 28
#define X_PREV 74
#define X_PLAY 120
#define X_NEXT 166
#define X_REPEAT 212
#define SLIDER_Y 187
#define SLIDER_X0 52  /* value 0 */
#define SLIDER_X1 188 /* value 1000 */

/* Track list layout */
#define LIST_TOP 30
#define ROW_H 40
#define MINI_Y 210

static int duration(int track)
{
    return (int) phone_tracks()[track].duration_sec;
}

static void test_now_playing(void)
{
    /* Tap the third song */
    ph_tap(120, LIST_TOP + 2 * ROW_H + ROW_H / 2);
    CHECK_EQ(phone_depth(), 2, "now playing pushed");
    CHECK_EQ(app_music_test_track(), 2, "tapped track selected");
    CHECK(app_music_test_playing(), "tapping a song plays it");
    ph_shot("music-now-playing");

    /* Simulated playback advances with (virtual) time */
    int e0 = app_music_test_elapsed();
    ph_wait(3000);
    int e1 = app_music_test_elapsed();
    CHECK(e1 - e0 >= 2 && e1 - e0 <= 4, "3 s of playback advanced %d s",
          e1 - e0);

    /* A playback tick repaints only the knob and the time labels */
    ph_wait(500);
    uint64_t p0 = phone_th->screen->stats.pixels;
    ph_wait(1000);
    uint64_t ticked = phone_th->screen->stats.pixels - p0;
    CHECK(ticked > 0 && ticked < PHONE_W * PHONE_CONTENT_H / 10,
          "one tick composited %llu pixels", (unsigned long long) ticked);

    /* Pause freezes the position */
    ph_tap(X_PLAY, CTL_Y);
    CHECK(!app_music_test_playing(), "pause");
    int paused = app_music_test_elapsed();
    ph_wait(3000);
    CHECK_EQ(app_music_test_elapsed(), paused, "paused position holds");
    ph_shot("music-paused");

    /* Next and previous switch tracks and resume playback */
    ph_tap(X_NEXT, CTL_Y);
    CHECK_EQ(app_music_test_track(), 3, "next track");
    CHECK(app_music_test_playing(), "skipping resumes playback");
    CHECK(app_music_test_elapsed() <= 3, "next starts at the top");
    ph_tap(X_PREV, CTL_Y);
    CHECK_EQ(app_music_test_track(), 2, "previous track");
    ph_tap(X_PREV, CTL_Y);
    ph_tap(X_PREV, CTL_Y);
    ph_tap(X_PREV, CTL_Y);
    CHECK_EQ(app_music_test_track(), PHONE_TRACK_COUNT - 1,
             "previous wraps to the last track");
    ph_tap(X_NEXT, CTL_Y);
    CHECK_EQ(app_music_test_track(), 0, "next wraps to the first track");
    ph_tap(X_NEXT, CTL_Y);
    ph_tap(X_NEXT, CTL_Y);
    CHECK_EQ(app_music_test_track(), 2, "back on the third track");

    /* Dragging the slider to 3/4 seeks there */
    int x = SLIDER_X0 + (SLIDER_X1 - SLIDER_X0) * 3 / 4;
    ph_drag(60, SLIDER_Y, x, SLIDER_Y);
    int want = duration(2) * 3 / 4;
    int got = app_music_test_elapsed();
    CHECK(got >= want && got <= want + 3, "seeked to %d s, want ~%d", got,
          want);
    CHECK_EQ(app_music_test_track(), 2, "seeking keeps the track");
    ph_shot("music-seeked");

    /* Reaching the end rolls over to the next track */
    ph_drag(x, SLIDER_Y, SLIDER_X1 + 4, SLIDER_Y);
    CHECK_EQ(app_music_test_track(), 3, "end of track advances");
    CHECK(app_music_test_elapsed() <= 3, "next track starts from the top");

    /* Repeat keeps looping the same track */
    ph_tap(X_REPEAT, CTL_Y);
    ph_drag(60, SLIDER_Y, SLIDER_X1 + 4, SLIDER_Y);
    CHECK_EQ(app_music_test_track(), 3, "repeat loops the track");
    CHECK(app_music_test_elapsed() <= 3, "repeat restarts the track");
    ph_shot("music-repeat");
    ph_tap(X_REPEAT, CTL_Y);

    /* Shuffle picks some other track */
    ph_tap(X_SHUFFLE, CTL_Y);
    ph_tap(X_NEXT, CTL_Y);
    int shuffled = app_music_test_track();
    CHECK(shuffled != 3 && shuffled >= 0 && shuffled < PHONE_TRACK_COUNT,
          "shuffle chose track %d", shuffled);
    ph_shot("music-shuffle");
    ph_tap(X_SHUFFLE, CTL_Y);

    /* Theme switch repaints from state */
    int theme = phone_theme_index();
    ph_tap(X_PLAY, CTL_Y);
    phone_theme_set(1);
    ph_shot("music-now-playing-dark");
    phone_theme_set(theme);
    ph_tap(X_PLAY, CTL_Y);
    CHECK(app_music_test_playing(), "resumed");
}

void test_music(void)
{
    ph_open("Music");
    CHECK_EQ(phone_depth(), 1, "music list opened");
    CHECK_EQ(app_music_test_scroll(), 0, "list starts at the top");
    ph_shot("music-list");

    test_now_playing();

    /* Back on the list: the playing song is highlighted */
    phone_back();
    ph_wait(2000);
    CHECK_EQ(phone_depth(), 1, "back to the list");
    ph_shot("music-list-playing");

    /* Mini player toggles playback without leaving the list */
    ph_tap(PHONE_W - 24, MINI_Y + 24);
    CHECK(!app_music_test_playing(), "mini player pauses");
    ph_tap(PHONE_W - 24, MINI_Y + 24);
    CHECK(app_music_test_playing(), "mini player resumes");

    /* Scrolling is not a tap, and rows still hit-test after it */
    ph_drag(120, 180, 120, 60);
    CHECK_EQ(phone_depth(), 1, "scrolling opened nothing");
    int off = app_music_test_scroll();
    CHECK(off > 0, "list scrolled to %d", off);
    ph_shot("music-list-scrolled");
    int last = PHONE_TRACK_COUNT - 1;
    int y = LIST_TOP + last * ROW_H - off + ROW_H / 2;
    CHECK(y > LIST_TOP && y < MINI_Y, "last row visible at %d", y);
    ph_tap(120, y);
    CHECK_EQ(phone_depth(), 2, "row tap after scrolling opens now playing");
    CHECK_EQ(app_music_test_track(), last, "tapped the last song");
    CHECK(app_music_test_playing(), "tapped song is playing");

    /* The player outlives the app: playback continues while closed */
    ph_home();
    int e0 = app_music_test_elapsed();
    ph_wait(4000);
    int e1 = app_music_test_elapsed();
    CHECK(e1 - e0 >= 3 && e1 - e0 <= 5, "background playback advanced %d s",
          e1 - e0);
    ph_open("Music");
    CHECK_EQ(app_music_test_track(), last, "reopened on the same song");
    CHECK(app_music_test_scroll() > 0, "list opens scrolled to the song");
    ph_shot("music-list-reopened");

    /* Mini player opens now playing for the current song */
    ph_tap(100, MINI_Y + 24);
    CHECK_EQ(phone_depth(), 2, "mini player opens now playing");
    ph_tap(X_PLAY, CTL_Y);
    CHECK(!app_music_test_playing(), "paused before leaving");
    ph_home();
}
#else
void test_music(void) {}
#endif
