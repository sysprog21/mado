/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Music: a track list and a now-playing screen over a simulated player.
 *
 * The player is a single static struct, the one allowed global, standing in
 * for a media service that outlives the app.  It holds no timer: while
 * playing it records the clock time at which the current track's position
 * was zero, so the position is derived from twin_now() on demand and
 * playback keeps "running" while the app is closed at zero cost.
 * player_sync() rolls finished tracks over whenever someone looks.
 *
 * Views only own a once-per-second timer that samples the player into a
 * snapshot and invalidates what moved (knob, time labels, a progress line,
 * a row on track change).  Paint draws from the snapshot only, so every
 * pixel change is paired with an invalidation; a full repaint (reveal,
 * theme switch) resamples first.  Album art is procedural: a diagonal
 * gradient keyed off album and track, vinyl rings and a note glyph.
 */

#include <stdio.h>

#include "data.h"
#include "phone.h"


/*
 * Player service
 */
static struct {
    int track;
    bool playing, shuffle, repeat;
    twin_time_t start; /* playing: clock time at position 0 of the track */
    int32_t pos;       /* paused: position in ms */
    uint32_t seed;     /* shuffle generator */
} player = {.seed = 0x9e3779b9u};

static int32_t track_ms(int i)
{
    return (int32_t) phone_tracks()[i].duration_sec * 1000;
}

/* twin_now() wraps: do clock arithmetic in unsigned and convert back */
static int32_t player_elapsed(void)
{
    return (int32_t) ((uint32_t) twin_now() - (uint32_t) player.start);
}

static twin_time_t time_add(twin_time_t t, int32_t ms)
{
    return (twin_time_t) ((uint32_t) t + (uint32_t) ms);
}

static int32_t player_pos(void)
{
    return player.playing ? player_elapsed() : player.pos;
}

static int shuffle_pick(void)
{
    /* xorshift32; any track but the current one */
    uint32_t x = player.seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    player.seed = x;
    int i = (int) (x % (PHONE_TRACK_COUNT - 1));
    return i >= player.track ? i + 1 : i;
}

static int next_track(void)
{
    return player.shuffle ? shuffle_pick()
                          : (player.track + 1) % PHONE_TRACK_COUNT;
}

/* Catch up with the clock: roll over every track that finished */
static void player_sync(void)
{
    if (!player.playing)
        return;
    while (player_elapsed() >= track_ms(player.track)) {
        player.start = time_add(player.start, track_ms(player.track));
        if (!player.repeat)
            player.track = next_track();
    }
}

static void player_seek(int32_t ms)
{
    int32_t dur = track_ms(player.track);
    if (ms < 0)
        ms = 0;
    if (ms > dur - 1)
        ms = dur - 1;
    if (player.playing)
        player.start = time_add(twin_now(), -ms);
    else
        player.pos = ms;
}

static void player_play(bool on)
{
    player_sync();
    if (on == player.playing)
        return;
    int32_t pos = player_pos();
    player.playing = on;
    player_seek(pos);
}

static void player_select(int track)
{
    player_sync();
    player.track = track;
    player_seek(0);
    player_play(true);
}

static void player_skip(int dir)
{
    player_sync();
    player_select(dir > 0 ? next_track()
                          : (player.track + PHONE_TRACK_COUNT - 1) %
                                PHONE_TRACK_COUNT);
}

/* Delay until the displayed second changes */
static twin_time_t player_next_second(void)
{
    return 1000 - player_pos() % 1000;
}

static void format_time(char *buf, size_t len, const char *sign, int sec)
{
    snprintf(buf, len, "%s%d:%02d", sign, sec / 60, sec % 60);
}

/*
 * Procedural album art
 */
static twin_argb32_t hsv(int h, int s, int v)
{
    /* h 0..359, s and v 0..255 */
    int f = (h % 60) * 255 / 60;
    int p = v * (255 - s) / 255;
    int q = v * (255 - s * f / 255) / 255;
    int t = v * (255 - s * (255 - f) / 255) / 255;
    int r, g, b;
    switch (h / 60) {
    case 0:
        r = v, g = t, b = p;
        break;
    case 1:
        r = q, g = v, b = p;
        break;
    case 2:
        r = p, g = v, b = t;
        break;
    case 3:
        r = p, g = q, b = v;
        break;
    case 4:
        r = t, g = p, b = v;
        break;
    default:
        r = v, g = p, b = q;
        break;
    }
    return UI_RGB((uint32_t) r << 16 | (uint32_t) g << 8 | (uint32_t) b);
}

