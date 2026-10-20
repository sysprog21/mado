/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Calculator: four functions on 64-bit integers scaled by 10^6.
 *
 * One source serves the desktop demo and the phone app.  Both draw into a
 * twin window, so the keypad, the arithmetic, the painting and the input
 * handling are shared; a front end only supplies the window and a palette.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#if defined(CONFIG_DEMO_CELLPHONE)
#include "phone.h"
#endif

#define COLS 4
#define ROWS 5
#define SCALE 1000000LL
#define BUF_LEN 24

#if defined(CONFIG_FONT_MONTSERRAT)
#define FONT (&twin_font_montserrat)
#else
#define FONT NULL /* the built-in font */
#endif

/* The erase key shows the backspace symbol where the icon font is built */
#if defined(CONFIG_FONT_ICONS)
#define ERASE TWIN_SYM_BACKSPACE
#define ERASE_FONT (&twin_font_icons)
#else
#define ERASE "<"
#define ERASE_FONT FONT
#endif

typedef enum { DIGIT, FUNCTION, OPERATOR, EQUALS, N_KINDS } calc_kind_t;

typedef struct {
    const char *label;
    char col, row, span;
    calc_kind_t kind;
} calc_key_t;

static const calc_key_t keys[] = {
    {"C", 0, 0, 1, FUNCTION},   {ERASE, 1, 0, 1, FUNCTION},
    {"%", 2, 0, 1, FUNCTION},   {"/", 3, 0, 1, OPERATOR},
    {"7", 0, 1, 1, DIGIT},      {"8", 1, 1, 1, DIGIT},
    {"9", 2, 1, 1, DIGIT},      {"x", 3, 1, 1, OPERATOR},
    {"4", 0, 2, 1, DIGIT},      {"5", 1, 2, 1, DIGIT},
    {"6", 2, 2, 1, DIGIT},      {"-", 3, 2, 1, OPERATOR},
    {"1", 0, 3, 1, DIGIT},      {"2", 1, 3, 1, DIGIT},
    {"3", 2, 3, 1, DIGIT},      {"+", 3, 3, 1, OPERATOR},
    {"+/-", 0, 4, 1, FUNCTION}, {"0", 1, 4, 1, DIGIT},
    {".", 2, 4, 1, DIGIT},      {"=", 3, 4, 1, EQUALS},
};

#define N_KEYS ((int) (sizeof(keys) / sizeof(keys[0])))

typedef struct {
    twin_argb32_t bg, text, accent; /* accent: the pending operator */
    twin_argb32_t key[N_KINDS], key_text[N_KINDS];
    twin_argb32_t down, down_text;
} calc_palette_t;

