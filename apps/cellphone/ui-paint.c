/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone UI kit: colors, text, shapes and the widgets drawn from them.
 */

#include <string.h>

#include "ui-internal.h"

/*
 * Colors
 */
twin_argb32_t ui_mix(twin_argb32_t a, twin_argb32_t b, int t)
{
    twin_argb32_t out = 0;
    if (t <= 0)
        return a;
    if (t >= 255)
        return b;
    for (int s = 0; s < 32; s += 8) {
        int ca = (a >> s) & 0xff, cb = (b >> s) & 0xff;
        out |= (twin_argb32_t) ((ca + ((cb - ca) * t + 127) / 255) & 0xff) << s;
    }
    return out;
}

twin_argb32_t ui_darken(twin_argb32_t c, int pct)
{
    return ui_mix(c, c & 0xff000000, pct * 255 / 100);
}

twin_argb32_t ui_alpha(twin_argb32_t c, twin_a8_t a)
{
    twin_argb32_t out = 0;
    for (int s = 0; s < 32; s += 8)
        out |= (twin_argb32_t) ((((c >> s) & 0xff) * a + 127) / 255) << s;
    return out;
}

bool ui_intersects(ui_rect_t a, ui_rect_t b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom &&
           b.top < a.bottom;
}

/*
 * True when [l, r) x [t, b), grown by 1 px for anti-aliasing, misses the
 * clip: the shape would draw nothing, so skip building its path.
 */
static bool _culled(twin_pixmap_t *px,
                    twin_coord_t l,
                    twin_coord_t t,
                    twin_coord_t r,
                    twin_coord_t b)
{
    twin_rect_t clip = twin_pixmap_get_clip(px);
    return r + 1 <= clip.left || l - 1 >= clip.right || b + 1 <= clip.top ||
           t - 1 >= clip.bottom;
}

/* A line of @size text with its baseline at @y misses the clip vertically.
 * The band is wider than twin_text_draw()'s own test, so skipping here never
 * changes what is drawn. */
static bool _text_culled(twin_pixmap_t *px, int size, twin_coord_t y)
{
    twin_rect_t clip = twin_pixmap_get_clip(px);
    return y + size + 2 <= clip.top || y - 2 * size - 2 >= clip.bottom;
}

/*
 * Typography
 */
twin_coord_t ui_text_width(int size, const char *s)
{
    return twin_fixed_to_int(
        twin_text_width(&twin_font_montserrat, ui_fx(size), s) +
        TWIN_FIXED_HALF);
}

void ui_text(twin_pixmap_t *px,
             int size,
             twin_coord_t x,
             twin_coord_t y,
             ui_align_t align,
             twin_argb32_t color,
             const char *s)
{
    if (_text_culled(px, size, y))
        return;
    twin_fixed_t left = ui_fx(x);
    if (align != UI_LEFT) {
        twin_fixed_t w = twin_text_width(&twin_font_montserrat, ui_fx(size), s);
        left -= align == UI_CENTER ? w / 2 : w;
    }
    twin_text_draw(px, &twin_font_montserrat, ui_fx(size), left, ui_fx(y),
                   color, s);
}

int ui_text_wrap(twin_pixmap_t *px,
                 int size,
                 twin_coord_t x,
                 twin_coord_t y,
                 twin_coord_t width,
                 twin_coord_t line_h,
                 twin_argb32_t color,
                 const char *s,
                 twin_coord_t *widest)
{
    char line[128];
    int lines = 0;
    twin_coord_t wmax = 0;
    while (*s) {
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        /* Greedily take words while they fit */
        size_t n = 0, fit = 0;
        while (n < sizeof(line) - 1 && s[n]) {
            size_t end = n;
            while (end < sizeof(line) - 1 && s[end] && s[end] != ' ')
                end++;
            memcpy(line, s, end);
            line[end] = 0;
            if (fit && ui_text_width(size, line) > width)
                break;
            fit = end;
            n = end;
            if (s[n] == ' ')
                n++;
        }
        if (!fit) /* a single word wider than the line: hard break */
            fit = n ? n : 1;
        memcpy(line, s, fit);
        line[fit] = 0;
        twin_coord_t w = ui_text_width(size, line);
        if (w > wmax)
            wmax = w;
        if (px)
            ui_text(px, size, x, y + lines * line_h, UI_LEFT, color, line);
        lines++;
        s += fit;
    }
    if (widest)
        *widest = wmax;
    return lines;
}