/* Albums get distinct base hues; tracks shift within their album's family */
static void art_colors(int track, twin_argb32_t *c0, twin_argb32_t *c1)
{
    unsigned h = 0;
    for (const char *s = phone_tracks()[track].album; *s; s++)
        h = h * 31 + (unsigned char) *s;
    int hue = (int) ((h % 6) * 60 + (unsigned) track * 17) % 360;
    *c0 = hsv(hue, 140, 235);
    *c1 = hsv((hue + 45) % 360, 220, 130);
}

static void paint_art(twin_pixmap_t *px, ui_rect_t r, int track, int radius)
{
    twin_argb32_t c0, c1;
    art_colors(track, &c0, &c1);
    twin_path_t *p = ui_path();
    twin_path_rounded_rectangle(p, ui_fx(r.left), ui_fx(r.top), ui_fx(ui_w(r)),
                                ui_fx(ui_h(r)), ui_fx(radius), ui_fx(radius));
    twin_paint_path_linear(px, p, ui_fx(r.left), ui_fx(r.top), c0,
                           ui_fx(r.right), ui_fx(r.bottom), c1);
    ui_path_done(p);

    int s = ui_w(r);
    twin_coord_t cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    if (s >= 64) {
        /* Vinyl grooves behind the note */
        for (int k = 0; k < 4; k++)
            ui_ring(px, cx, cy, s * (44 - k * 7) / 100, 1,
                    ui_alpha(UI_WHITE, (twin_a8_t) (0x30 - k * 6)));
        ui_circle(px, cx, cy, s * 16 / 100, ui_alpha(UI_BLACK, 0x18));
    }
    ui_icon(px, TWIN_SYM_MUSIC, s * 2 / 5, cx, cy, ui_alpha(UI_WHITE, 0xe8));
}

/*
 * Now playing screen
 */
#define ART 116
#define ART_TOP 8
#define TINT_H 140
#define TITLE_BASE 146
#define SUB_BASE 164
#define SLIDER_Y 187 /* center of the progress row */
#define CTL_Y 230

enum { C_SHUFFLE, C_PREV, C_PLAY, C_NEXT, C_REPEAT, C_COUNT };

typedef struct {
    ui_view_t view;
    /* Snapshot of the player as last painted */
    int track;
    bool playing;
    int value;    /* slider 0..1000 */
    int sec;      /* elapsed seconds */
    bool seeking; /* finger on the slider: value follows it */
    int pressed;  /* control under the finger, -1 when none */
} np_t;

static ui_rect_t art_rect(void)
{
    return ui_rect((PHONE_W - ART) / 2, ART_TOP, ART, ART);
}

static ui_rect_t info_rect(void)
{
    return ui_rect(0, TINT_H - 12, PHONE_W, SUB_BASE + 6 - (TINT_H - 12));
}

static ui_rect_t slider_rect(void)
{
    /* Times flank the bar on one row so a tick damages one thin band */
    return ui_rect(44, SLIDER_Y - 11, PHONE_W - 88, 22);
}

/* Generous touch target around the thin slider track */
static ui_rect_t slider_hit(void)
{
    ui_rect_t r = slider_rect();
    r.top -= 6;
    r.bottom += 6;
    return r;
}

static ui_rect_t elapsed_rect(void)
{
    return ui_rect(8, SLIDER_Y - 7, 36, 14);
}

static ui_rect_t remain_rect(void)
{
    return ui_rect(PHONE_W - 44, SLIDER_Y - 7, 36, 14);
}

static const twin_coord_t ctl_x[C_COUNT] = {28, 74, 120, 166, 212};

static ui_rect_t ctl_rect(int c)
{
    int s = c == C_PLAY ? 52 : 40;
    return ui_rect(ctl_x[c] - s / 2, CTL_Y - s / 2, s, s);
}

static int ctl_at(twin_coord_t x, twin_coord_t y)
{
    for (int c = 0; c < C_COUNT; c++)
        if (ui_hit(ctl_rect(c), x, y))
            return c;
    return -1;
}

static twin_coord_t knob_x(int value)
{
    return ui_slider_knob_x(slider_rect(), value);
}