typedef struct {
    twin_window_t *window;
    twin_path_t *path; /* reused by every paint; NULL draws square keys */
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

/*
 * Arithmetic and entry
 */

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

/* Operand, sign and fraction position from the digits on display, as
 * append_digit() would have built them */
static void parse_display(calc_t *c)
{
    const char *p = c->display;
    int64_t v = 0;
    c->negative = *p == '-';
    c->frac_div = 0;
    for (p += c->negative; *p; p++) {
        if (*p == '.') {
            c->frac_div = 10;
        } else if (c->frac_div) {
            v += (*p - '0') * (SCALE / c->frac_div);
            c->frac_div *= 10;
        } else {
            v = v * 10 + (*p - '0') * SCALE;
        }
    }
    c->operand = c->negative ? -v : v;
}

/* Drop the last character typed; a result or an operand not yet begun has
 * none */
static void erase(calc_t *c)
{
    if (c->new_input)
        return;
    size_t len = strlen(c->display) - 1;
    c->display[len] = 0;
    if (!len || !strcmp(c->display, "-"))
        strcpy(c->display, "0");
    parse_display(c);
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
    if (!strcmp(t, ERASE)) {
        erase(c);
    } else if (!strcmp(t, "+/-")) {
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
        /* Flip the sign on display too: re-formatting would drop a trailing
         * dot or show "0" for a negative entry still being typed */
        c->operand = -c->operand;
        c->negative = !c->negative;
        size_t len = strlen(c->display);
        if (c->display[0] == '-') {
            memmove(c->display, c->display + 1, len);
        } else if (len < BUF_LEN - 1) {
            memmove(c->display + 1, c->display, len + 1);
            c->display[0] = '-';
        }
    } else if (!strcmp(t, "%")) {
        c->operand /= 100;
        format_scaled(c, c->operand);
        c->new_input = true;
        c->fresh = false;
        c->frac_div = 0;
    } else if (keys[i].kind == DIGIT) {
        if (t[0] == '.')
            append_dot(c);
        else
            append_digit(c, t[0]);
    } else if (keys[i].kind == EQUALS) {
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

static int key_named(const char *label)
{
    for (int i = 0; i < N_KEYS; i++)
        if (!strcmp(keys[i].label, label))
            return i;
    return -1;
}

/* Key for a typed character: digits, + - / . = %, x or * multiplies,
 * Enter is '=', Backspace and Delete erase, Escape and 'c' clear */
static int key_for(int ch)
{
    if (ch <= 0 || ch >= 0x80)
        return -1;
    const char *want = (char[2]) {(char) ch, 0};
    if (ch == '*')
        want = "x";
    else if (ch == '\r' || ch == '\n')
        want = "=";
    else if (ch == 8 || ch == 127)
        want = ERASE;
    else if (ch == 27 || ch == 'c')
        want = "C";
    return key_named(want);
}

static const char *calc_text(const calc_t *c)
{
    return c->error ? "Error" : c->display;
}

/*
 * Layout: the display takes the top sixth of the client, the keypad the
 * rest.  Everything scales with the window.
 */

static twin_rect_t client_rect(const calc_t *c)
{
    twin_rect_t r = twin_window_client(c->window);
    return (twin_rect_t) {0, r.right - r.left, 0, r.bottom - r.top};
}

static twin_rect_t display_rect(const calc_t *c)
{
    twin_rect_t r = client_rect(c);
    r.bottom = r.bottom / 6;
    return r;
}

static int key_gap(twin_coord_t kw, twin_coord_t kh)
{
    return (kw < kh ? kw : kh) / 16 + 1;
}

static twin_rect_t key_rect(const calc_t *c, int i)
{
    twin_rect_t r = client_rect(c);
    twin_coord_t top = display_rect(c).bottom;
    twin_coord_t kw = r.right / COLS, kh = (r.bottom - top) / ROWS;
    twin_coord_t gap = key_gap(kw, kh);
    twin_coord_t left = keys[i].col * kw + gap;
    top += keys[i].row * kh + gap;
    return (twin_rect_t) {left, left + keys[i].span * kw - 2 * gap, top,
                          top + kh - 2 * gap};
}

static int key_at(const calc_t *c, twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < N_KEYS; i++) {
        twin_rect_t r = key_rect(c, i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return i;
    }
    return -1;
}

static void damage(const calc_t *c, twin_rect_t r)
{
    twin_rect_t o = twin_window_client(c->window);
    twin_window_damage(c->window, r.left + o.left, r.top + o.top,
                       r.right + o.left, r.bottom + o.top);
}

/*
 * Painting
 */

static bool overlaps(twin_rect_t a, twin_rect_t b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom &&
           b.top < a.bottom;
}

/* @s in @font at the largest size up to @size that fits @r, baseline
 * centered; @align is -1 left, 0 centered, 1 right */
static void draw_text(twin_pixmap_t *px,
                      twin_font_t *font,
                      twin_rect_t r,
                      int size,
                      int align,
                      twin_argb32_t color,
                      const char *s)
{
    twin_fixed_t room = twin_int_to_fixed(r.right - r.left);
    twin_fixed_t w = twin_text_width(font, twin_int_to_fixed(size), s);
    /* Widths scale with the size: jump near the fit, then step down */
    if (w > room)
        size = (int) ((int64_t) size * room / w);
    if (size < 8)
        size = 8;
    while ((w = twin_text_width(font, twin_int_to_fixed(size), s)) > room &&
           size > 8)
        size--;
    twin_fixed_t x = twin_int_to_fixed(r.left);
    if (align == 0)
        x += (room - w) / 2;
    else if (align > 0)
        x += room - w;
    /* Center the cap height (~0.7 em) rather than the full line box */
    twin_coord_t y = r.top + (r.bottom - r.top + size * 7 / 10 + 1) / 2;
    twin_text_draw(px, font, twin_int_to_fixed(size), x, twin_int_to_fixed(y),
                   color, s);
}

static void calc_paint(const calc_t *c,
                       twin_pixmap_t *px,
                       const calc_palette_t *pal)
{
    twin_rect_t all = client_rect(c), clip = twin_pixmap_get_clip(px);
    twin_fill(px, pal->bg, TWIN_SOURCE, all.left, all.top, all.right,
              all.bottom);

    twin_rect_t d = display_rect(c);
    if (overlaps(d, clip)) {
        twin_coord_t pad = (d.bottom - d.top) / 6 + 2;
        twin_rect_t text = {d.left + pad, d.right - pad, d.top, d.bottom};
        draw_text(px, FONT, text, (d.bottom - d.top) * 3 / 5, 1, pal->text,
                  calc_text(c));
        if (c->pending) {
            char op[2] = {c->pending == '*' ? 'x' : c->pending, 0};
            text.bottom = d.top + (d.bottom - d.top) / 2;
            draw_text(px, FONT, text, (d.bottom - d.top) / 3, -1, pal->accent,
                      op);
        }
    }

    twin_path_t *path = c->path;
    for (int i = 0; i < N_KEYS; i++) {
        twin_rect_t r = key_rect(c, i);
        if (!overlaps(r, clip))
            continue;
        bool down = i == c->pressed;
        twin_argb32_t bg = down ? pal->down : pal->key[keys[i].kind];
        twin_argb32_t fg = down ? pal->down_text : pal->key_text[keys[i].kind];
        twin_coord_t w = r.right - r.left, h = r.bottom - r.top;
        /* Round keys, pill-shaped where a key spans two cells */
        twin_fixed_t radius = twin_int_to_fixed((w < h ? w : h) / 2);
        if (path) {
            twin_path_rounded_rectangle(
                path, twin_int_to_fixed(r.left), twin_int_to_fixed(r.top),
                twin_int_to_fixed(w), twin_int_to_fixed(h), radius, radius);
            twin_paint_path(px, bg, path);
            twin_path_empty(path);
        } else {
            twin_fill(px, bg, TWIN_SOURCE, r.left, r.top, r.right, r.bottom);
        }
        bool erase_key = !strcmp(keys[i].label, ERASE);
        draw_text(px, erase_key ? ERASE_FONT : FONT, r, h * 2 / 5, 0, fg,
                  keys[i].label);
    }
}

/*
 * Input, the same for mouse and touch
 */

static void set_pressed(calc_t *c, int k)
{
    if (c->pressed == k)
        return;
    if (c->pressed >= 0)
        damage(c, key_rect(c, c->pressed));
    c->pressed = k;
    if (k >= 0)
        damage(c, key_rect(c, k));
}

static void press(calc_t *c, int k)
{
    press_key(c, k);
    damage(c, display_rect(c));
}

/* Finger or button released at (@x, @y): press the key under it */
static void release(calc_t *c, twin_coord_t x, twin_coord_t y)
{
    int k = key_at(c, x, y);
    set_pressed(c, -1);
    if (k >= 0)
        press(c, k);
}

static bool type(calc_t *c, int ch)
{
    int k = key_for(ch);
    if (k < 0)
        return false;
    press(c, k);
    return true;
}

static void calc_init(calc_t *c, twin_window_t *window)
{
    c->window = window;
    c->path = twin_path_create();
    c->pressed = -1;
    reset(c);
}

static void calc_fini(calc_t *c)
{
    if (c->path)
        twin_path_destroy(c->path);
}

/*
 * Desktop window
 */

/* clang-format off */
static const calc_palette_t desktop_palette = {
    .bg = 0xff1c1c1e, .text = 0xffffffff, .accent = 0xffff9f0a,
    .key = {[DIGIT] = 0xff3a3a3c, [FUNCTION] = 0xffa5a5a5,
            [OPERATOR] = 0xffff9f0a, [EQUALS] = 0xffff9f0a},
    .key_text = {[DIGIT] = 0xffffffff, [FUNCTION] = 0xff1c1c1e,
                 [OPERATOR] = 0xffffffff, [EQUALS] = 0xffffffff},
    .down = 0xff737376, .down_text = 0xffffffff,
};
/* clang-format on */

static void desktop_draw(twin_window_t *window)
{
    calc_paint(twin_window_client_data(window), twin_window_pixmap(window),
               &desktop_palette);
}

static bool desktop_event(twin_window_t *window, twin_event_t *ev)
{
    calc_t *c = twin_window_client_data(window);
    switch (ev->kind) {
    case TwinEventButtonDown:
        set_pressed(c, key_at(c, ev->u.pointer.x, ev->u.pointer.y));
        return true;
    case TwinEventMotion:
        if (c->pressed >= 0)
            set_pressed(c, key_at(c, ev->u.pointer.x, ev->u.pointer.y));
        return true;
    case TwinEventButtonUp:
        release(c, ev->u.pointer.x, ev->u.pointer.y);
        return true;
    case TwinEventUcs4:
        return type(c, (int) ev->u.ucs4.ucs4);
    default:
        return false;
    }
}

static void desktop_destroy(twin_window_t *window)
{
    calc_t *c = twin_window_client_data(window);
    calc_fini(c);
    free(c);
}

bool apps_calc_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    calc_t *c = malloc(sizeof(*c));
    if (!c)
        return false;
    twin_window_t *window = twin_window_create(
        screen, TWIN_ARGB32, TwinWindowApplication, x, y, w, h);
    if (!window) {
        free(c);
        return false;
    }
    calc_init(c, window);
    twin_window_set_opaque(window, true);
    twin_window_set_name(window, name);
    twin_window_set_client_data(window, c);
    twin_window_set_callbacks(window, desktop_draw, desktop_event,
                              desktop_destroy);
    damage(c, client_rect(c));
    twin_window_show(window);
    return true;
}

/*
 * Phone view: the same calculator in the UI kit's theme
 */

#if defined(CONFIG_DEMO_CELLPHONE)
typedef struct {
    ui_view_t view;
    calc_t calc;
} phone_calc_t;

static void phone_paint(ui_view_t *v, twin_pixmap_t *px)
{
    const phone_theme_t *th = phone_theme();
    twin_argb32_t op = ui_mix(th->indicator, th->primary, 90);
    const calc_palette_t pal = {
        .bg = th->card,
        .text = th->text,
        .accent = th->primary,
        .key = {[DIGIT] = th->indicator,
                [FUNCTION] = th->indicator,
                [OPERATOR] = op,
                [EQUALS] = th->primary},
        .key_text = {[DIGIT] = th->text,
                     [FUNCTION] = th->text,
                     [OPERATOR] = th->text,
                     [EQUALS] = th->on_primary},
        .down = th->primary_dk,
        .down_text = th->on_primary,
    };
    calc_paint(&((phone_calc_t *) v)->calc, px, &pal);
}

static bool phone_event(ui_view_t *v, const ui_event_t *e)
{
    calc_t *c = &((phone_calc_t *) v)->calc;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        set_pressed(c, key_at(c, e->x, e->y));
        return true;
    case UI_TAP:
        release(c, e->x, e->y);
        return true;
    case UI_KEY: {
        int ch = ui_key_ascii(e);
        return ch >= 0 && type(c, ch);
    }
    default:
        set_pressed(c, -1);
        return false;
    }
}

static void phone_destroy(ui_view_t *v)
{
    calc_fini(&((phone_calc_t *) v)->calc);
}

void app_calc_open(void)
{
    phone_calc_t *p = ui_view_new(sizeof(*p), "Calculator", phone_paint,
                                  phone_event, phone_destroy);
    if (!p)
        return;
    calc_init(&p->calc, p->view.window);
    ui_view_show(&p->view);
}

/* Test hook: feed typed keys ("12+3=") to the calculator on top; '~'
 * stands for the sign key, which no character types */
const char *app_calc_eval(const char *typed)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != phone_paint)
        return NULL;
    calc_t *c = &((phone_calc_t *) v)->calc;
    for (const char *p = typed; *p; p++) {
        if (*p == '~')
            press(c, key_named("+/-"));
        else
            type(c, (unsigned char) *p);
    }
    return calc_text(c);
}
#endif
