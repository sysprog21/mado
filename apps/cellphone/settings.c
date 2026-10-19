/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Settings: a root list and one sub-screen per category.
 *
 * Every screen is the same kind of view driven by a small item table built
 * when the screen opens (sections, links, radios, switches, sliders, info
 * rows, notes and the wallpaper grid).  Layout is recomputed from the view
 * size on every paint and event, so the screen follows the window manager
 * when it is resized (the 'none' scene gives views the whole display).
 *
 * Only user preferences outlive a screen; they sit in one static struct so
 * that sliders and switches keep their values across visits.
 */

#include <stdio.h>
#include <string.h>

#include "phone.h"

#define MAX_ITEMS 20
#define VAL_LEN 16
#define HEADER_H PHONE_HEADER_H
#define ROW_H PHONE_LIST_ROW_H
#define RADIO_SUB_H 46
#define SLIDER_H 52
#define INFO_H 30
#define CLOCK_H 86
#define PLATE 26
#define ICON_COL 40 /* icon column on the grouped surface */
#define TILE_GAP 12
#define TILE_LABEL_H 20
#define TILE_R 8

/* Preferences shared by every visit to Settings (visual only) */
static struct {
    int brightness; /* 0..1000 */
    int volume;     /* 0..1000 */
    bool vibrate;
    bool keypad_tones;
    bool clock_24h;
} prefs = {700, 500, true, false, true};

enum { PREF_BRIGHTNESS, PREF_VOLUME, PREF_VIBRATE, PREF_KEYPAD, PREF_24H };

typedef enum {
    PAGE_ROOT,
    PAGE_DISPLAY,
    PAGE_SOUND,
    PAGE_WALLPAPER,
    PAGE_TIME,
    PAGE_ABOUT,
} page_id_t;

static const char *const page_titles[] = {
    "Settings", "Display", "Sound", "Wallpaper", "Date & time", "About",
};

typedef enum {
    IT_SECTION,
    IT_LINK,       /* id: page */
    IT_RADIO,      /* arg: theme */
    IT_SWITCH,     /* id: pref */
    IT_SLIDER,     /* id: pref */
    IT_INFO,       /* id: value */
    IT_NOTE,       /* sub: wrapped text */
    IT_WALLPAPERS, /* grid of wallpaper tiles */
    IT_CLOCK,      /* large time and date */
} item_kind_t;

/* Info values; those from VAL_HEAP on are refreshed once per second */
enum {
    VAL_NAME,
    VAL_SCREEN,
    VAL_TZ,
    VAL_HEAP,
    VAL_PEAK,
    VAL_HITS,
    VAL_MISSES,
    VAL_RATE,
    VAL_BYTES,
    VAL_FRAMES,
    VAL_PIXELS,
};

typedef struct {
    item_kind_t kind;
    int id, arg;
    const char *sym;
    const char *label;
    const char *sub;
} item_t;

typedef struct {
    ui_view_t view;
    page_id_t page;
    item_t items[MAX_ITEMS];
    int n;
    /* Layout in content coordinates, refreshed from the view size */
    twin_coord_t y[MAX_ITEMS], h[MAX_ITEMS];
    ui_scroll_t scroll;
    int pressed;                   /* item under the finger, -1 when none */
    int pressed_tile;              /* wallpaper tile under the finger */
    int grab;                      /* slider item being dragged, -1 when none */
    int minute;                    /* last minute shown by the clock */
    int refreshes;                 /* About: value refresh ticks (tests) */
    char vals[MAX_ITEMS][VAL_LEN]; /* About: live values last painted */
} page_t;

static void settings_paint(ui_view_t *v, twin_pixmap_t *px);
static void open_page(page_id_t page);

static int *pref_int(int id)
{
    return id == PREF_BRIGHTNESS ? &prefs.brightness : &prefs.volume;
}

static bool *pref_bool(int id)
{
    switch (id) {
    case PREF_VIBRATE:
        return &prefs.vibrate;
    case PREF_KEYPAD:
        return &prefs.keypad_tones;
    default:
        return &prefs.clock_24h;
    }
}

/*
 * Formatting (integers only)
 */
