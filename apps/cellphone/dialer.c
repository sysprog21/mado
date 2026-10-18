/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Phone: dialer keypad and the in-call screen.
 *
 * The dialer and the call are separate views so the call can be placed
 * from Contacts or the Call Log without a keypad underneath.  Both repaint
 * only what a press changes: one key, the number display, or the timer.
 */

#include <stdio.h>
#include <string.h>

#include "data.h"
#include "phone.h"

#define NUM_MAX 20
#define DISPLAY_H 50
#define BOTTOM_H 48
#define KEY_ROWS 4
#define KEY_COLS 3
#define KEY_W (PHONE_W / KEY_COLS)
#define KEY_GAP 3

/* Pseudo keys past the 12 keypad keys */
enum { KEY_DEL = 12, KEY_CALL, N_HIT };

typedef struct {
    char digit;
    const char *letters;
} dial_key_t;

static const dial_key_t keys[12] = {
    {'1', ""},     {'2', "ABC"}, {'3', "DEF"},  {'4', "GHI"},
    {'5', "JKL"},  {'6', "MNO"}, {'7', "PQRS"}, {'8', "TUV"},
    {'9', "WXYZ"}, {'*', ""},    {'0', "+"},    {'#', ""},
};

typedef struct {
    ui_view_t view;
    char number[NUM_MAX + 1];
    int len;
    int pressed; /* key or KEY_DEL/KEY_CALL, -1 when none */
} dialer_t;

/*
 * Shared helpers
 */

/* Compare phone numbers by their digits only ("15550101" = "+1-555-0101") */
static bool same_number(const char *a, const char *b)
{
    for (;;) {
        while (*a && (*a < '0' || *a > '9'))
            a++;
        while (*b && (*b < '0' || *b > '9'))
            b++;
        if (!*a || !*b)
            return !*a && !*b;
        if (*a++ != *b++)
            return false;
    }
}

static const char *contact_for(const char *number)
{
    if (!number || !*number)
        return NULL;
    for (uint32_t i = 0; i < phone_contact_count(); i++) {
        const phone_contact_t *c = phone_contact_at(i);
        if (c && c->phone && same_number(c->phone, number))
            return c->name;
    }
    return NULL;
}

/*
 * Draw a phone number centered in @r, shrinking once and then dropping
 * leading digits: the digits just typed are the ones worth seeing.
 */
static void draw_number(twin_pixmap_t *px,
                        ui_rect_t r,
                        twin_coord_t baseline,
                        twin_argb32_t color,
                        const char *s)
{
    twin_coord_t room = ui_w(r) - 8, cx = (r.left + r.right) / 2;
    int size = FONT_LARGE;
    if (ui_text_width(size, s) > room)
        size = FONT_HEADING;
    if (ui_text_width(size, s) <= room) {
        ui_text(px, size, cx, baseline, UI_CENTER, color, s);
        return;
    }
    char buf[NUM_MAX + 8];
    size_t n = strlen(s);
    const char *tail = s;
    do {
        tail++;
        n--;
        snprintf(buf, sizeof(buf), "...%s", tail);
    } while (n > 1 && ui_text_width(size, buf) > room);
    ui_text(px, size, cx, baseline, UI_CENTER, color, buf);
}

/*
 * In-call screen
 */
enum { BTN_MUTE, BTN_SPEAKER, BTN_END, N_BTN };

#define AVATAR_CY 64
#define AVATAR_R 30
#define TIMER_TOP 148
#define ROW_CY 204
#define TOGGLE_R 22
#define END_R 26

typedef struct {
    ui_view_t view;
    char name[40]; /* empty when the number matches no contact */
    char number[32];
    int seconds;
    bool muted, speaker;
    int pressed; /* BTN_* or -1 */
} incall_t;

static ui_rect_t timer_rect(void)
{
    return ui_rect(PHONE_W / 2 - 40, TIMER_TOP, 80, 22);
}

static twin_coord_t button_cx(int b)
{
    return b == BTN_MUTE ? 48 : b == BTN_SPEAKER ? PHONE_W - 48 : PHONE_W / 2;
}

/* Hit and repaint area of a bottom-row button, caption included */
static ui_rect_t button_rect(int b)
{
    return ui_rect(button_cx(b) - 36, ROW_CY - END_R - 2, 72, 2 * END_R + 26);
}