static void np_sample(np_t *n, int *value, int *sec)
{
    int32_t dur = track_ms(n->track);
    int32_t pos = player_pos();
    if (n->seeking) {
        *value = n->value;
        pos = (int32_t) ((int64_t) n->value * dur / 1000);
    } else {
        if (pos > dur)
            pos = dur;
        *value = (int) ((int64_t) pos * 1000 / dur);
    }
    *sec = pos / 1000;
}

/* Slider band swept by the knob (with its shadow) moving from a to b */
static void np_invalidate_knob(np_t *n, int from, int to)
{
    twin_coord_t a = knob_x(from), b = knob_x(to);
    ui_rect_t s = slider_rect();
    twin_coord_t l = (a < b ? a : b) - 12, r = (a < b ? b : a) + 12;
    ui_view_invalidate(&n->view, ui_rect(l, s.top, r - l, ui_h(s)));
}

static void np_snapshot(np_t *n)
{
    player_sync();
    n->track = player.track;
    n->playing = player.playing;
    np_sample(n, &n->value, &n->sec);
}

/* Tick only while playing; a paused player has nothing to animate */
static void ensure_tick(ui_view_t *v, ui_timer_t tick)
{
    if (player.playing && !ui_view_has_timer(v, tick))
        ui_view_every(v, player_next_second(), tick);
}

static twin_time_t np_tick(ui_view_t *v);

/* Re-sample the player, invalidate only what changed, and keep the tick
 * running while it plays */
static void np_refresh(np_t *n)
{
    ui_view_t *v = &n->view;
    player_sync();
    ensure_tick(v, np_tick);
    if (player.track != n->track) {
        np_snapshot(n);
        ui_view_invalidate_all(v);
        return;
    }
    if (player.playing != n->playing) {
        n->playing = player.playing;
        ui_view_invalidate(v, ctl_rect(C_PLAY));
    }
    int value, sec;
    np_sample(n, &value, &sec);
    if (value != n->value) {
        np_invalidate_knob(n, n->value, value);
        n->value = value;
    }
    if (sec != n->sec) {
        n->sec = sec;
        ui_view_invalidate(v, elapsed_rect());
        ui_view_invalidate(v, remain_rect());
    }
}

static twin_time_t np_tick(ui_view_t *v)
{
    np_refresh((np_t *) v);
    return player.playing ? player_next_second() : -1;
}

static void paint_control(twin_pixmap_t *px, np_t *n, int c)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = ctl_rect(c);
    twin_coord_t cx = ctl_x[c];
    bool down = c == n->pressed;
    if (c == C_PLAY) {
        ui_circle(px, cx, CTL_Y + 2, 25, ui_alpha(UI_BLACK, 0x28));
        ui_circle(px, cx, CTL_Y, 24, down ? th->primary_dk : th->primary);
        /* The play triangle's mass sits left of its box: nudge it right */
        ui_icon(px, n->playing ? TWIN_SYM_PAUSE : TWIN_SYM_PLAY, 20,
                cx + (n->playing ? 0 : 2), CTL_Y, th->on_primary);
        return;
    }
    if (down)
        ui_circle(px, cx, CTL_Y, ui_w(r) / 2 - 2,
                  ui_mix(th->bg, th->primary, 70));
    if (c == C_PREV || c == C_NEXT) {
        ui_icon(px, c == C_PREV ? TWIN_SYM_PREV : TWIN_SYM_NEXT, 18, cx, CTL_Y,
                th->text);
        return;
    }
    bool on = c == C_SHUFFLE ? player.shuffle : player.repeat;
    ui_icon(px, c == C_SHUFFLE ? TWIN_SYM_SHUFFLE : TWIN_SYM_REPEAT, 15, cx,
            CTL_Y - 1, on ? th->primary : th->text_sec);
    if (on)
        ui_circle(px, cx, CTL_Y + 13, 2, th->primary);
}

