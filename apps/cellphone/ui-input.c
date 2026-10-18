/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone UI kit: text field and soft keyboard.
 */

#include <string.h>

#include "ui-internal.h"

/*
 * Text field
 */
void ui_field(twin_pixmap_t *px,
              ui_rect_t r,
              const ui_field_t *f,
              const char *placeholder)
{
    const phone_theme_t *th = phone_theme();
    ui_rrect(px, r, 6, th->card);
    ui_rrect_stroke(px, r, 6, 1, f->focused ? th->primary : th->separator);
    ui_rect_t t = ui_rect(r.left + 4, r.top, ui_w(r) - 8, ui_h(r));
    if (f->len)
        ui_text_fit(px, FONT_NORMAL, t, UI_LEFT, th->text, f->text);
    else if (placeholder)
        /* Leave room for the caret in front of the placeholder */
        ui_text_in(
            px, FONT_NORMAL,
            ui_rect(t.left + (f->focused ? 3 : 0), t.top, ui_w(t), ui_h(t)),
            UI_LEFT, th->text_sec, placeholder);
    if (f->focused) {
        twin_coord_t w = f->len ? ui_text_width(FONT_NORMAL, f->text) : 0;
        twin_coord_t cx = min((twin_coord_t) (t.left + 4 + w + 1),
                              (twin_coord_t) (r.right - 6));
        ui_fill(px, ui_rect(cx, r.top + 6, 1, ui_h(r) - 12), th->primary);
    }
}

void ui_field_set(ui_field_t *f, const char *s)
{
    size_t n = strlen(s);
    if (n >= sizeof(f->text))
        n = sizeof(f->text) - 1;
    memcpy(f->text, s, n);
    f->text[n] = 0;
    f->len = (int) n;
}

bool ui_field_key(ui_field_t *f, int key)
{
    if (key == UI_KEY_BACKSPACE) {
        if (!f->len)
            return false;
        f->text[--f->len] = 0;
        return true;
    }
    if (key >= 0x20 && key < 0x7f && f->len < (int) sizeof(f->text) - 1) {
        f->text[f->len++] = (char) key;
        f->text[f->len] = 0;
        return true;
    }
    return false;
}

/*
 * Soft keyboard: four rows, letters or digits/symbols
 */
typedef struct {
    ui_rect_t r;
    int code;
    const char *label;
} kbd_key_t;

#define KBD_MAX_KEYS 40

