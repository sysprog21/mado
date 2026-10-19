/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Calculator: four functions on 64-bit integers scaled by 10^6.
 *
 * Reference app for the UI kit: all state lives in the view struct, the
 * keypad is a static table, and a key press repaints only that key and the
 * display.
 */

#include <stdio.h>
#include <string.h>

#include "phone.h"

#define DISPLAY_H 48
#define COLS 4
#define ROWS 5
#define SCALE 1000000LL
#define BUF_LEN 24

typedef struct {
    const char *label;
    char col, row, span;
    char kind; /* 'd' digit, 'o' operator, 'f' function, '=' equals */
} calc_key_t;

static const calc_key_t keys[] = {
    {"C", 0, 0, 1, 'f'}, {"+/-", 1, 0, 1, 'f'}, {"%", 2, 0, 1, 'f'},
    {"/", 3, 0, 1, 'o'}, {"7", 0, 1, 1, 'd'},   {"8", 1, 1, 1, 'd'},
    {"9", 2, 1, 1, 'd'}, {"x", 3, 1, 1, 'o'},   {"4", 0, 2, 1, 'd'},
    {"5", 1, 2, 1, 'd'}, {"6", 2, 2, 1, 'd'},   {"-", 3, 2, 1, 'o'},
    {"1", 0, 3, 1, 'd'}, {"2", 1, 3, 1, 'd'},   {"3", 2, 3, 1, 'd'},
    {"+", 3, 3, 1, 'o'}, {"0", 0, 4, 2, 'd'},   {".", 2, 4, 1, 'd'},
    {"=", 3, 4, 1, '='},
};

#define N_KEYS ((int) (sizeof(keys) / sizeof(keys[0])))

typedef struct {
    ui_view_t view;
    char display[BUF_LEN];
    int64_t accumulator; /* left operand, scaled */
    int64_t operand;     /* value being entered, scaled */
    char pending;        /* '+', '-', '*', '/' or 0 */
    bool new_input;      /* next digit starts a new number */
    int32_t frac_div;    /* 0: integer part; 10, 100, ...: after the dot */
    bool negative;
    bool fresh; /* an operator was just pressed: no operand typed yet */
    bool error;
    int pressed; /* key index, -1 when none */
} calc_t;

static ui_rect_t key_rect(ui_view_t *v, int i)
{
    twin_coord_t kw = PHONE_W / COLS;
    twin_coord_t kh = (ui_view_height(v) - DISPLAY_H) / ROWS;
    return ui_rect(keys[i].col * kw + 2, DISPLAY_H + keys[i].row * kh + 2,
                   keys[i].span * kw - 4, kh - 4);
}

static ui_rect_t display_rect(void)
{
    return ui_rect(0, 0, PHONE_W, DISPLAY_H);
}

static void format_scaled(calc_t *c, int64_t val)
{
    char *p = c->display;
    int64_t a = val < 0 ? -val : val;
    int pos = snprintf(p, BUF_LEN, "%s%lld", val < 0 ? "-" : "",
                       (long long) (a / SCALE));
    int64_t frac = a % SCALE;
    if (frac && pos < BUF_LEN - 2) {
        p[pos++] = '.';
        for (int64_t div = SCALE / 10; div && frac && pos < BUF_LEN - 1;
             div /= 10) {
            p[pos++] = (char) ('0' + frac / div);
            frac %= div;
        }
        while (p[pos - 1] == '0')
            pos--;
    }
    p[pos] = 0;
}

/*
 * (a * b) / SCALE without a 128-bit type (32-bit targets lack one):
 * split both operands into integer and fractional parts.
 */
static bool scaled_mul(int64_t a, int64_t b, int64_t *res)
{
    int64_t ai = a / SCALE, af = a % SCALE, bi = b / SCALE, bf = b % SCALE;
    int64_t t0, t1, t2, sum;
    if (__builtin_mul_overflow(ai, bi, &t0) ||
        __builtin_mul_overflow(t0, SCALE, &t0) ||
        __builtin_mul_overflow(ai, bf, &t1) ||
        __builtin_mul_overflow(af, bi, &t2) ||
        __builtin_add_overflow(t0, t1, &sum) ||
        __builtin_add_overflow(sum, t2, &sum) ||
        __builtin_add_overflow(sum, af * bf / SCALE, &sum))
        return false;
    *res = sum;
    return true;
}

/*
 * (a * SCALE) / b, truncated toward zero, without forming the product:
 * integer quotient first, then the six fractional digits by long division
 * of the remainder.  Fails only when the result itself does not fit.
 */