static void np_paint(ui_view_t *v, twin_pixmap_t *px)
{
    np_t *n = (np_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v), bounds = ui_view_bounds(v);
    if (clip.left <= bounds.left && clip.top <= bounds.top &&
        clip.right >= bounds.right && clip.bottom >= bounds.bottom)
        np_snapshot(n);

    const phone_track_t *t = &phone_tracks()[n->track];
    twin_argb32_t c0, c1;
    art_colors(n->track, &c0, &c1);

    /* Background washed with the cover's color, fading into the theme */
    ui_rect_t tint = ui_rect(0, 0, PHONE_W, TINT_H);
    if (ui_intersects(clip, tint))
        ui_vgrad(px, tint, ui_mix(th->bg, c0, 110), th->bg);
    ui_fill(px, ui_rect(0, TINT_H, PHONE_W, ui_h(bounds) - TINT_H), th->bg);

    ui_rect_t art = art_rect();
    if (ui_intersects(clip, ui_inset(art, -6))) {
        ui_rrect(px, ui_rect(art.left + 2, art.top + 4, ART - 4, ART), 14,
                 ui_alpha(UI_BLACK, 0x30));
        paint_art(px, art, n->track, 14);
    }

    if (ui_intersects(clip, info_rect())) {
        char sub[96];
        snprintf(sub, sizeof(sub), "%s \xc2\xb7 %s", t->artist, t->album);
        ui_text_fit(px, FONT_HEADING,
                    ui_rect(8, TITLE_BASE - 15, PHONE_W - 16, 20), UI_CENTER,
                    th->text, t->title);
        ui_text_fit(px, FONT_SM, ui_rect(8, SUB_BASE - 12, PHONE_W - 16, 16),
                    UI_CENTER, th->text_sec, sub);
    }

    if (ui_intersects(clip, slider_rect()))
        ui_slider(px, slider_rect(), n->value, n->seeking);

    char buf[16];
    if (ui_intersects(clip, elapsed_rect())) {
        format_time(buf, sizeof(buf), "", n->sec);
        ui_text(px, FONT_SM, elapsed_rect().right - 2, SLIDER_Y + 4, UI_RIGHT,
                th->text_sec, buf);
    }
    if (ui_intersects(clip, remain_rect())) {
        int left = (int) t->duration_sec - n->sec;
        format_time(buf, sizeof(buf), "-", left > 0 ? left : 0);
        ui_text(px, FONT_SM, remain_rect().left + 2, SLIDER_Y + 4, UI_LEFT,
                th->text_sec, buf);
    }

    for (int c = 0; c < C_COUNT; c++)
        if (ui_intersects(clip, ctl_rect(c)))
            paint_control(px, n, c);
}

static ui_rect_t np_ctl(ui_view_t *v, int c)
{
    (void) v;
    return ctl_rect(c);
}

static void np_set_pressed(np_t *n, int c)
{
    ui_view_set_pressed(&n->view, &n->pressed, c, np_ctl);
}

static void np_seek_to(np_t *n, twin_coord_t x)
{
    int value = ui_slider_value_at(slider_rect(), x);
    if (value == n->value)
        return;
    np_invalidate_knob(n, n->value, value);
    n->value = value;
    np_refresh(n); /* time labels follow the finger */
}

static void np_action(np_t *n, int c)
{
    switch (c) {
    case C_PLAY:
        player_play(!player.playing);
        break;
    case C_PREV:
    case C_NEXT:
        player_skip(c == C_NEXT ? 1 : -1);
        break;
    case C_SHUFFLE:
        player.shuffle = !player.shuffle;
        ui_view_invalidate(&n->view, ctl_rect(c));
        break;
    case C_REPEAT:
        player.repeat = !player.repeat;
        ui_view_invalidate(&n->view, ctl_rect(c));
        break;
    }
    np_refresh(n);
}

static bool np_event(ui_view_t *v, const ui_event_t *e)
{
    np_t *n = (np_t *) v;
    switch (e->kind) {
    case UI_SHOW:
        /* The player may have changed while another view covered this one,
         * and a covered view keeps its old pixels unless it was evicted */
        np_refresh(n);
        return true;
    case UI_PRESS:
        if (ui_hit(slider_hit(), e->x, e->y)) {
            n->seeking = true;
            ui_view_invalidate(v, slider_rect()); /* knob grows */
            np_seek_to(n, e->x);
            return true;
        }
        np_set_pressed(n, ctl_at(e->x, e->y));
        return true;
    case UI_MOVE:
    case UI_DRAG:
        if (n->seeking)
            np_seek_to(n, e->x);
        else
            np_set_pressed(n, e->kind == UI_MOVE ? ctl_at(e->x, e->y) : -1);
        return true;
    case UI_TAP:
    case UI_RELEASE:
        if (n->seeking) {
            np_seek_to(n, e->x);
            player_seek(
                (int32_t) ((int64_t) n->value * track_ms(n->track) / 1000));
            n->seeking = false;
            ui_view_invalidate(v, slider_rect()); /* knob shrinks */
            np_refresh(n);
            return true;
        }
        np_set_pressed(n, -1);
        if (e->kind == UI_TAP) {
            int c = ctl_at(e->x, e->y);
            if (c >= 0)
                np_action(n, c);
        }
        return true;
    case UI_CANCEL:
        if (n->seeking) {
            n->seeking = false;
            ui_view_invalidate(v, slider_rect());
            np_refresh(n);
        }
        np_set_pressed(n, -1);
        return false;
    default:
        return false;
    }
}