twin_coord_t ui_baseline(int size, twin_coord_t top, twin_coord_t h)
{
    /* Center the cap height (~0.7 em) rather than the full line box */
    return top + (h + size * 7 / 10 + 1) / 2;
}

void ui_text_in(twin_pixmap_t *px,
                int size,
                ui_rect_t r,
                ui_align_t align,
                twin_argb32_t color,
                const char *s)
{
    twin_coord_t x = align == UI_LEFT    ? r.left + 4
                     : align == UI_RIGHT ? r.right - 4
                                         : (r.left + r.right) / 2;
    ui_text(px, size, x, ui_baseline(size, r.top, ui_h(r)), align, color, s);
}

void ui_text_fit(twin_pixmap_t *px,
                 int size,
                 ui_rect_t r,
                 ui_align_t align,
                 twin_argb32_t color,
                 const char *s)
{
    if (!*s)
        return; /* also keeps buf below from being drawn uninitialized */
    twin_coord_t room = ui_w(r) - 8;
    if (ui_text_width(size, s) <= room) {
        ui_text_in(px, size, r, align, color, s);
        return;
    }
    char buf[96];
    size_t n = strlen(s);
    if (n > sizeof(buf) - 4)
        n = sizeof(buf) - 4;
    while (n > 0) {
        /* Never cut a UTF-8 sequence: the partial glyph would end the
         * string early and drop the ellipsis */
        while (n > 0 && ((unsigned char) s[n] & 0xc0) == 0x80)
            n--;
        if (!n)
            break;
        size_t m = n;
        while (m > 0 && s[m - 1] == ' ')
            m--; /* "how about..." rather than "how about ..." */
        memcpy(buf, s, m);
        strcpy(buf + m, "...");
        if (ui_text_width(size, buf) <= room)
            break;
        n--;
    }
    ui_text_in(px, size, r, align, color, buf);
}

void ui_icon(twin_pixmap_t *px,
             const char *sym,
             int size,
             twin_coord_t cx,
             twin_coord_t cy,
             twin_argb32_t color)
{
    /* Font Awesome glyphs span -0.125 em .. 0.875 em around the baseline */
    if (_text_culled(px, size, cy + size * 3 / 8 + 1))
        return;
    twin_fixed_t w = twin_text_width(&twin_font_icons, ui_fx(size), sym);
    twin_fixed_t base = ui_fx(cy) + ui_fx(size) * 3 / 8;
    twin_text_draw(px, &twin_font_icons, ui_fx(size), ui_fx(cx) - w / 2, base,
                   color, sym);
}

/*
 * Shapes
 */
static twin_path_t *shape_path;
static twin_path_t *user_path;

bool ui_init(void)
{
    if (!shape_path)
        shape_path = twin_path_create();
    if (!user_path)
        user_path = twin_path_create();
    if (shape_path && user_path)
        return true;
    ui_fini();
    return false;
}

void ui_fini(void)
{
    if (shape_path)
        twin_path_destroy(shape_path);
    if (user_path)
        twin_path_destroy(user_path);
    shape_path = user_path = NULL;
}

/* Both paths exist from ui_init() on, so these never fail */
static twin_path_t *_shape(void)
{
    twin_path_empty(shape_path);
    return shape_path;
}

twin_path_t *ui_path(void)
{
    twin_path_empty(user_path);
    return user_path;
}

void ui_path_done(twin_path_t *path)
{
    if (path)
        twin_path_empty(path);
}

void ui_fill(twin_pixmap_t *px, ui_rect_t r, twin_argb32_t color)
{
    twin_fill(px, color, (color >> 24) == 0xff ? TWIN_SOURCE : TWIN_OVER,
              r.left, r.top, r.right, r.bottom);
}

static void _rrect_path(twin_path_t *p, ui_rect_t r, int radius)
{
    int max_r = min(ui_w(r), ui_h(r)) / 2;
    if (radius > max_r)
        radius = max_r;
    if (radius <= 0)
        twin_path_rectangle(p, ui_fx(r.left), ui_fx(r.top), ui_fx(ui_w(r)),
                            ui_fx(ui_h(r)));
    else
        twin_path_rounded_rectangle(p, ui_fx(r.left), ui_fx(r.top),
                                    ui_fx(ui_w(r)), ui_fx(ui_h(r)),
                                    ui_fx(radius), ui_fx(radius));
}