static const char *kbd_rows_alpha[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
static const char *kbd_rows_sym[3] = {"1234567890", "-/:;()$&@", ".,?!'\"#"};

static int _kbd_layout(const ui_kbd_t *k, ui_rect_t r, kbd_key_t *keys)
{
    static char labels[KBD_MAX_KEYS][2];
    const char **rows = k->symbols ? kbd_rows_sym : kbd_rows_alpha;
    int n = 0;
    twin_coord_t kw = (ui_w(r) - 4) / 10, kh = (ui_h(r) - 10) / 4;
    for (int row = 0; row < 3; row++) {
        const char *keysrow = rows[row];
        int count = (int) strlen(keysrow);
        twin_coord_t y = r.top + 4 + row * (kh + 2);
        twin_coord_t x = r.left + 2 + (10 - count) * kw / 2;
        if (row == 2)
            x = r.left + 2 + kw * 3 / 2;
        for (int i = 0; i < count; i++) {
            char c = keysrow[i];
            if (k->shift && c >= 'a' && c <= 'z')
                c = (char) (c - 'a' + 'A');
            labels[n][0] = c;
            labels[n][1] = 0;
            keys[n] =
                (kbd_key_t) {ui_rect(x + i * kw, y, kw - 2, kh), c, labels[n]};
            n++;
        }
        if (row == 2) {
            twin_coord_t wide = kw * 3 / 2 - 2;
            if (!k->symbols)
                keys[n++] = (kbd_key_t) {ui_rect(r.left + 2, y, wide, kh),
                                         UI_KEY_SHIFT, "^"};
            keys[n++] = (kbd_key_t) {ui_rect(r.right - 2 - wide, y, wide, kh),
                                     UI_KEY_BACKSPACE, TWIN_SYM_BACKSPACE};
        }
    }
    twin_coord_t y = r.top + 4 + 3 * (kh + 2);
    keys[n++] = (kbd_key_t) {ui_rect(r.left + 2, y, kw * 2 - 2, kh),
                             UI_KEY_MODE, k->symbols ? "ABC" : "?123"};
    keys[n++] = (kbd_key_t) {ui_rect(r.left + 2 + kw * 2, y, kw * 5 - 2, kh),
                             ' ', "space"};
    keys[n++] =
        (kbd_key_t) {ui_rect(r.left + 2 + kw * 7, y, kw - 2, kh), '.', "."};
    keys[n++] = (kbd_key_t) {ui_rect(r.left + 2 + kw * 8, y, kw * 2 - 2, kh),
                             UI_KEY_ENTER, TWIN_SYM_SEND};
    return n;
}

void ui_kbd_init(ui_kbd_t *k)
{
    memset(k, 0, sizeof(*k));
    k->pressed = -1;
}

ui_rect_t ui_kbd_rect(twin_coord_t view_h)
{
    return ui_rect(0, view_h - UI_KBD_H, PHONE_W, UI_KBD_H);
}

void ui_kbd_draw(twin_pixmap_t *px, const ui_kbd_t *k, ui_rect_t r)
{
    const phone_theme_t *th = phone_theme();
    kbd_key_t keys[KBD_MAX_KEYS];
    int n = _kbd_layout(k, r, keys);
    ui_fill(px, r, ui_mix(th->bg, th->text_sec, 60));
    for (int i = 0; i < n; i++) {
        bool special = keys[i].code >= 0x100 || keys[i].code == UI_KEY_ENTER ||
                       keys[i].code == UI_KEY_BACKSPACE;
        twin_argb32_t bg =
            special ? ui_mix(th->card, th->text_sec, 60) : th->card;
        if (keys[i].code == UI_KEY_SHIFT && k->shift)
            bg = th->primary;
        if (i == k->pressed)
            bg = ui_mix(bg, th->primary, 120);
        /* 1 px pressed travel for tactile feedback */
        ui_rect_t kr = keys[i].r;
        if (i == k->pressed) {
            kr.top++;
            kr.bottom++;
        }
        ui_rrect(px, kr, 4, bg);
        int size = keys[i].code == ' ' || keys[i].code >= 0x100 ? FONT_SM
                                                                : FONT_NORMAL;
        if (_is_symbol(keys[i].label))
            ui_icon(px, keys[i].label, FONT_NORMAL, (kr.left + kr.right) / 2,
                    (kr.top + kr.bottom) / 2, th->text);
        else
            ui_text_in(px, size, kr, UI_CENTER, th->text, keys[i].label);
    }
}

bool ui_kbd_find(const ui_kbd_t *k, ui_rect_t r, int code, ui_rect_t *out)
{
    kbd_key_t keys[KBD_MAX_KEYS];
    int n = _kbd_layout(k, r, keys);
    for (int i = 0; i < n; i++) {
        if (keys[i].code == code) {
            *out = keys[i].r;
            return true;
        }
    }
    return false;
}

/* Pressed keys draw 1 px lower: cover both positions */
static ui_rect_t _key_dirty(const kbd_key_t *keys, int i)
{
    if (i < 0)
        return (ui_rect_t) {0, 0, 0, 0};
    ui_rect_t r = keys[i].r;
    r.bottom++;
    return r;
}

static int _kbd_event(ui_kbd_t *k,
                      const kbd_key_t *keys,
                      int n,
                      const ui_event_t *e);

int ui_kbd_event(ui_kbd_t *k, ui_rect_t r, const ui_event_t *e)
{
    kbd_key_t keys[KBD_MAX_KEYS];
    int before = k->pressed;
    bool shift = k->shift, symbols = k->symbols;
    int n = _kbd_layout(k, r, keys);
    int key = _kbd_event(k, keys, n, e);
    if (shift != k->shift || symbols != k->symbols)
        k->dirty = r; /* every label changed */
    else if (before != k->pressed)
        k->dirty = ui_rect_union(_key_dirty(keys, before),
                                 _key_dirty(keys, k->pressed));
    else
        k->dirty = (ui_rect_t) {0, 0, 0, 0};
    return key;
}

/* @keys is the layout as it was before the event */
static int _kbd_event(ui_kbd_t *k,
                      const kbd_key_t *keys,
                      int n,
                      const ui_event_t *e)
{
    int hit = -1;
    for (int i = 0; i < n; i++)
        if (ui_hit(keys[i].r, e->x, e->y))
            hit = i;

    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
    case UI_DRAG:
        k->pressed = hit;
        return UI_KEY_NONE;
    case UI_TAP: {
        k->pressed = -1;
        if (hit < 0)
            return UI_KEY_NONE;
        int code = keys[hit].code;
        if (code == UI_KEY_SHIFT) {
            k->shift = !k->shift;
            return code;
        }
        if (code == UI_KEY_MODE) {
            k->symbols = !k->symbols;
            k->shift = false;
            return code;
        }
        if (code < 0x100)
            k->shift = false; /* one-shot; the layout already uppercased */
        return code;
    }
    case UI_RELEASE:
    case UI_CANCEL:
        k->pressed = -1;
        return UI_KEY_NONE;
    default:
        return UI_KEY_NONE;
    }
}