static void now_playing_open(void)
{
    np_t *n = ui_view_new(sizeof(*n), "Now Playing", np_paint, np_event, NULL);
    if (!n)
        return;
    n->pressed = -1;
    np_snapshot(n);
    ensure_tick(&n->view, np_tick);
    ui_view_show(&n->view);
}

/*
 * Track list
 */
#define ROW_H PHONE_LIST_ROW_H
#define MINI_H 48
#define LIST_TOP PHONE_HEADER_H
#define NUM_W 36

enum { HIT_MINI = -2, HIT_PLAY = -3, HIT_NONE = -1 };

typedef struct {
    ui_view_t view;
    ui_scroll_t scroll;
    int pressed; /* row, HIT_MINI, HIT_PLAY or HIT_NONE */
    /* Snapshot of the player as last painted */
    int track;
    bool playing;
    twin_coord_t fill; /* mini bar progress width */
} list_t;

/* Top of the mini player, which sits at the bottom of the view */
static twin_coord_t mini_y(const list_t *l)
{
    return ui_view_height(&l->view) - MINI_H;
}

static ui_rect_t list_area(const list_t *l)
{
    return ui_rect(0, LIST_TOP, PHONE_W, mini_y(l) - LIST_TOP);
}

static void list_layout(ui_view_t *v)
{
    list_t *l = (list_t *) v;
    ui_scroll_setup(&l->scroll, list_area(l), PHONE_TRACK_COUNT * ROW_H);
}

static ui_rect_t mini_rect(const list_t *l)
{
    return ui_rect(0, mini_y(l), PHONE_W, MINI_H);
}

static ui_rect_t mini_play_rect(const list_t *l)
{
    return ui_rect(PHONE_W - 48, mini_y(l) + 2, 48, MINI_H - 2);
}

static ui_rect_t row_rect(const list_t *l, int i)
{
    return ui_rect(0, LIST_TOP + i * ROW_H - l->scroll.offset, PHONE_W, ROW_H);
}

/* Part of row @i inside the list viewport */
static ui_rect_t row_visible(const list_t *l, int i)
{
    ui_rect_t r = row_rect(l, i), a = list_area(l);
    if (r.top < a.top)
        r.top = a.top;
    if (r.bottom > a.bottom)
        r.bottom = a.bottom;
    return r;
}

static twin_coord_t mini_fill(void)
{
    int32_t dur = track_ms(player.track), pos = player_pos();
    if (pos > dur)
        pos = dur;
    return (twin_coord_t) ((int64_t) pos * PHONE_W / dur);
}

static void list_snapshot(list_t *l)
{
    player_sync();
    l->track = player.track;
    l->playing = player.playing;
    l->fill = mini_fill();
}

static twin_time_t list_tick(ui_view_t *v);

/* As np_refresh(), for the list's rows and mini player */
static void list_refresh(list_t *l)
{
    ui_view_t *v = &l->view;
    player_sync();
    ensure_tick(v, list_tick);
    if (player.track != l->track) {
        ui_view_invalidate(v, row_visible(l, l->track));
        ui_view_invalidate(v, row_visible(l, player.track));
        ui_view_invalidate(v, mini_rect(l));
        list_snapshot(l);
        return;
    }
    if (player.playing != l->playing) {
        l->playing = player.playing;
        ui_view_invalidate(v, row_visible(l, l->track)); /* note vs number */
        ui_view_invalidate(v, mini_play_rect(l));
    }
    twin_coord_t fill = mini_fill();
    if (fill != l->fill) {
        twin_coord_t a = fill < l->fill ? fill : l->fill;
        twin_coord_t b = fill < l->fill ? l->fill : fill;
        ui_view_invalidate(v, ui_rect(a, mini_y(l), b - a, 2));
        l->fill = fill;
    }
}