void ui_rrect(twin_pixmap_t *px, ui_rect_t r, int radius, twin_argb32_t color)
{
    if (ui_w(r) <= 0 || ui_h(r) <= 0 ||
        _culled(px, r.left, r.top, r.right, r.bottom))
        return;
    if (radius <= 0) {
        ui_fill(px, r, color);
        return;
    }
    twin_path_t *p = _shape();
    _rrect_path(p, r, radius);
    twin_paint_path(px, color, p);
}

void ui_rrect_grad(twin_pixmap_t *px,
                   ui_rect_t r,
                   int radius,
                   twin_argb32_t top,
                   twin_argb32_t bottom)
{
    if (ui_w(r) <= 0 || ui_h(r) <= 0 ||
        _culled(px, r.left, r.top, r.right, r.bottom))
        return;
    if (top == bottom) {
        ui_rrect(px, r, radius, top);
        return;
    }
    twin_path_t *p = _shape();
    _rrect_path(p, r, radius);
    twin_paint_path_linear(px, p, 0, ui_fx(r.top), top, 0, ui_fx(r.bottom),
                           bottom);
}

void ui_rrect_stroke(twin_pixmap_t *px,
                     ui_rect_t r,
                     int radius,
                     int width,
                     twin_argb32_t color)
{
    if (_culled(px, r.left, r.top, r.right, r.bottom))
        return;
    twin_path_t *p = _shape();
    /* Stroke centered on a rectangle inset by half the pen */
    twin_fixed_t h = ui_fx(width) / 2;
    twin_fixed_t w = ui_fx(ui_w(r)) - 2 * h, hh = ui_fx(ui_h(r)) - 2 * h;
    twin_fixed_t rad = ui_fx(radius) - h;
    if (rad < 0)
        rad = 0;
    twin_path_rounded_rectangle(p, ui_fx(r.left) + h, ui_fx(r.top) + h, w, hh,
                                rad, rad);
    twin_paint_stroke(px, color, p, ui_fx(width));
}

void ui_circle(twin_pixmap_t *px,
               twin_coord_t cx,
               twin_coord_t cy,
               int radius,
               twin_argb32_t color)
{
    if (_culled(px, cx - radius, cy - radius, cx + radius, cy + radius))
        return;
    twin_path_t *p = _shape();
    twin_path_circle(p, ui_fx(cx), ui_fx(cy), ui_fx(radius));
    twin_paint_path(px, color, p);
}

void ui_ring(twin_pixmap_t *px,
             twin_coord_t cx,
             twin_coord_t cy,
             int radius,
             int width,
             twin_argb32_t color)
{
    if (_culled(px, cx - radius, cy - radius, cx + radius, cy + radius))
        return;
    twin_path_t *p = _shape();
    twin_path_circle(p, ui_fx(cx), ui_fx(cy), ui_fx(radius) - ui_fx(width) / 2);
    twin_paint_stroke(px, color, p, ui_fx(width));
}

void ui_line(twin_pixmap_t *px,
             twin_coord_t x0,
             twin_coord_t y0,
             twin_coord_t x1,
             twin_coord_t y1,
             int width,
             twin_argb32_t color)
{
    if (_culled(px, min(x0, x1) - width, min(y0, y1) - width,
                max(x0, x1) + width, max(y0, y1) + width))
        return;
    twin_path_t *p = _shape();
    twin_path_move(p, ui_fx(x0), ui_fx(y0));
    twin_path_draw(p, ui_fx(x1), ui_fx(y1));
    twin_paint_stroke(px, color, p, ui_fx(width));
}

void ui_vgrad(twin_pixmap_t *px,
              ui_rect_t r,
              twin_argb32_t top,
              twin_argb32_t bottom)
{
    twin_coord_t h = ui_h(r);
    if (h <= 0)
        return;
    if (top == bottom) {
        ui_fill(px, r, top);
        return;
    }
    /* One solid span per row; rows outside the clip cost nothing */
    twin_rect_t clip = twin_pixmap_get_clip(px);
    twin_coord_t y0 = max(r.top, clip.top), y1 = min(r.bottom, clip.bottom);
    for (twin_coord_t y = y0; y < y1; y++) {
        int t = h > 1 ? (y - r.top) * 255 / (h - 1) : 0;
        twin_argb32_t c = ui_mix(top, bottom, t);
        twin_fill(px, c, (c >> 24) == 0xff ? TWIN_SOURCE : TWIN_OVER, r.left, y,
                  r.right, y + 1);
    }
}

/*
 * Widgets
 */