static void fmt_bytes(char *buf, size_t len, uint64_t b)
{
    if (b < 1024)
        snprintf(buf, len, "%u B", (unsigned) b);
    else if (b < 1024 * 1024)
        snprintf(buf, len, "%u.%u KB", (unsigned) (b / 1024),
                 (unsigned) (b * 10 / 1024 % 10));
    else
        snprintf(buf, len, "%u.%u MB", (unsigned) (b >> 20),
                 (unsigned) ((b * 10 >> 20) % 10));
}

static void fmt_count(char *buf, size_t len, uint64_t n)
{
    if (n < 100000)
        snprintf(buf, len, "%u", (unsigned) n);
    else if (n < 10000000)
        snprintf(buf, len, "%u.%uK", (unsigned) (n / 1000),
                 (unsigned) (n / 100 % 10));
    else
        snprintf(buf, len, "%u.%uM", (unsigned) (n / 1000000),
                 (unsigned) (n / 100000 % 10));
}

static void format_value(int id, char *buf, size_t len)
{
    twin_memory_info_t mem;
    twin_cache_stats_t cache;
    twin_screen_t *screen = phone_screen();

    switch (id) {
    case VAL_NAME:
        snprintf(buf, len, "Mado Phone");
        return;
    case VAL_SCREEN:
        snprintf(buf, len, "%dx%d", PHONE_W, PHONE_H);
        return;
    case VAL_TZ:
        snprintf(buf, len, "GMT+8 Taipei");
        return;
    case VAL_HEAP:
    case VAL_PEAK:
        twin_memory_get_info(&mem);
        if (!mem.peak_bytes) /* CONFIG_MEMORY_STATS is off */
            snprintf(buf, len, "n/a");
        else
            fmt_bytes(buf, len,
                      id == VAL_HEAP ? mem.current_bytes : mem.peak_bytes);
        return;
    case VAL_HITS:
    case VAL_MISSES:
    case VAL_RATE:
    case VAL_BYTES:
        twin_cache_get_stats(&cache);
        if (id == VAL_HITS) {
            fmt_count(buf, len, cache.hits);
        } else if (id == VAL_MISSES) {
            fmt_count(buf, len, cache.misses);
        } else if (id == VAL_BYTES) {
            fmt_bytes(buf, len, cache.bytes);
        } else {
            uint64_t total = (uint64_t) cache.hits + cache.misses;
            unsigned pm = total ? (unsigned) (cache.hits * 1000ULL / total) : 0;
            snprintf(buf, len, "%u.%u%%", pm / 10, pm % 10);
        }
        return;
    case VAL_FRAMES:
        fmt_count(buf, len, screen ? twin_screen_stats(screen)->frames : 0);
        return;
    case VAL_PIXELS:
        fmt_count(buf, len, screen ? twin_screen_stats(screen)->pixels : 0);
        return;
    default:
        buf[0] = 0;
    }
}

static bool is_live(const item_t *it)
{
    return it->kind == IT_INFO && it->id >= VAL_HEAP;
}

/*
 * Item table
 */
static void add(page_t *s,
                item_kind_t kind,
                int id,
                int arg,
                const char *sym,
                const char *label,
                const char *sub)
{
    if (s->n >= MAX_ITEMS)
        return;
    s->items[s->n++] = (item_t) {kind, id, arg, sym, label, sub};
}

static void section(page_t *s, const char *label)
{
    add(s, IT_SECTION, 0, 0, NULL, label, NULL);
}

static void info(page_t *s, const char *label, int id)
{
    add(s, IT_INFO, id, 0, NULL, label, NULL);
}