static bool scaled_div(int64_t a, int64_t b, int64_t *res)
{
    if (!b)
        return false;
    bool neg = (a < 0) != (b < 0);
    uint64_t ua = a < 0 ? -(uint64_t) a : (uint64_t) a;
    uint64_t ub = b < 0 ? -(uint64_t) b : (uint64_t) b;
    uint64_t q = ua / ub, r = ua % ub;
    if (q > (uint64_t) INT64_MAX / SCALE)
        return false;
    q *= SCALE;
    uint64_t frac = 0;
    for (int64_t div = SCALE / 10; div; div /= 10) {
        /* Next digit of r / ub: 10 r = d ub + r', with r, r' < ub, so the
         * running sum stays below 2 ub and never wraps */
        uint64_t t = 0;
        int d = 0;
        for (int k = 0; k < 10; k++) {
            t += r;
            if (t >= ub) {
                t -= ub;
                d++;
            }
        }
        r = t;
        frac += (uint64_t) d * (uint64_t) div;
    }
    if (q + frac > (uint64_t) INT64_MAX)
        return false;
    *res = neg ? -(int64_t) (q + frac) : (int64_t) (q + frac);
    return true;
}

static bool execute(calc_t *c)
{
    switch (c->pending) {
    case '+':
        if (__builtin_add_overflow(c->accumulator, c->operand, &c->accumulator))
            goto fail;
        break;
    case '-':
        if (__builtin_sub_overflow(c->accumulator, c->operand, &c->accumulator))
            goto fail;
        break;
    case '*':
        if (!scaled_mul(c->accumulator, c->operand, &c->accumulator))
            goto fail;
        break;
    case '/':
        if (!scaled_div(c->accumulator, c->operand, &c->accumulator))
            goto fail;
        break;
    default:
        c->accumulator = c->operand;
        break;
    }
    /* INT64_MIN has no positive counterpart: +/- and the display negate */
    if (c->accumulator == INT64_MIN)
        goto fail;
    return true;
fail:
    c->error = true;
    return false;
}

static void reset(calc_t *c)
{
    c->accumulator = c->operand = 0;
    c->pending = 0;
    c->new_input = true;
    c->frac_div = 0;
    c->negative = c->fresh = c->error = false;
    strcpy(c->display, "0");
}

static void append_digit(calc_t *c, char digit)
{
    if (c->new_input) {
        c->display[0] = digit;
        c->display[1] = 0;
        c->new_input = false;
        c->frac_div = 0;
        c->negative = false;
        c->operand = 0;
    } else {
        if (c->frac_div > SCALE)
            return;
        size_t len = strlen(c->display);
        /* A lone "0" or "-0" is replaced, not extended */
        if (!c->frac_div && !c->operand && c->display[len - 1] == '0')
            len--;
        size_t digits = len - (c->display[0] == '-');
        if (len >= BUF_LEN - 2 || (!c->frac_div && digits >= 12))
            return;
        c->display[len] = digit;
        c->display[len + 1] = 0;
    }
    int64_t d = digit - '0';
    int64_t step = c->frac_div ? d * (SCALE / c->frac_div) : 0;
    if (c->frac_div) {
        c->operand += c->negative ? -step : step;
        c->frac_div *= 10;
    } else {
        c->operand = c->operand * 10 + (c->negative ? -d : d) * SCALE;
    }
}

static void append_dot(calc_t *c)
{
    if (c->frac_div)
        return;
    c->frac_div = 10;
    if (c->new_input) {
        strcpy(c->display, "0.");
        c->new_input = false;
        c->operand = 0;
        c->negative = false;
    } else if (strlen(c->display) < BUF_LEN - 2) {
        strcat(c->display, ".");
    }
}

static void press_key(calc_t *c, int i)
{
    const char *t = keys[i].label;
    if (!strcmp(t, "C")) {
        reset(c);
        return;
    }
    if (c->error)
        return;
    if (!strcmp(t, "+/-")) {
        if (c->new_input && (c->fresh || !c->operand)) {
            /* Nothing typed yet: sign the number about to be entered */
            c->new_input = c->fresh = false;
            c->frac_div = 0;
            c->operand = 0;
            c->negative = true;
            strcpy(c->display, "-0");
            return;
        }
        if (c->operand == INT64_MIN) {
            c->error = true;
            return;
        }
        c->operand = -c->operand;
        c->negative = !c->negative;
        format_scaled(c, c->operand);
    } else if (!strcmp(t, "%")) {
        c->operand /= 100;
        format_scaled(c, c->operand);
        c->new_input = true;
        c->fresh = false;
        c->frac_div = 0;
    } else if (keys[i].kind == 'd') {
        if (t[0] == '.')
            append_dot(c);
        else
            append_digit(c, t[0]);
    } else if (keys[i].kind == '=') {
        if (!c->pending)
            return;
        if (execute(c)) {
            c->operand = c->accumulator;
            c->pending = 0;
            format_scaled(c, c->operand);
            c->new_input = true;
            c->fresh = false;
            c->frac_div = 0;
        }
    } else {
        char op = t[0] == 'x' ? '*' : t[0];
        if (c->pending && !c->new_input) {
            if (!execute(c))
                return;
            c->operand = c->accumulator;
            format_scaled(c, c->operand);
        } else {
            c->accumulator = c->operand;
        }
        c->pending = op;
        c->new_input = c->fresh = true;
        c->frac_div = 0;
    }
}