void ui_header(twin_pixmap_t *px,
               ui_rect_t r,
               const char *title,
               const char *trailing)
{
    const phone_theme_t *th = phone_theme();
    ui_vgrad(px, r, th->card, ui_mix(th->card, th->bg, 96));
    ui_text_in(px, FONT_HEADING,
               ui_rect(r.left + 6, r.top, ui_w(r) - 40, ui_h(r)), UI_LEFT,
               th->text, title);
    if (trailing) {
        ui_rect_t t = ui_rect(r.right - 40, r.top, 40, ui_h(r));
        if (_is_symbol(trailing))
            ui_icon(px, trailing, FONT_HEADING, (t.left + t.right) / 2,
                    (t.top + t.bottom) / 2, th->primary);
        else
            ui_text_in(px, FONT_SM,
                       ui_rect(r.left, r.top, ui_w(r) - 4, ui_h(r)), UI_RIGHT,
                       th->primary, trailing);
    }
    ui_separator(px, r);
}

void ui_section(twin_pixmap_t *px, ui_rect_t r, const char *label)
{
    const phone_theme_t *th = phone_theme();
    ui_fill(px, r, ui_mix(th->bg, th->text_sec, 40));
    ui_text_in(px, FONT_SM, ui_rect(r.left + 6, r.top, ui_w(r) - 6, ui_h(r)),
               UI_LEFT, th->text_sec, label);
}

static void _button_colors(ui_button_style_t style,
                           bool pressed,
                           twin_argb32_t *bg,
                           twin_argb32_t *fg)
{
    const phone_theme_t *th = phone_theme();
    switch (style) {
    case UI_BTN_PRIMARY:
        *bg = th->primary;
        *fg = th->on_primary;
        break;
    case UI_BTN_GREEN:
        *bg = th->call_green;
        *fg = 0xffffffff;
        break;
    case UI_BTN_RED:
        *bg = th->call_red;
        *fg = 0xffffffff;
        break;
    case UI_BTN_FLAT:
        *bg = pressed ? ui_mix(th->bg, th->primary, 70) : 0;
        *fg = th->text;
        return;
    case UI_BTN_CARD:
    default:
        *bg = th->card;
        *fg = th->text;
        break;
    }
    if (pressed)
        *bg = ui_darken(*bg, 18);
}

void ui_button(twin_pixmap_t *px,
               ui_rect_t r,
               int radius,
               ui_button_style_t style,
               bool pressed,
               int size,
               const char *label)
{
    twin_argb32_t bg, fg;
    _button_colors(style, pressed, &bg, &fg);
    if (bg)
        ui_rrect(px, r, radius, bg);
    if (!label)
        return;
    if (_is_symbol(label))
        ui_icon(px, label, size, (r.left + r.right) / 2, (r.top + r.bottom) / 2,
                fg);
    else
        ui_text_in(px, size, r, UI_CENTER, fg, label);
}

static const twin_argb32_t avatar_palette[] = {
    0xff3b82f6, 0xff10b981, 0xfff59e0b, 0xffec4899,
    0xff8b5cf6, 0xff14b8a6, 0xffef4444, 0xff6366f1,
};

twin_argb32_t ui_avatar_color(const char *name)
{
    /* Keyed by initial: neighbors in an alphabetical list always differ */
    const unsigned n = sizeof(avatar_palette) / sizeof(avatar_palette[0]);
    unsigned c = name ? (unsigned char) name[0] : 0;
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z')
        return avatar_palette[(c - 'A') % n];
    unsigned h = 0x811c9dc5u;
    while (name && *name)
        h = (h ^ (unsigned char) *name++) * 0x01000193u;
    return avatar_palette[(h ^ (h >> 16)) % n];
}

void ui_avatar(twin_pixmap_t *px,
               twin_coord_t cx,
               twin_coord_t cy,
               int radius,
               char initial,
               twin_argb32_t color)
{
    char s[2] = {initial, 0};
    ui_circle(px, cx, cy, radius, color);
    int size = radius >= 20 ? FONT_LARGE : FONT_NORMAL;
    ui_text(px, size, cx, ui_baseline(size, cy - radius, radius * 2), UI_CENTER,
            0xffffffff, s);
}