static void build(page_t *s)
{
    switch (s->page) {
    case PAGE_ROOT:
        section(s, "Device");
        add(s, IT_LINK, PAGE_DISPLAY, 0, TWIN_SYM_SUN, "Display", NULL);
        add(s, IT_LINK, PAGE_SOUND, 0, TWIN_SYM_VOLUME, "Sound", NULL);
        add(s, IT_LINK, PAGE_WALLPAPER, 0, TWIN_SYM_IMAGE, "Wallpaper", NULL);
        section(s, "System");
        add(s, IT_LINK, PAGE_TIME, 0, TWIN_SYM_CLOCK, "Date & time", NULL);
        add(s, IT_LINK, PAGE_ABOUT, 0, TWIN_SYM_LIST, "About", NULL);
        break;
    case PAGE_DISPLAY:
        section(s, "Theme");
        for (int i = 0; i < phone_theme_count(); i++)
            add(s, IT_RADIO, 0, i, NULL, phone_theme_at(i)->name, NULL);
        section(s, "Screen");
        add(s, IT_SLIDER, PREF_BRIGHTNESS, 0, NULL, "Brightness", NULL);
        break;
    case PAGE_SOUND:
        section(s, "Audio");
        add(s, IT_SLIDER, PREF_VOLUME, 0, NULL, "Volume", NULL);
        section(s, "Feedback");
        add(s, IT_SWITCH, PREF_VIBRATE, 0, TWIN_SYM_BELL, "Vibrate", NULL);
        add(s, IT_SWITCH, PREF_KEYPAD, 0, TWIN_SYM_KEYBOARD, "Keypad tones",
            NULL);
        break;
    case PAGE_WALLPAPER:
        add(s, IT_WALLPAPERS, 0, 0, NULL, "Wallpapers", NULL);
        break;
    case PAGE_TIME:
        add(s, IT_CLOCK, 0, 0, NULL, "Clock", NULL);
        section(s, "Format");
        add(s, IT_SWITCH, PREF_24H, 0, TWIN_SYM_CLOCK, "24-hour time", NULL);
        section(s, "Region");
        add(s, IT_INFO, VAL_TZ, 0, TWIN_SYM_GLOBE, "Time zone", NULL);
        break;
    case PAGE_ABOUT:
        section(s, "Device");
        info(s, "Name", VAL_NAME);
        info(s, "Screen", VAL_SCREEN);
        section(s, "Memory");
        info(s, "Heap in use", VAL_HEAP);
        info(s, "Heap peak", VAL_PEAK);
        section(s, "Coverage cache");
        info(s, "Hits", VAL_HITS);
        info(s, "Misses", VAL_MISSES);
        info(s, "Hit rate", VAL_RATE);
        info(s, "Resident", VAL_BYTES);
        section(s, "Compositor");
        info(s, "Frames", VAL_FRAMES);
        info(s, "Pixels", VAL_PIXELS);
        break;
    }
}

/*
 * Layout
 */
static int grid_cols(twin_coord_t w)
{
    int cols = w / 110;
    return cols < 2 ? 2 : cols;
}

static twin_coord_t tile_w(twin_coord_t w)
{
    int cols = grid_cols(w);
    return (w - (cols + 1) * TILE_GAP) / cols;
}

static twin_coord_t tile_h(twin_coord_t w)
{
    return tile_w(w) * 3 / 4;
}

/* Wallpaper tile @i inside a grid item whose top-left is (0, top) */
static ui_rect_t tile_rect(twin_coord_t w, twin_coord_t top, int i)
{
    int cols = grid_cols(w);
    twin_coord_t tw = tile_w(w), th = tile_h(w);
    return ui_rect(TILE_GAP + (i % cols) * (tw + TILE_GAP),
                   top + TILE_GAP + (i / cols) * (th + TILE_LABEL_H + 6), tw,
                   th);
}

static twin_coord_t item_height(const item_t *it, twin_coord_t w)
{
    switch (it->kind) {
    case IT_SECTION:
        return PHONE_SECTION_H;
    case IT_RADIO:
        return it->sub ? RADIO_SUB_H : ROW_H;
    case IT_SLIDER:
        return SLIDER_H;
    case IT_INFO:
        return it->sym ? ROW_H : INFO_H;
    case IT_NOTE:
        return 16 + 15 * ui_text_wrap(NULL, FONT_SM, 0, 0, w - 2 * 12, 15, 0,
                                      it->sub, NULL);
    case IT_WALLPAPERS: {
        int rows = (phone_wallpaper_count() + grid_cols(w) - 1) / grid_cols(w);
        return TILE_GAP + rows * (tile_h(w) + TILE_LABEL_H + 6);
    }
    case IT_CLOCK:
        return CLOCK_H;
    case IT_LINK:
    case IT_SWITCH:
    default:
        return ROW_H;
    }
}