static void paint_button(twin_pixmap_t *px, incall_t *c, int b)
{
    const phone_theme_t *th = phone_theme();
    twin_coord_t cx = button_cx(b);
    bool on = b == BTN_MUTE ? c->muted : b == BTN_SPEAKER && c->speaker;
    twin_argb32_t bg, fg, cap = on ? th->primary : th->text_sec;
    const char *sym, *label;
    int r = TOGGLE_R;

    if (b == BTN_END) {
        bg = th->call_red;
        fg = UI_RGB(0xffffff);
        sym = TWIN_SYM_HANGUP;
        label = "End";
        r = END_R;
    } else {
        bg = on ? th->primary : ui_mix(th->card, th->indicator, 110);
        fg = on ? th->on_primary : th->text;
        sym = b == BTN_MUTE ? TWIN_SYM_MIC : TWIN_SYM_VOLUME;
        label = b == BTN_MUTE ? "Mute" : "Speaker";
    }
    if (c->pressed == b)
        bg = ui_darken(bg, 18);
    ui_circle(px, cx, ROW_CY, r, bg);
    ui_icon(px, sym, b == BTN_END ? FONT_LARGE : FONT_HEADING, cx, ROW_CY, fg);
    /* No slashed-mic glyph in the icon font: strike it through */
    if (b == BTN_MUTE && on)
        ui_line(px, cx - 9, ROW_CY - 9, cx + 9, ROW_CY + 9, 2, fg);
    ui_text(px, FONT_SM, cx, ROW_CY + END_R + 16, UI_CENTER, cap, label);
}