static twin_time_t list_tick(ui_view_t *v)
{
    list_refresh((list_t *) v);
    return player.playing ? player_next_second() : -1;
}

static void paint_row(twin_pixmap_t *px, list_t *l, int i)
{
    const phone_theme_t *th = phone_theme();
    const phone_track_t *t = &phone_tracks()[i];
    ui_rect_t r = row_rect(l, i);
    bool current = i == l->track;
    twin_argb32_t bg = th->card;
    if (i == l->pressed)
        bg = ui_mix(th->card, th->primary, 70);
    else if (current)
        bg = ui_mix(th->card, th->primary, 34);
    ui_fill(px, r, bg);

    twin_coord_t cy = (r.top + r.bottom) / 2;
    if (current && l->playing) {
        ui_icon(px, TWIN_SYM_MUSIC, 14, NUM_W / 2 + 2, cy, th->primary);
    } else {
        char num[12];
        snprintf(num, sizeof(num), "%d", i + 1);
        ui_text(px, FONT_SM, NUM_W / 2 + 2, ui_baseline(FONT_SM, r.top, ROW_H),
                UI_CENTER, current ? th->primary : th->text_sec, num);
    }

    char dur[16];
    format_time(dur, sizeof(dur), "", (int) t->duration_sec);
    twin_coord_t dw = ui_text_width(FONT_SM, dur);
    ui_text(px, FONT_SM, r.right - 10, ui_baseline(FONT_SM, r.top, ROW_H),
            UI_RIGHT, th->text_sec, dur);

    ui_rect_t text = {NUM_W - 4, (twin_coord_t) (r.right - dw - 14), r.top,
                      r.bottom};
    ui_text_fit(px, FONT_NORMAL,
                ui_rect(text.left, r.top + 3, ui_w(text), ROW_H / 2 - 2),
                UI_LEFT, current ? th->primary : th->text, t->title);
    ui_text_fit(
        px, FONT_SM,
        ui_rect(text.left, r.top + ROW_H / 2 - 1, ui_w(text), ROW_H / 2 - 2),
        UI_LEFT, th->text_sec, t->artist);
    ui_separator(px, ui_rect(NUM_W, r.top, PHONE_W - NUM_W, ROW_H));
}

static void paint_mini(twin_pixmap_t *px, list_t *l, ui_rect_t clip)
{
    const phone_theme_t *th = phone_theme();
    const phone_track_t *t = &phone_tracks()[l->track];
    ui_rect_t m = mini_rect(l);
    twin_coord_t my = m.top;

    bool body_down = l->pressed == HIT_MINI;
    twin_argb32_t top =
        body_down ? ui_mix(th->card, th->primary, 70) : th->card;
    ui_vgrad(px, m, top, ui_mix(th->card, th->bg, 110));
    ui_fill(px, ui_rect(0, my, PHONE_W, 2), th->indicator); /* progress */
    ui_fill(px, ui_rect(0, my, l->fill, 2), th->primary);

    if (ui_intersects(clip, ui_rect(0, my + 2, PHONE_W - 48, MINI_H - 2))) {
        paint_art(px, ui_rect(8, my + 8, 34, 34), l->track, 6);
        twin_coord_t x = 46, w = PHONE_W - 48 - x;
        ui_text_fit(px, FONT_NORMAL, ui_rect(x, my + 6, w, 20), UI_LEFT,
                    th->text, t->title);
        ui_text_fit(px, FONT_SM, ui_rect(x, my + 24, w, 18), UI_LEFT,
                    th->text_sec, t->artist);
    }

    ui_rect_t p = mini_play_rect(l);
    if (ui_intersects(clip, p)) {
        twin_coord_t cx = (p.left + p.right) / 2, cy = (p.top + p.bottom) / 2;
        ui_circle(px, cx, cy, 16,
                  l->pressed == HIT_PLAY ? th->primary_dk : th->primary);
        ui_icon(px, l->playing ? TWIN_SYM_PAUSE : TWIN_SYM_PLAY, 13,
                cx + (l->playing ? 0 : 1), cy, th->on_primary);
    }
}