static void layout(page_t *s)
{
    twin_coord_t w = ui_view_width(&s->view), h = ui_view_height(&s->view);
    twin_coord_t y = 0;
    for (int i = 0; i < s->n; i++) {
        s->y[i] = y;
        s->h[i] = item_height(&s->items[i], w);
        y += s->h[i];
    }
    ui_scroll_setup(&s->scroll, ui_rect(0, HEADER_H, w, h - HEADER_H), y);
}

/* Item rectangle in view coordinates (scrolled) */
static ui_rect_t item_rect(page_t *s, int i)
{
    return ui_rect(0, s->scroll.area.top + s->y[i] - s->scroll.offset,
                   ui_w(s->scroll.area), s->h[i]);
}

/* Part of @r inside the scroll viewport: what invalidation should cover */
static ui_rect_t visible(page_t *s, ui_rect_t r)
{
    ui_rect_t a = s->scroll.area;
    if (r.top < a.top)
        r.top = a.top;
    if (r.bottom > a.bottom)
        r.bottom = a.bottom;
    if (r.bottom < r.top)
        r.bottom = r.top;
    return r;
}

static void invalidate_item(page_t *s, int i)
{
    ui_rect_t r = visible(s, item_rect(s, i));
    if (ui_h(r) > 0)
        ui_view_invalidate(&s->view, r);
}

static ui_rect_t slider_rect(ui_rect_t row)
{
    return ui_rect(row.left + 4, row.top + 24, ui_w(row) - 8, 24);
}

static ui_rect_t value_rect(ui_rect_t row)
{
    return ui_rect(row.right - 120, row.top, 120, ui_h(row) - 1);
}

/*
 * Painting
 */

/* Symbolic icon in the row's icon column, on the grouped surface */
static void paint_icon_column(twin_pixmap_t *px, ui_rect_t r, const char *sym)
{
    const phone_theme_t *th = phone_theme();
    twin_coord_t cy = (r.top + r.bottom) / 2;
    ui_fill(px, ui_rect(r.left, r.top, ICON_COL, ui_h(r)), th->group);
    if (sym)
        ui_icon(px, sym, FONT_NORMAL, r.left + ICON_COL / 2, cy, th->text);
}

/* Theme preview: the theme's background with its primary color on it */
static void paint_swatch(twin_pixmap_t *px, twin_coord_t cy, int theme)
{
    const phone_theme_t *t = phone_theme_at(theme);
    ui_rect_t plate =
        ui_rect((ICON_COL - PLATE) / 2, cy - PLATE / 2, PLATE, PLATE);
    ui_rrect(px, plate, 7, phone_theme()->separator);
    ui_rrect_grad(px, ui_inset(plate, 1), 6, t->bg, t->bg_grad);
    ui_rrect(px, ui_rect(plate.left + 5, cy - 5, PLATE - 10, 10), 3, t->card);
    ui_circle(px, plate.left + 9, cy, 3, t->primary);
}

static bool radio_on(const item_t *it)
{
    return phone_theme_index() == it->arg;
}

static void link_detail(const item_t *it, char *buf, size_t len)
{
    buf[0] = 0;
    switch (it->id) {
    case PAGE_DISPLAY:
        snprintf(buf, len, "%s", phone_theme()->name);
        break;
    case PAGE_SOUND:
        snprintf(buf, len, "%d%%", prefs.volume / 10);
        break;
    case PAGE_WALLPAPER:
        snprintf(buf, len, "%s", phone_wallpaper_name(phone_wallpaper()));
        break;
    default:
        break;
    }
}