static void incall_paint(ui_view_t *v, twin_pixmap_t *px)
{
    incall_t *c = (incall_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    twin_coord_t cx = PHONE_W / 2;

    ui_vgrad(px, ui_view_bounds(v), th->card, th->bg);

    if (ui_intersects(clip, ui_rect(0, 0, PHONE_W, TIMER_TOP))) {
        ui_text(px, FONT_SM, cx, 18, UI_CENTER, th->text_sec, "Calling...");
        ui_ring(px, cx, AVATAR_CY, AVATAR_R + 5, 2,
                ui_alpha(th->call_green, 0x90));
        if (c->name[0]) {
            ui_avatar(px, cx, AVATAR_CY, AVATAR_R, c->name[0],
                      ui_avatar_color(c->name));
        } else {
            ui_circle(px, cx, AVATAR_CY, AVATAR_R, th->indicator);
            ui_icon(px, TWIN_SYM_USER, FONT_LARGE, cx, AVATAR_CY,
                    th->on_primary);
        }
        ui_text_fit(px, FONT_LARGE, ui_rect(8, 104, PHONE_W - 16, 26),
                    UI_CENTER, th->text, c->name[0] ? c->name : "Unknown");
        ui_text_fit(px, FONT_NORMAL, ui_rect(8, 128, PHONE_W - 16, 18),
                    UI_CENTER, th->text_sec, c->number);
    }
    if (ui_intersects(clip, timer_rect())) {
        char t[8];
        snprintf(t, sizeof(t), "%02d:%02d", (c->seconds / 60) % 100,
                 c->seconds % 60);
        ui_text_in(px, FONT_HEADING, timer_rect(), UI_CENTER, th->primary, t);
    }
    for (int b = 0; b < N_BTN; b++)
        if (ui_intersects(clip, button_rect(b)))
            paint_button(px, c, b);
}

static twin_time_t incall_tick(ui_view_t *v)
{
    incall_t *c = (incall_t *) v;
    c->seconds++;
    ui_view_invalidate(v, timer_rect());
    return 1000;
}

static int incall_button_at(twin_coord_t x, twin_coord_t y)
{
    for (int b = 0; b < N_BTN; b++)
        if (ui_hit(button_rect(b), x, y))
            return b;
    return -1;
}

static ui_rect_t incall_button(ui_view_t *v, int b)
{
    (void) v;
    return button_rect(b);
}

static void incall_set_pressed(incall_t *c, int b)
{
    ui_view_set_pressed(&c->view, &c->pressed, b, incall_button);
}

static bool incall_event(ui_view_t *v, const ui_event_t *e)
{
    incall_t *c = (incall_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        incall_set_pressed(c, incall_button_at(e->x, e->y));
        return true;
    case UI_TAP: {
        int b = incall_button_at(e->x, e->y);
        incall_set_pressed(c, -1);
        if (b == BTN_END) {
            ui_view_close(v); /* frees c */
        } else if (b >= 0) {
            if (b == BTN_MUTE)
                c->muted = !c->muted;
            else
                c->speaker = !c->speaker;
            ui_view_invalidate(v, button_rect(b));
        }
        return true;
    }
    default:
        incall_set_pressed(c, -1);
        return false;
    }
}

void app_dialer_call(const char *name, const char *number)
{
    if (!number || !*number)
        return;
    incall_t *c =
        ui_view_new(sizeof(*c), "Call", incall_paint, incall_event, NULL);
    if (!c)
        return;
    /* Copies: the caller's strings may live in a view that closes */
    if (!name || !*name || !strcmp(name, "Unknown"))
        name = contact_for(number);
    snprintf(c->name, sizeof(c->name), "%s", name ? name : "");
    snprintf(c->number, sizeof(c->number), "%s", number);
    c->pressed = -1;
    ui_view_every(&c->view, 1000, incall_tick);
    ui_view_show(&c->view);
}

/*
 * Dialer
 */
static ui_rect_t display_rect(void)
{
    return ui_rect(0, 0, PHONE_W, DISPLAY_H);
}

static ui_rect_t key_rect(ui_view_t *v, int i)
{
    twin_coord_t h = ui_view_height(v), bottom_y = h - BOTTOM_H;
    twin_coord_t key_h = (h - DISPLAY_H - BOTTOM_H) / KEY_ROWS;
    if (i == KEY_DEL)
        return ui_rect(KEY_GAP * 2, bottom_y + 5, 70, BOTTOM_H - 10);
    if (i == KEY_CALL)
        return ui_rect(KEY_GAP * 2 + 76, bottom_y + 5,
                       PHONE_W - KEY_GAP * 4 - 76, BOTTOM_H - 10);
    return ui_rect((i % KEY_COLS) * KEY_W + KEY_GAP,
                   DISPLAY_H + (i / KEY_COLS) * key_h + KEY_GAP,
                   KEY_W - 2 * KEY_GAP, key_h - 2 * KEY_GAP);
}

static void paint_key(twin_pixmap_t *px, dialer_t *d, int i)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t r = key_rect(&d->view, i);
    bool down = d->pressed == i;
    twin_coord_t cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;

    if (i == KEY_DEL) {
        twin_argb32_t bg = ui_mix(th->card, th->indicator, 110);
        ui_rrect(px, r, 10, down ? th->primary_dk : bg);
        ui_icon(px, TWIN_SYM_BACKSPACE, FONT_HEADING, cx, cy,
                down     ? th->on_primary
                : d->len ? th->text
                         : th->text_sec);
        return;
    }
    if (i == KEY_CALL) {
        /* Dimmed while there is nothing to dial */
        twin_argb32_t bg =
            d->len ? th->call_green : ui_mix(th->call_green, th->card, 110);
        ui_rrect(px, r, ui_h(r) / 2, down ? ui_darken(bg, 18) : bg);
        twin_coord_t w = 24 + ui_text_width(FONT_HEADING, "Call");
        twin_coord_t x = cx - w / 2;
        ui_icon(px, TWIN_SYM_PHONE, FONT_HEADING, x + 8, cy, UI_RGB(0xffffff));
        ui_text(px, FONT_HEADING, x + 24,
                ui_baseline(FONT_HEADING, r.top, ui_h(r)), UI_LEFT,
                UI_RGB(0xffffff), "Call");
        return;
    }
    char s[2] = {keys[i].digit, 0};
    twin_argb32_t bg =
        down ? th->primary_dk : ui_mix(th->card, th->indicator, 110);
    ui_rrect(px, r, 10, bg);
    if (!keys[i].letters[0]) {
        /* * and # sit lower in the em box; center them optically */
        ui_text_in(px, FONT_LARGE,
                   ui_rect(r.left, r.top + (keys[i].digit == '*' ? 5 : 0),
                           ui_w(r), ui_h(r)),
                   UI_CENTER, down ? th->on_primary : th->text, s);
        return;
    }
    ui_text(px, FONT_LARGE, cx, r.top + 21, UI_CENTER,
            down ? th->on_primary : th->text, s);
    ui_text(px, 9, cx, r.top + 31, UI_CENTER,
            down ? th->on_primary : th->text_sec, keys[i].letters);
}