static void list_paint(ui_view_t *v, twin_pixmap_t *px)
{
    list_t *l = (list_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v), bounds = ui_view_bounds(v);
    if (clip.left <= bounds.left && clip.top <= bounds.top &&
        clip.right >= bounds.right && clip.bottom >= bounds.bottom)
        list_snapshot(l);

    ui_rect_t area = list_area(l);
    for (int i = 0; i < PHONE_TRACK_COUNT; i++)
        if (ui_intersects(clip, row_visible(l, i)))
            paint_row(px, l, i);
    /* Below the last row when the list is shorter than the viewport */
    twin_coord_t end = row_rect(l, PHONE_TRACK_COUNT - 1).bottom;
    if (end < area.bottom)
        ui_fill(px, ui_rect(0, end, PHONE_W, area.bottom - end), th->card);
    ui_scroll_bar(px, &l->scroll);

    /* Header and mini player sit on top of rows that scrolled under them */
    ui_rect_t hdr = ui_rect(0, 0, PHONE_W, LIST_TOP);
    if (ui_intersects(clip, hdr)) {
        char count[16];
        snprintf(count, sizeof(count), "%d songs", PHONE_TRACK_COUNT);
        ui_header(px, hdr, NULL, "Music", count);
    }
    if (ui_intersects(clip, mini_rect(l)))
        paint_mini(px, l, clip);
}

static int list_hit(const list_t *l, twin_coord_t x, twin_coord_t y)
{
    if (ui_hit(mini_play_rect(l), x, y))
        return HIT_PLAY;
    if (ui_hit(mini_rect(l), x, y))
        return HIT_MINI;
    if (!ui_hit(list_area(l), x, y))
        return HIT_NONE;
    int i = (y - LIST_TOP + l->scroll.offset) / ROW_H;
    return i < PHONE_TRACK_COUNT ? i : HIT_NONE;
}

static void list_invalidate_hit(list_t *l, int h)
{
    if (h >= 0)
        ui_view_invalidate(&l->view, row_visible(l, h));
    else if (h == HIT_PLAY)
        ui_view_invalidate(&l->view, mini_play_rect(l));
    else if (h == HIT_MINI)
        ui_view_invalidate(&l->view, mini_rect(l));
}

static void list_set_pressed(list_t *l, int h)
{
    if (h == l->pressed)
        return;
    list_invalidate_hit(l, l->pressed);
    l->pressed = h;
    list_invalidate_hit(l, h);
}

static bool list_event(ui_view_t *v, const ui_event_t *e)
{
    list_t *l = (list_t *) v;
    if (e->kind == UI_SHOW) {
        /* Now Playing may have changed the song while it covered the list */
        list_refresh(l);
        return true;
    }
    if (ui_scroll_event(v, &l->scroll, e)) {
        list_set_pressed(l, HIT_NONE);
        return true;
    }
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        list_set_pressed(l, list_hit(l, e->x, e->y));
        return true;
    case UI_TAP: {
        int h = list_hit(l, e->x, e->y);
        list_set_pressed(l, HIT_NONE);
        if (h == HIT_PLAY) {
            player_play(!player.playing);
            list_refresh(l);
        } else if (h == HIT_MINI || h >= 0) {
            /* Tapping the current song resumes it rather than restarting */
            if (h >= 0 && h != player.track)
                player_select(h);
            else if (h >= 0)
                player_play(true);
            list_refresh(l);
            now_playing_open();
        }
        return true;
    }
    default:
        list_set_pressed(l, HIT_NONE);
        return false;
    }
}

void app_music_open(void)
{
    list_t *l = ui_view_new(sizeof(*l), "Music", list_paint, list_event, NULL);
    if (!l)
        return;
    l->pressed = HIT_NONE;
    list_snapshot(l);
    ui_view_set_layout(&l->view, list_layout);
    /* Start with the current song in view */
    l->scroll.offset = (l->track - 1) * ROW_H;
    list_layout(&l->view); /* clamps the offset */
    ensure_tick(&l->view, list_tick);
    ui_view_show(&l->view);
}

/*
 * Test hooks
 */
int app_music_test_elapsed(void)
{
    player_sync();
    return player_pos() / 1000;
}

int app_music_test_track(void)
{
    player_sync();
    return player.track;
}

bool app_music_test_playing(void)
{
    return player.playing;
}

/* Scroll offset of the track list on top, -1 when it is not on top */
int app_music_test_scroll(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != list_paint)
        return -1;
    return ((list_t *) v)->scroll.offset;
}