static void paint_row(page_t *s, twin_pixmap_t *px, int i, ui_rect_t r)
{
    const phone_theme_t *th = phone_theme();
    const item_t *it = &s->items[i];
    twin_coord_t cy = (r.top + r.bottom) / 2;
    twin_coord_t x = 14, right = r.right - 10;
    char buf[24];

    /* Rows with an icon column fill the rest of the row only */
    bool column = it->sym || it->kind == IT_RADIO;
    ui_rect_t body = r;
    if (column)
        body.left += ICON_COL;
    ui_fill(px, body,
            i == s->pressed ? ui_mix(th->card, th->primary, 70) : th->card);
    if (column) {
        paint_icon_column(px, r, it->sym);
        if (it->kind == IT_RADIO)
            paint_swatch(px, cy, it->arg);
        x = ICON_COL + 10;
    }

    switch (it->kind) {
    case IT_LINK:
        ui_icon(px, TWIN_SYM_RIGHT, FONT_SM, r.right - 14, cy, th->text_sec);
        link_detail(it, buf, sizeof(buf));
        right = r.right - 26;
        /* The label wins: drop the detail when both do not fit */
        if (buf[0] && x + ui_text_width(FONT_NORMAL, it->label) + 12 +
                              ui_text_width(FONT_SM, buf) <=
                          right) {
            ui_text_in(px, FONT_SM,
                       ui_rect(r.left, r.top, right - r.left, ui_h(r)),
                       UI_RIGHT, th->text_sec, buf);
            right -= ui_text_width(FONT_SM, buf) + 8;
        }
        break;
    case IT_SWITCH:
        ui_switch(px, ui_rect(r.left, r.top, ui_w(r) - 12, ui_h(r)),
                  *pref_bool(it->id));
        right = r.right - 54;
        break;
    case IT_RADIO:
        if (radio_on(it)) {
            ui_circle(px, r.right - 22, cy, 9, th->primary);
            ui_circle(px, r.right - 22, cy, 4, th->on_primary);
        } else {
            ui_ring(px, r.right - 22, cy, 8, 2, th->indicator);
        }
        right = r.right - 36;
        break;
    case IT_INFO: {
        const char *val = s->vals[i];
        if (!is_live(it)) {
            format_value(it->id, buf, sizeof(buf));
            val = buf;
        }
        ui_text_in(px, FONT_NORMAL,
                   ui_rect(r.left, r.top, ui_w(r) - 8, ui_h(r)), UI_RIGHT,
                   th->text_sec, val);
        right = r.right - 8 - ui_text_width(FONT_NORMAL, val) - 8;
        break;
    }
    default:
        break;
    }

    ui_rect_t text = {(twin_coord_t) (x - 4), right, r.top, r.bottom};
    if (it->sub) {
        ui_rect_t top = {text.left, text.right, (twin_coord_t) (r.top + 3),
                         (twin_coord_t) (cy + 1)};
        ui_rect_t bot = {text.left, text.right, (twin_coord_t) (cy - 1),
                         (twin_coord_t) (r.bottom - 3)};
        ui_text_fit(px, FONT_NORMAL, top, UI_LEFT, th->text, it->label);
        ui_text_fit(px, FONT_SM, bot, UI_LEFT, th->text_sec, it->sub);
    } else {
        ui_text_fit(px, FONT_NORMAL, text, UI_LEFT, th->text, it->label);
    }
    ui_separator(px, ui_rect(x, r.top, r.right - x, ui_h(r)));
}

static void paint_slider(page_t *s, twin_pixmap_t *px, int i, ui_rect_t r)
{
    const phone_theme_t *th = phone_theme();
    const item_t *it = &s->items[i];
    int value = *pref_int(it->id);
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", value / 10);
    ui_fill(px, r, th->card);
    ui_text(px, FONT_NORMAL, 12, r.top + 18, UI_LEFT, th->text, it->label);
    ui_text(px, FONT_SM, r.right - 12, r.top + 18, UI_RIGHT, th->text_sec, pct);
    ui_slider(px, slider_rect(r), value, s->grab == i);
    ui_separator(px, r);
}

static void paint_note(page_t *s, twin_pixmap_t *px, int i, ui_rect_t r)
{
    const phone_theme_t *th = phone_theme();
    ui_fill(px, r, th->bg);
    ui_text_wrap(px, FONT_SM, 12, r.top + 18, ui_w(r) - 24, 15, th->text_sec,
                 s->items[i].sub, NULL);
}

static void paint_clock(twin_pixmap_t *px, ui_rect_t r)
{
    const phone_theme_t *th = phone_theme();
    char t[16], d[32];
    if (prefs.clock_24h) {
        phone_format_time(t, sizeof(t));
    } else {
        phone_clock_t c;
        phone_clock(&c);
        int h12 = c.hour % 12 ? c.hour % 12 : 12;
        snprintf(t, sizeof(t), "%d:%02d %s", h12, c.min,
                 c.hour < 12 ? "AM" : "PM");
    }
    phone_format_date(d, sizeof(d));
    twin_coord_t cx = (r.left + r.right) / 2;
    ui_vgrad(px, r, th->card, ui_mix(th->card, th->bg, 128));
    ui_text(px, FONT_CLOCK, cx, r.top + 46, UI_CENTER, th->text, t);
    ui_text(px, FONT_SM, cx, r.top + 70, UI_CENTER, th->text_sec, d);
    ui_separator(px, r);
}