static void dialer_paint(ui_view_t *v, twin_pixmap_t *px)
{
    dialer_t *d = (dialer_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);

    if (ui_intersects(clip, display_rect())) {
        ui_vgrad(px, display_rect(), th->card, ui_mix(th->card, th->bg, 128));
        if (!d->len) {
            ui_text_in(px, FONT_NORMAL, display_rect(), UI_CENTER, th->text_sec,
                       "Enter a number");
        } else {
            const char *who = contact_for(d->number);
            draw_number(px, display_rect(), who ? 26 : 33, th->text, d->number);
            if (who)
                ui_text_fit(px, FONT_SM, ui_rect(8, 31, PHONE_W - 16, 16),
                            UI_CENTER, th->primary, who);
        }
        ui_separator(px, display_rect());
    }
    ui_rect_t pad =
        ui_rect(0, DISPLAY_H, PHONE_W, ui_view_height(v) - DISPLAY_H);
    if (ui_intersects(clip, pad))
        ui_fill(px, pad, th->card);
    for (int i = 0; i < N_HIT; i++)
        if (ui_intersects(clip, key_rect(v, i)))
            paint_key(px, d, i);
}

static int dialer_key_at(ui_view_t *v, twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < N_HIT; i++)
        if (ui_hit(key_rect(v, i), x, y))
            return i;
    return -1;
}

static void dialer_set_pressed(dialer_t *d, int k)
{
    ui_view_set_pressed(&d->view, &d->pressed, k, key_rect);
}

static void dialer_changed(dialer_t *d, bool was_empty)
{
    ui_view_invalidate(&d->view, display_rect());
    /* Backspace and Call change look when the number empties or fills */
    if (was_empty != !d->len) {
        ui_view_invalidate(&d->view, key_rect(&d->view, KEY_DEL));
        ui_view_invalidate(&d->view, key_rect(&d->view, KEY_CALL));
    }
}

static void dialer_press(dialer_t *d, int k)
{
    bool was_empty = !d->len;
    if (k == KEY_CALL) {
        if (d->len)
            app_dialer_call(NULL, d->number);
        return;
    }
    if (k == KEY_DEL) {
        if (!d->len)
            return;
        d->number[--d->len] = 0;
    } else if (k >= 0 && k < 12) {
        if (d->len >= NUM_MAX)
            return;
        d->number[d->len++] = keys[k].digit;
        d->number[d->len] = 0;
    } else {
        return;
    }
    dialer_changed(d, was_empty);
}

/* Map a typed character to a keypad key; '+' types 0's letter */
static int dialer_key_for(int ch)
{
    if (ch == UI_KEY_BACKSPACE)
        return KEY_DEL;
    if (ch == UI_KEY_ENTER)
        return KEY_CALL;
    if (ch < 0)
        return -1;
    for (int i = 0; i < 12; i++)
        if (keys[i].digit == (char) ch)
            return i;
    return -1;
}

static bool dialer_event(ui_view_t *v, const ui_event_t *e)
{
    dialer_t *d = (dialer_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        dialer_set_pressed(d, dialer_key_at(v, e->x, e->y));
        return true;
    case UI_TAP: {
        int k = dialer_key_at(v, e->x, e->y);
        dialer_set_pressed(d, -1);
        dialer_press(d, k);
        return true;
    }
    case UI_KEY: {
        if (e->ucs4 == '+') {
            if (d->len < NUM_MAX) {
                bool was_empty = !d->len;
                d->number[d->len++] = '+';
                d->number[d->len] = 0;
                dialer_changed(d, was_empty);
            }
            return true;
        }
        int k = dialer_key_for(ui_key_ascii(e));
        if (k < 0)
            return false;
        dialer_press(d, k);
        return true;
    }
    default:
        dialer_set_pressed(d, -1);
        return false;
    }
}

void app_dialer_open(void)
{
    dialer_t *d =
        ui_view_new(sizeof(*d), "Phone", dialer_paint, dialer_event, NULL);
    if (!d)
        return;
    d->pressed = -1;
    ui_view_show(&d->view);
}

/*
 * Test hooks: inspect the dialer or call on top of the stack
 */
const char *app_dialer_test_number(void)
{
    ui_view_t *v = phone_top();
    if (v && v->paint == dialer_paint)
        return ((dialer_t *) v)->number;
    if (v && v->paint == incall_paint)
        return ((incall_t *) v)->number;
    return NULL;
}

const char *app_dialer_test_name(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == incall_paint ? ((incall_t *) v)->name : NULL;
}

/* Elapsed call seconds, or -1 when no call is on top */
int app_dialer_test_seconds(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == incall_paint ? ((incall_t *) v)->seconds : -1;
}

/* Bit 0: muted, bit 1: speaker */
int app_dialer_test_toggles(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != incall_paint)
        return -1;
    incall_t *c = (incall_t *) v;
    return (c->muted ? 1 : 0) | (c->speaker ? 2 : 0);
}