static void calc_paint(ui_view_t *v, twin_pixmap_t *px)
{
    calc_t *c = (calc_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);

    if (ui_intersects(clip, display_rect())) {
        ui_vgrad(px, display_rect(), th->card, ui_mix(th->card, th->bg, 128));
        ui_text_fit(px, FONT_LARGE, ui_rect(0, 4, PHONE_W - 6, DISPLAY_H - 8),
                    UI_RIGHT, th->text, c->error ? "Error" : c->display);
        if (c->pending) {
            char op[2] = {c->pending == '*' ? 'x' : c->pending, 0};
            ui_text(px, FONT_SM, 8, 18, UI_LEFT, th->primary, op);
        }
    }
    ui_fill(px, ui_rect(0, DISPLAY_H, PHONE_W, ui_view_height(v) - DISPLAY_H),
            th->card);
    for (int i = 0; i < N_KEYS; i++) {
        ui_rect_t r = key_rect(v, i);
        if (!ui_intersects(r, clip))
            continue;
        bool down = i == c->pressed;
        twin_argb32_t bg = th->indicator, fg = th->text;
        if (keys[i].kind == 'o' || keys[i].kind == '=') {
            bg = keys[i].kind == '=' ? th->primary
                                     : ui_mix(th->indicator, th->primary, 90);
            fg = keys[i].kind == '=' ? th->on_primary : th->text;
        }
        if (down) {
            bg = th->primary_dk;
            fg = th->on_primary;
        }
        ui_rrect(px, r, 6, bg);
        ui_text_in(px, FONT_HEADING, r, UI_CENTER, fg, keys[i].label);
    }
}

/* Key for a typed character: digits, + - / . = %, x or * multiplies,
 * Enter is '=', Escape and 'c' clear */
static int key_for(char ch)
{
    if (ch == '*')
        ch = 'x';
    else if (ch == UI_KEY_ENTER)
        ch = '=';
    else if (ch == 27 || ch == 'c')
        ch = 'C';
    for (int i = 0; i < N_KEYS; i++)
        if (keys[i].label[0] == ch && !keys[i].label[1])
            return i;
    return -1;
}

static int key_at(ui_view_t *v, twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < N_KEYS; i++)
        if (ui_hit(key_rect(v, i), x, y))
            return i;
    return -1;
}

static void set_pressed(calc_t *c, int k)
{
    ui_view_set_pressed(&c->view, &c->pressed, k, key_rect);
}

static bool calc_event(ui_view_t *v, const ui_event_t *e)
{
    calc_t *c = (calc_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        set_pressed(c, key_at(v, e->x, e->y));
        return true;
    case UI_TAP: {
        int k = key_at(v, e->x, e->y);
        set_pressed(c, -1);
        if (k >= 0) {
            press_key(c, k);
            ui_view_invalidate(v, display_rect());
        }
        return true;
    }
    case UI_KEY: {
        int ch = ui_key_ascii(e);
        int k = ch < 0 ? -1 : key_for((char) ch);
        if (k < 0)
            return false;
        press_key(c, k);
        ui_view_invalidate(v, display_rect());
        return true;
    }
    default:
        set_pressed(c, -1);
        return false;
    }
}

void app_calc_open(void)
{
    calc_t *c =
        ui_view_new(sizeof(*c), "Calculator", calc_paint, calc_event, NULL);
    if (!c)
        return;
    reset(c);
    c->pressed = -1;
    ui_view_show(&c->view);
}

/* Test hook: feed typed keys ("12+3=") to the calculator on top */
const char *app_calc_eval(const char *typed)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != calc_paint)
        return NULL;
    calc_t *c = (calc_t *) v;
    for (const char *p = typed; *p; p++) {
        int k = key_for(*p);
        if (k >= 0)
            press_key(c, k);
    }
    ui_view_invalidate(v, display_rect());
    return c->error ? "Error" : c->display;
}