static void paint_wallpapers(page_t *s,
                             twin_pixmap_t *px,
                             ui_rect_t r,
                             ui_rect_t clip)
{
    const phone_theme_t *th = phone_theme();
    twin_coord_t w = ui_w(r);
    ui_fill(px, r, th->bg);
    for (int i = 0; i < phone_wallpaper_count(); i++) {
        ui_rect_t t = tile_rect(w, r.top, i);
        ui_rect_t cell = ui_rect(t.left - 6, t.top - 6, ui_w(t) + 12,
                                 ui_h(t) + TILE_LABEL_H + 8);
        if (!ui_intersects(cell, clip))
            continue;
        bool sel = i == phone_wallpaper();
        twin_argb32_t label = phone_paint_wallpaper(px, t, i);
        /* Hint of the home grid so the tile reads as a screen */
        twin_coord_t u = ui_w(t) / 9;
        for (int k = 0; k < 6; k++)
            ui_rrect(px,
                     ui_rect(t.left + u + (k % 3) * 3 * u,
                             t.top + u + (k / 3) * 3 * u, u * 2, u * 2),
                     u / 2, ui_alpha(label, 0x50));
        if (i == s->pressed_tile)
            ui_fill(px, t, ui_alpha(UI_RGB(0x000000), 0x30));
        /* Round the corners by painting the background around them */
        ui_rrect_stroke(px, ui_inset(t, -6), TILE_R + 6, 6, th->bg);
        if (sel) {
            ui_rrect_stroke(px, ui_inset(t, -3), TILE_R + 3, 2, th->primary);
            ui_circle(px, t.right - 10, t.top + 10, 7, th->primary);
            ui_icon(px, TWIN_SYM_CHECK, 9, t.right - 10, t.top + 10,
                    th->on_primary);
        } else {
            ui_rrect_stroke(px, t, TILE_R, 1, ui_alpha(th->text, 0x30));
        }
        ui_text_in(
            px, FONT_SM, ui_rect(t.left, t.bottom + 2, ui_w(t), TILE_LABEL_H),
            UI_CENTER, sel ? th->primary : th->text, phone_wallpaper_name(i));
    }
}

static void settings_paint(ui_view_t *v, twin_pixmap_t *px)
{
    page_t *s = (page_t *) v;
    const phone_theme_t *th = phone_theme();
    ui_rect_t clip = ui_view_clip(v);
    layout(s);
    ui_rect_t area = s->scroll.area;

    if (ui_intersects(clip, area)) {
        twin_coord_t bottom = area.top;
        for (int i = 0; i < s->n; i++) {
            ui_rect_t r = item_rect(s, i);
            bottom = r.bottom;
            if (r.bottom <= area.top || r.top >= area.bottom ||
                !ui_intersects(r, clip))
                continue;
            switch (s->items[i].kind) {
            case IT_SECTION:
                ui_section(px, r, s->items[i].label);
                break;
            case IT_SLIDER:
                paint_slider(s, px, i, r);
                break;
            case IT_NOTE:
                paint_note(s, px, i, r);
                break;
            case IT_CLOCK:
                paint_clock(px, r);
                break;
            case IT_WALLPAPERS:
                paint_wallpapers(s, px, r, clip);
                break;
            default:
                paint_row(s, px, i, r);
                break;
            }
        }
        if (bottom < area.bottom)
            ui_fill(px, ui_rect(0, bottom, ui_w(area), area.bottom - bottom),
                    th->bg);
        ui_scroll_bar(px, &s->scroll);
    }
    /* Last, so rows scrolled under the header never show through */
    if (clip.top < HEADER_H)
        ui_header(px, ui_rect(0, 0, ui_view_width(&s->view), HEADER_H), NULL,
                  s->view.title, NULL);
}

/*
 * Behavior
 */