void ui_list_row(twin_pixmap_t *px,
                 ui_rect_t r,
                 bool pressed,
                 const char *sym,
                 twin_argb32_t sym_color,
                 const char *title,
                 const char *subtitle,
                 const char *detail)
{
    const phone_theme_t *th = phone_theme();
    ui_fill(px, r, pressed ? ui_mix(th->card, th->primary, 70) : th->card);
    twin_coord_t x = r.left + 10;
    if (sym) {
        twin_coord_t cy = (r.top + r.bottom) / 2;
        if (_is_symbol(sym)) {
            ui_icon(px, sym, FONT_HEADING, x + 10, cy, sym_color);
        } else {
            ui_avatar(px, x + 12, cy, 13, sym[0], sym_color);
        }
        x += 32;
    }
    twin_coord_t detail_w = detail ? ui_text_width(FONT_SM, detail) + 12 : 0;
    ui_rect_t text = {x, (twin_coord_t) (r.right - detail_w - 4), r.top,
                      r.bottom};
    if (subtitle) {
        ui_rect_t top = {text.left, text.right, r.top + 3,
                         (twin_coord_t) (r.top + ui_h(r) / 2 + 1)};
        ui_rect_t bot = {text.left, text.right,
                         (twin_coord_t) (r.top + ui_h(r) / 2 - 1),
                         r.bottom - 3};
        top.left -= 4;
        bot.left -= 4;
        ui_text_fit(px, FONT_NORMAL, top, UI_LEFT, th->text, title);
        ui_text_fit(px, FONT_SM, bot, UI_LEFT, th->text_sec, subtitle);
    } else {
        text.left -= 4;
        ui_text_fit(px, FONT_NORMAL, text, UI_LEFT, th->text, title);
    }
    if (detail)
        ui_text_in(px, FONT_SM, ui_rect(r.left, r.top, ui_w(r) - 6, ui_h(r)),
                   UI_RIGHT, th->text_sec, detail);
    ui_separator(px, ui_rect(x, r.top, r.right - x, ui_h(r)));
}

twin_rect_t ui_clip_push(twin_pixmap_t *px, ui_rect_t r)
{
    twin_rect_t saved = twin_pixmap_save_clip(px);
    twin_pixmap_clip(px, r.left, r.top, r.right, r.bottom);
    return saved;
}

void ui_separator(twin_pixmap_t *px, ui_rect_t r)
{
    ui_fill(px, ui_rect(r.left, r.bottom - 1, ui_w(r), 1),
            phone_theme()->separator);
}

void ui_slider(twin_pixmap_t *px, ui_rect_t r, int value, bool pressed)
{
    const phone_theme_t *th = phone_theme();
    twin_coord_t cy = (r.top + r.bottom) / 2;
    twin_coord_t x0 = r.left + 8, x1 = r.right - 8;
    twin_coord_t kx = ui_slider_knob_x(r, value);
    ui_rrect(px, ui_rect(x0, cy - 2, x1 - x0, 4), 2, th->indicator);
    ui_rrect(px, ui_rect(x0, cy - 2, kx - x0, 4), 2, th->primary);
    int kr = pressed ? 9 : 7;
    ui_circle(px, kx, cy + 1, kr + 1, 0x30000000);
    ui_circle(px, kx, cy, kr, 0xffffffff);
    ui_ring(px, kx, cy, kr, 1, ui_alpha(th->primary, 0xc0));
}

twin_coord_t ui_slider_knob_x(ui_rect_t r, int value)
{
    twin_coord_t x0 = r.left + 8, x1 = r.right - 8;
    return x0 + (x1 - x0) * value / 1000;
}

int ui_slider_value_at(ui_rect_t r, twin_coord_t x)
{
    twin_coord_t x0 = r.left + 8, x1 = r.right - 8;
    if (x <= x0)
        return 0;
    if (x >= x1)
        return 1000;
    return (x - x0) * 1000 / (x1 - x0);
}

void ui_switch(twin_pixmap_t *px, ui_rect_t r, bool on)
{
    const phone_theme_t *th = phone_theme();
    ui_rect_t track =
        ui_rect(r.right - 38, (r.top + r.bottom) / 2 - 10, 36, 20);
    ui_rrect(px, track, 10, on ? th->primary : th->indicator);
    twin_coord_t kx = on ? track.right - 10 : track.left + 10;
    ui_circle(px, kx, (track.top + track.bottom) / 2, 8, 0xffffffff);
}

void ui_dots(twin_pixmap_t *px,
             twin_coord_t cx,
             twin_coord_t cy,
             int count,
             int current)
{
    const phone_theme_t *th = phone_theme();
    int total = count * 10 + 8;
    twin_coord_t x = cx - total / 2;
    for (int i = 0; i < count; i++) {
        if (i == current) {
            ui_rrect(px, ui_rect(x, cy - 3, 14, 6), 3, th->primary);
            x += 18;
        } else {
            ui_circle(px, x + 3, cy, 3, th->indicator);
            x += 10;
        }
    }
}