/* Repaint the other Settings screens: their details mirror this change */
static void refresh_others(page_t *self)
{
    if (!phone_screen())
        return;
    for (twin_window_t *w = twin_screen_first_window(phone_screen()); w;
         w = twin_window_next(w)) {
        ui_view_t *o = ui_view_of(w);
        if (o && o != &self->view && o->paint == settings_paint)
            ui_view_invalidate_all(o);
    }
}

static int item_at(page_t *s, twin_coord_t x, twin_coord_t y)
{
    if (!ui_hit(s->scroll.area, x, y))
        return -1;
    for (int i = 0; i < s->n; i++)
        if (ui_hit(item_rect(s, i), x, y))
            return i;
    return -1;
}

static int tile_at(page_t *s, int i, twin_coord_t x, twin_coord_t y)
{
    ui_rect_t r = item_rect(s, i);
    for (int k = 0; k < phone_wallpaper_count(); k++) {
        ui_rect_t t = tile_rect(ui_w(r), r.top, k);
        t.bottom += TILE_LABEL_H;
        if (ui_hit(t, x, y))
            return k;
    }
    return -1;
}

static bool tappable(const item_t *it)
{
    return it->kind == IT_LINK || it->kind == IT_RADIO ||
           it->kind == IT_SWITCH || it->kind == IT_WALLPAPERS;
}

static void set_pressed(page_t *s, int i, int tile)
{
    if (i == s->pressed && tile == s->pressed_tile)
        return;
    if (s->pressed >= 0)
        invalidate_item(s, s->pressed);
    s->pressed = i;
    s->pressed_tile = tile;
    if (i >= 0)
        invalidate_item(s, i);
}

static void slide(page_t *s, twin_coord_t x)
{
    int *val = pref_int(s->items[s->grab].id);
    int nv = ui_slider_value_at(slider_rect(item_rect(s, s->grab)), x);
    if (nv != *val) {
        *val = nv;
        invalidate_item(s, s->grab);
    }
}

static void activate(page_t *s, int i, int tile)
{
    item_t *it = &s->items[i];
    switch (it->kind) {
    case IT_LINK:
        open_page((page_id_t) it->id);
        break;
    case IT_RADIO:
        if (radio_on(it))
            break;
        phone_theme_set(it->arg); /* repaints every window */
        break;
    case IT_SWITCH: {
        bool *b = pref_bool(it->id);
        *b = !*b;
        invalidate_item(s, i);
        /* The 24-hour switch also changes the clock above it */
        if (it->id == PREF_24H)
            for (int k = 0; k < s->n; k++)
                if (s->items[k].kind == IT_CLOCK)
                    invalidate_item(s, k);
        break;
    }
    case IT_WALLPAPERS:
        if (tile >= 0 && tile != phone_wallpaper()) {
            phone_set_wallpaper(tile);
            invalidate_item(s, i);
            refresh_others(s);
        }
        break;
    default:
        break;
    }
}

static bool settings_event(ui_view_t *v, const ui_event_t *e)
{
    page_t *s = (page_t *) v;
    layout(s);

    /* A grabbed slider owns the gesture, whatever its direction */
    if (s->grab >= 0) {
        int g = s->grab;
        if (e->kind == UI_MOVE || e->kind == UI_DRAG) {
            slide(s, e->x);
            return true;
        }
        if (e->kind == UI_RELEASE || e->kind == UI_TAP ||
            e->kind == UI_CANCEL) {
            s->grab = -1;
            invalidate_item(s, g);
            refresh_others(s);
            return true;
        }
    }

    if (ui_scroll_event(v, &s->scroll, e)) {
        set_pressed(s, -1, -1);
        return true;
    }

    switch (e->kind) {
    case UI_PRESS: {
        int i = item_at(s, e->x, e->y);
        if (i >= 0 && s->items[i].kind == IT_SLIDER &&
            ui_hit(slider_rect(item_rect(s, i)), e->x, e->y)) {
            s->grab = i;
            slide(s, e->x);
            invalidate_item(s, i);
            return true;
        }
        if (i >= 0 && tappable(&s->items[i])) {
            int tile = s->items[i].kind == IT_WALLPAPERS
                           ? tile_at(s, i, e->x, e->y)
                           : -1;
            if (s->items[i].kind != IT_WALLPAPERS || tile >= 0)
                set_pressed(s, i, tile);
        }
        return true;
    }
    case UI_TAP: {
        int i = s->pressed, tile = s->pressed_tile;
        set_pressed(s, -1, -1);
        if (i >= 0 && i == item_at(s, e->x, e->y))
            activate(s, i, tile);
        return true;
    }
    case UI_MOVE:
        return true;
    default:
        set_pressed(s, -1, -1);
        return false;
    }
}

/* About: refresh the live values, repainting only those that changed */
static twin_time_t about_tick(ui_view_t *v)
{
    page_t *s = (page_t *) v;
    char buf[VAL_LEN];
    layout(s);
    for (int i = 0; i < s->n; i++) {
        if (!is_live(&s->items[i]))
            continue;
        format_value(s->items[i].id, buf, sizeof(buf));
        if (!strcmp(buf, s->vals[i]))
            continue;
        strcpy(s->vals[i], buf);
        ui_rect_t r = visible(s, value_rect(item_rect(s, i)));
        if (ui_h(r) > 0)
            ui_view_invalidate(v, r);
    }
    s->refreshes++;
    return 1000;
}

/* Date & time: repaint the clock when the minute turns */
static twin_time_t clock_tick(ui_view_t *v)
{
    page_t *s = (page_t *) v;
    phone_clock_t c;
    phone_clock(&c);
    int minute = c.hour * 60 + c.min;
    if (minute != s->minute) {
        s->minute = minute;
        layout(s);
        invalidate_item(s, 0);
    }
    return 1000;
}

static void open_page(page_id_t page)
{
    page_t *s = ui_view_new(sizeof(*s), page_titles[page], settings_paint,
                            settings_event, NULL);
    if (!s)
        return;
    s->page = page;
    s->pressed = s->pressed_tile = s->grab = -1;
    build(s);
    if (page == PAGE_ABOUT) {
        for (int i = 0; i < s->n; i++)
            if (is_live(&s->items[i]))
                format_value(s->items[i].id, s->vals[i], VAL_LEN);
        ui_view_every(&s->view, 1000, about_tick);
    } else if (page == PAGE_TIME) {
        phone_clock_t c;
        phone_clock(&c);
        s->minute = c.hour * 60 + c.min;
        ui_view_every(&s->view, 1000, clock_tick);
    }
    ui_view_show(&s->view);
}

void app_settings_open(void)
{
    open_page(PAGE_ROOT);
}

/*
 * Test hooks
 */

/* Rectangle (view coordinates) of the item or wallpaper tile labelled
 * @label on the Settings screen on top; false if there is none */
bool app_settings_item_rect(const char *label, ui_rect_t *out)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != settings_paint)
        return false;
    page_t *s = (page_t *) v;
    layout(s);
    for (int i = 0; i < s->n; i++) {
        const item_t *it = &s->items[i];
        if (it->kind == IT_WALLPAPERS) {
            ui_rect_t r = item_rect(s, i);
            for (int k = 0; k < phone_wallpaper_count(); k++) {
                if (strcmp(phone_wallpaper_name(k), label))
                    continue;
                *out = tile_rect(ui_w(r), r.top, k);
                return true;
            }
        } else if (it->kind != IT_SECTION && it->label &&
                   !strcmp(it->label, label)) {
            *out = it->kind == IT_SLIDER ? slider_rect(item_rect(s, i))
                                         : item_rect(s, i);
            return true;
        }
    }
    return false;
}

/* Current preference value by name; -1 if unknown */
int app_settings_pref(const char *name)
{
    if (!strcmp(name, "brightness"))
        return prefs.brightness;
    if (!strcmp(name, "volume"))
        return prefs.volume;
    if (!strcmp(name, "vibrate"))
        return prefs.vibrate;
    if (!strcmp(name, "keypad"))
        return prefs.keypad_tones;
    if (!strcmp(name, "24h"))
        return prefs.clock_24h;
    return -1;
}

/* Number of About value refreshes on the screen on top; -1 if not About */
int app_settings_about_refreshes(void)
{
    ui_view_t *v = phone_top();
    if (!v || v->paint != settings_paint || ((page_t *) v)->page != PAGE_ABOUT)
        return -1;
    return ((page_t *) v)->refreshes;
}
