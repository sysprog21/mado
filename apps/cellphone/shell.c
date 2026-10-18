/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone shell: themes, clock, app registry, system bars, navigation.
 *
 * The status bar and navigation bar are plain windows registered with the
 * mobile window manager as system bars; every screen above them is a view.
 * Navigation is nothing more than showing and closing views: the window
 * manager owns the stack, the transitions and the memory of hidden screens.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "apps.h"
#include "phone.h"

/*
 * Themes
 */
static const phone_theme_t themes[] = {
    {
        /* Sage: warm white pages, sage green groups, deep olive bars */
        .name = "Sage",
        .primary = UI_RGB(0x667a3e),
        .primary_dk = UI_RGB(0x4f5f30),
        .statusbar = UI_RGB(0x4e5540),
        .statusbar_grad = UI_RGB(0x646b54),
        .header = UI_RGB(0x7d8568),
        .header_grad = UI_RGB(0x6a7257),
        .on_bar = UI_RGB(0xf7f5ec),
        .navbar = UI_RGB(0xd3d9c4),
        .navbar_grad = UI_RGB(0xe0e5d3),
        .bg = UI_RGB(0xf4f2e9),
        .bg_grad = UI_RGB(0xedeadf),
        .card = UI_RGB(0xfbfaf4),
        .group = UI_RGB(0xe2e6d6),
        .text = UI_RGB(0x26301f),
        .text_sec = UI_RGB(0x5c6652),
        .separator = UI_RGB(0xdadccc),
        .call_green = UI_RGB(0x55873a),
        .call_red = UI_RGB(0xb0473e),
        .missed_red = UI_RGB(0xa63c34),
        .sms_sent = UI_RGB(0xdde5c6),
        .sms_recv = UI_RGB(0xfbfaf4),
        .indicator = UI_RGB(0xbcc2ab),
        .on_primary = UI_RGB(0xffffff),
    },
    {
        .name = "Dark",
        .primary = UI_RGB(0x8fb06a),
        .primary_dk = UI_RGB(0x6f8f4c),
        .statusbar = UI_RGB(0x000000),
        .statusbar_grad = UI_RGB(0x0c0c0c),
        .header = UI_RGB(0x262923),
        .header_grad = UI_RGB(0x1e201c),
        .on_bar = UI_RGB(0xe4e6df),
        .navbar = UI_RGB(0x1a1b18),
        .navbar_grad = UI_RGB(0x22241f),
        .bg = UI_RGB(0x121311),
        .bg_grad = UI_RGB(0x121311),
        .card = UI_RGB(0x1c1d1a),
        .group = UI_RGB(0x2a2c27),
        .text = UI_RGB(0xe4e6df),
        .text_sec = UI_RGB(0xa0a498),
        .separator = UI_RGB(0x34372f),
        .call_green = UI_RGB(0x6fbf5a),
        .call_red = UI_RGB(0xe0574d),
        .missed_red = UI_RGB(0xe0574d),
        .sms_sent = UI_RGB(0x34452a),
        .sms_recv = UI_RGB(0x2a2c27),
        .indicator = UI_RGB(0x50544a),
        .on_primary = UI_RGB(0x10140c),
    },
    {
        .name = "Ocean",
        .primary = UI_RGB(0x1e78b4),
        .primary_dk = UI_RGB(0x165e8e),
        .statusbar = UI_RGB(0x14496e),
        .statusbar_grad = UI_RGB(0x1d5f8c),
        .header = UI_RGB(0x2b78a8),
        .header_grad = UI_RGB(0x226690),
        .on_bar = UI_RGB(0xffffff),
        .navbar = UI_RGB(0xcfe0ec),
        .navbar_grad = UI_RGB(0xdeeaf3),
        .bg = UI_RGB(0xf2f7fa),
        .bg_grad = UI_RGB(0xe8f0f6),
        .card = UI_RGB(0xffffff),
        .group = UI_RGB(0xdde9f2),
        .text = UI_RGB(0x1c2833),
        .text_sec = UI_RGB(0x55687a),
        .separator = UI_RGB(0xd5dee5),
        .call_green = UI_RGB(0x2e9a58),
        .call_red = UI_RGB(0xc8403f),
        .missed_red = UI_RGB(0xc03030),
        .sms_sent = UI_RGB(0xcfe5f6),
        .sms_recv = UI_RGB(0xffffff),
        .indicator = UI_RGB(0xa9c3d6),
        .on_primary = UI_RGB(0xffffff),
    },
};

#define N_THEMES ((int) (sizeof(themes) / sizeof(themes[0])))

static int theme_idx;

const phone_theme_t *phone_theme(void)
{
    return &themes[theme_idx];
}

int phone_theme_count(void)
{
    return N_THEMES;
}

int phone_theme_index(void)
{
    return theme_idx;
}

const phone_theme_t *phone_theme_at(int idx)
{
    return (idx >= 0 && idx < N_THEMES) ? &themes[idx] : NULL;
}

/*
 * Shell state
 */
static twin_screen_t *g_screen;
static twin_window_t *g_status, *g_nav;
static ui_view_t *g_home;
static twin_timeout_t *g_tick;
static int g_wallpaper;
static int g_status_minute = -1;
static int g_nav_pressed = -1;
/* nav_state() the nav bar last painted in full, NAV_STALE if unknown */
#define NAV_STALE (-100)
static int g_nav_drawn = NAV_STALE;

twin_screen_t *phone_screen(void)
{
    return g_screen;
}

static void _repaint_window(twin_window_t *w)
{
    if (!w)
        return;
    twin_rect_t c = twin_window_client(w);
    twin_window_damage(w, c.left, c.top, c.right, c.bottom);
}

static int nav_state(void);

/* The view stack changed: repaint the nav bar if its tabs look different */
void phone_view_changed(void)
{
    if (nav_state() != g_nav_drawn)
        _repaint_window(g_nav);
}

void phone_theme_set(int idx)
{
    if (idx < 0 || idx >= N_THEMES || !g_screen)
        return;
    theme_idx = idx;
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;
         w = twin_window_next(w))
        _repaint_window(w);
}

/*
 * Wallpapers
 */
typedef struct {
    const char *name;
    twin_argb32_t top, bottom, label;
} wallpaper_t;

static const wallpaper_t wallpapers[] = {
    {"Theme", 0, 0, 0},
    {"Dusk", UI_RGB(0x4a3a78), UI_RGB(0xe08a5a), UI_RGB(0xffffff)},
    {"Ocean", UI_RGB(0x0f4c81), UI_RGB(0x46a6c8), UI_RGB(0xffffff)},
    {"Forest", UI_RGB(0x1f4d36), UI_RGB(0x7aa86a), UI_RGB(0xffffff)},
};

#define N_WALLPAPERS ((int) (sizeof(wallpapers) / sizeof(wallpapers[0])))

int phone_wallpaper(void)
{
    return g_wallpaper;
}

int phone_wallpaper_count(void)
{
    return N_WALLPAPERS;
}

const char *phone_wallpaper_name(int idx)
{
    return (idx >= 0 && idx < N_WALLPAPERS) ? wallpapers[idx].name : "";
}

void phone_set_wallpaper(int idx)
{
    if (idx < 0 || idx >= N_WALLPAPERS)
        return;
    g_wallpaper = idx;
    if (g_home)
        ui_view_invalidate_all(g_home);
}

twin_argb32_t phone_paint_wallpaper(twin_pixmap_t *px, ui_rect_t r, int idx)
{
    const phone_theme_t *th = phone_theme();
    if (idx <= 0 || idx >= N_WALLPAPERS) {
        /* The grouped surface: the launcher's tiles stand out on it */
        ui_vgrad(px, r, th->group, ui_darken(th->group, 6));
        return th->text;
    }
    ui_vgrad(px, r, wallpapers[idx].top, wallpapers[idx].bottom);
    return wallpapers[idx].label;
}

/*
 * Clock
 */
static void (*clock_source)(phone_clock_t *c);

void phone_set_clock_source(void (*source)(phone_clock_t *c))
{
    clock_source = source;
}

void phone_clock(phone_clock_t *c)
{
    if (clock_source) {
        clock_source(c);
        return;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    c->hour = tm.tm_hour;
    c->min = tm.tm_min;
    c->sec = tm.tm_sec;
    c->wday = tm.tm_wday;
    c->mday = tm.tm_mday;
    c->mon = tm.tm_mon;
}

void phone_format_time(char *buf, size_t len)
{
    phone_clock_t c;
    phone_clock(&c);
    snprintf(buf, len, "%02d:%02d", c.hour, c.min);
}

void phone_format_date(char *buf, size_t len)
{
    static const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
    static const char *months[] = {
        "January", "February", "March",     "April",   "May",      "June",
        "July",    "August",   "September", "October", "November", "December"};
    phone_clock_t c;
    phone_clock(&c);
    snprintf(buf, len, "%s, %s %d", days[c.wday % 7], months[c.mon % 12],
             c.mday);
}

/*
 * App registry and icons
 */
static void glyph_snake(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg);
static void glyph_pong(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg);
static void glyph_tetris(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg);

static const phone_app_t apps[] = {
    {"Phone", TWIN_SYM_PHONE, NULL, UI_RGB(0x5b8c45), app_dialer_open, NULL},
    {"Contacts", TWIN_SYM_CONTACTS, NULL, UI_RGB(0x5a7896), app_contacts_open,
     NULL},
    {"Messages", TWIN_SYM_MESSAGE, NULL, UI_RGB(0x4f8c7c), app_sms_open, NULL},
    {"Calculator", TWIN_SYM_CALCULATOR, NULL, UI_RGB(0xc08a3e), app_calc_open,
     NULL},
    {"Music", TWIN_SYM_MUSIC, NULL, UI_RGB(0xa95c72), app_music_open, NULL},
    {"Photos", TWIN_SYM_IMAGE, NULL, UI_RGB(0xbb6e3e), app_photos_open, NULL},
    {"Camera", TWIN_SYM_CAMERA, NULL, UI_RGB(0x6c7064), app_camera_open, NULL},
    {"Settings", TWIN_SYM_SETTINGS, NULL, UI_RGB(0x767b6c), app_settings_open,
     NULL},
    {"Call Log", TWIN_SYM_HISTORY, NULL, UI_RGB(0x7a6a96), app_calllog_open,
     NULL},
    {"Snake", NULL, glyph_snake, UI_RGB(0x4d8f86), app_snake_open, NULL},
    {"Pong", NULL, glyph_pong, UI_RGB(0x646c9a), app_pong_open, NULL},
    {"Tetris", NULL, glyph_tetris, UI_RGB(0xad5348), app_tetris_open, NULL},
};

#define N_APPS ((int) (sizeof(apps) / sizeof(apps[0])))

/* Then every other app of the shared registry (apps.c), in registry order:
 * a mobile build compiles in only apps that run under it.  Listed at boot. */
static phone_app_t demos[16];
static int n_demos;

static void _list_demos(void)
{
    static const twin_argb32_t accents[] = {
        UI_RGB(0x3f7f78), UI_RGB(0x7c7468), UI_RGB(0x82689a),
        UI_RGB(0x9a5f86), UI_RGB(0x7a8f3e), UI_RGB(0x4a7fa0),
    };
    n_demos = 0;
    for (int i = 0; i < apps_count(); i++) {
        const apps_entry_t *e = apps_at(i);
        if (e->shell)
            continue;
        if (n_demos == (int) (sizeof(demos) / sizeof(demos[0])))
            break;
        demos[n_demos] = (phone_app_t) {
            .name = e->name,
            .sym = e->icon,
            .accent = accents[n_demos % (sizeof(accents) / sizeof(accents[0]))],
            .demo = e,
        };
        n_demos++;
    }
}

int phone_app_count(void)
{
    return N_APPS + n_demos;
}

const phone_app_t *phone_app_at(int idx)
{
    if (idx >= 0 && idx < N_APPS)
        return &apps[idx];
    return idx >= N_APPS && idx < N_APPS + n_demos ? &demos[idx - N_APPS]
                                                   : NULL;
}

int phone_app_find(const char *name)
{
    for (int i = 0; i < phone_app_count(); i++)
        if (!strcmp(phone_app_at(i)->name, name))
            return i;
    return -1;
}

static void glyph_snake(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg)
{
    int u = ui_w(p) / 8;
    twin_coord_t x = p.left + 2 * u, y = p.top + 2 * u;
    twin_path_t *path = ui_path();
    twin_path_move(path, twin_int_to_fixed(x), twin_int_to_fixed(y + 4 * u));
    twin_path_draw(path, twin_int_to_fixed(x), twin_int_to_fixed(y));
    twin_path_draw(path, twin_int_to_fixed(x + 4 * u), twin_int_to_fixed(y));
    twin_path_draw(path, twin_int_to_fixed(x + 4 * u),
                   twin_int_to_fixed(y + 2 * u));
    twin_path_draw(path, twin_int_to_fixed(x + 2 * u),
                   twin_int_to_fixed(y + 2 * u));
    twin_path_draw(path, twin_int_to_fixed(x + 2 * u),
                   twin_int_to_fixed(y + 4 * u));
    twin_path_set_cap_style(path, TwinCapRound);
    twin_paint_stroke(px, fg, path, twin_int_to_fixed(u + u / 2));
    ui_path_done(path);
    ui_circle(px, x + 4 * u, y + 4 * u, u / 2 + 1, ui_alpha(fg, 0xc0));
}

static void glyph_pong(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg)
{
    int u = ui_w(p) / 8;
    ui_rrect(px, ui_rect(p.left + u + u / 2, p.top + 2 * u, u, 3 * u), u / 2,
             fg);
    ui_rrect(px, ui_rect(p.right - 2 * u - u / 2, p.top + 3 * u, u, 3 * u),
             u / 2, fg);
    ui_circle(px, (p.left + p.right) / 2, p.top + 3 * u + u / 2, u * 2 / 3 + 1,
              fg);
}

static void glyph_tetris(twin_pixmap_t *px, ui_rect_t p, twin_argb32_t fg)
{
    int u = ui_w(p) / 7;
    twin_coord_t x = p.left + (ui_w(p) - 3 * u) / 2;
    twin_coord_t y = p.top + (ui_h(p) - 2 * u) / 2;
    static const int cells[4][2] = {{0, 0}, {1, 0}, {2, 0}, {1, 1}};
    for (int i = 0; i < 4; i++)
        ui_rrect(px,
                 ui_rect(x + cells[i][0] * u + 1, y + cells[i][1] * u + 1,
                         u - 2, u - 2),
                 2, i == 3 ? ui_alpha(fg, 0xb0) : fg);
}

void phone_app_icon(twin_pixmap_t *px, int app, ui_rect_t plate, bool pressed)
{
    const phone_app_t *a = phone_app_at(app);
    if (!a)
        return;
    /* Squircle plate: radius ~27% of the side, accent fading 14% darker */
    int radius = ui_w(plate) * 27 / 100;
    twin_argb32_t top = a->accent, bottom = ui_darken(a->accent, 14);
    if (pressed) {
        top = ui_darken(top, 18);
        bottom = ui_darken(bottom, 18);
    }
    ui_rrect(px, ui_rect(plate.left, plate.top + 2, ui_w(plate), ui_h(plate)),
             radius, 0x28000000);
    ui_rrect_grad(px, plate, radius, top, bottom);
    /* A soft sheen over the upper half: the small tactile touch the
     * launcher keeps, never repeated in toolbars or lists */
    ui_rrect_grad(px,
                  ui_rect(plate.left + 2, plate.top + 2, ui_w(plate) - 4,
                          ui_h(plate) * 2 / 5),
                  radius - 2, 0x2c2c2c2c, 0x04040404);
    if (a->sym)
        ui_icon(px, a->sym, ui_w(plate) / 2, (plate.left + plate.right) / 2,
                (plate.top + plate.bottom) / 2, 0xffffffff);
    else if (a->glyph)
        a->glyph(px, plate, 0xffffffff);
}

/*
 * Status bar: signal, time, battery
 */
static void status_draw(twin_window_t *w)
{
    const phone_theme_t *th = phone_theme();
    twin_pixmap_t *px = twin_window_pixmap(w);
    ui_rect_t r = ui_rect(0, 0, PHONE_W, PHONE_STATUS_H);
    ui_vgrad(px, r, th->statusbar_grad, th->statusbar);
    ui_icon(px, TWIN_SYM_SIGNAL, FONT_SM, 14, PHONE_STATUS_H / 2, th->on_bar);
    ui_icon(px, TWIN_SYM_WIFI, FONT_SM, 34, PHONE_STATUS_H / 2, th->on_bar);
    char t[8];
    phone_format_time(t, sizeof(t));
    ui_text_in(px, FONT_SM, r, UI_CENTER, th->on_bar, t);
    ui_icon(px, TWIN_SYM_BATTERY, FONT_NORMAL, PHONE_W - 16, PHONE_STATUS_H / 2,
            th->on_bar);
}

static twin_time_t status_tick(twin_time_t now, void *closure)
{
    (void) now;
    twin_window_t *w = closure;
    phone_clock_t c;
    phone_clock(&c);
    int minute = c.hour * 60 + c.min;
    if (minute != g_status_minute) {
        g_status_minute = minute;
        /* Only the time label changes */
        twin_window_damage(w, PHONE_W / 2 - 30, 0, PHONE_W / 2 + 30,
                           PHONE_STATUS_H);
    }
    return 1000;
}

/*
 * Navigation bar: Dialer | Home/Back | Contacts
 */
#define NAV_TABS 3

static int nav_active(void)
{
    ui_view_t *top = phone_top();
    if (top && top->app == phone_app_find("Phone"))
        return 0;
    if (top && top->app == phone_app_find("Contacts"))
        return 2;
    return phone_depth() > 0 ? -1 : 1;
}

/* Everything the tabs show besides the press highlight and the theme */
static int nav_state(void)
{
    return nav_active() * 2 + (phone_depth() > 0);
}

static ui_rect_t nav_tab_rect(int i)
{
    twin_coord_t w = PHONE_W / NAV_TABS;
    return ui_rect(i * w + 3, 3, w - 6, PHONE_NAV_H - 6);
}

static void nav_draw(twin_window_t *w)
{
    const phone_theme_t *th = phone_theme();
    twin_pixmap_t *px = twin_window_pixmap(w);
    ui_rect_t clip = twin_pixmap_get_clip(px);
    bool full = clip.left <= 0 && clip.top <= 0 && clip.right >= PHONE_W &&
                clip.bottom >= PHONE_NAV_H;
    g_nav_drawn = full ? nav_state() : NAV_STALE;
    ui_vgrad(px, ui_rect(0, 0, PHONE_W, PHONE_NAV_H), th->navbar_grad,
             th->navbar);
    ui_fill(px, ui_rect(0, 0, PHONE_W, 1), th->separator);
    bool back = phone_depth() > 0;
    int active = nav_active();
    static const char *labels[NAV_TABS] = {"Dialer", "Home", "Contacts"};
    static const char *syms[NAV_TABS] = {TWIN_SYM_PHONE, TWIN_SYM_HOME,
                                         TWIN_SYM_CONTACTS};
    for (int i = 0; i < NAV_TABS; i++) {
        ui_rect_t r = nav_tab_rect(i);
        /* The current page is filled with the accent; the rest are soft
         * keys on the tray, so the selection never relies on color alone */
        bool on = i == active;
        ui_button(px, r, 7, on ? UI_BTN_PRIMARY : UI_BTN_CARD,
                  i == g_nav_pressed, 0, NULL);
        twin_argb32_t fg = on ? th->on_primary : th->text;
        const char *sym = (i == 1 && back) ? TWIN_SYM_LEFT : syms[i];
        const char *label = (i == 1 && back) ? "Back" : labels[i];
        twin_coord_t cx = (r.left + r.right) / 2;
        ui_icon(px, sym, FONT_NORMAL, cx, r.top + 12, fg);
        ui_text(px, FONT_SM, cx, r.bottom - 5, UI_CENTER, fg, label);
    }
}

static int nav_hit(twin_coord_t x, twin_coord_t y)
{
    for (int i = 0; i < NAV_TABS; i++)
        if (ui_hit(nav_tab_rect(i), x, y))
            return i;
    return -1;
}

/* The stack holds phone views and the windows of registry apps alike */
static bool is_app_window(twin_window_t *w)
{
    return twin_window_is_mapped(w) &&
           twin_window_style(w) == TwinWindowApplication;
}

static bool is_home(twin_window_t *w)
{
    return g_home && w == g_home->window;
}

/* Destroy every app window except home and @keep, without animation */
static void _drop_apps(twin_window_t *keep)
{
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;) {
        twin_window_t *next = twin_window_next(w);
        if (w != keep && is_app_window(w) && !is_home(w))
            twin_window_destroy(w);
        w = next;
    }
}

static void nav_go_root(const char *name)
{
    int app = phone_app_find(name);
    ui_view_t *top = phone_top();
    if (top && top->app == app)
        return;
    /* Tabs are destinations, not history: reset to home, then open */
    _drop_apps(NULL);
    phone_open(app);
}

static bool nav_event(twin_window_t *w, twin_event_t *ev)
{
    switch (ev->kind) {
    case TwinEventButtonDown:
        g_nav_pressed = nav_hit(ev->u.pointer.x, ev->u.pointer.y);
        _repaint_window(w);
        return true;
    case TwinEventButtonUp: {
        int tab = nav_hit(ev->u.pointer.x, ev->u.pointer.y);
        bool clicked = tab >= 0 && tab == g_nav_pressed;
        g_nav_pressed = -1;
        _repaint_window(w);
        if (!clicked)
            return true;
        if (tab == 0)
            nav_go_root("Phone");
        else if (tab == 2)
            nav_go_root("Contacts");
        else if (phone_depth() > 0)
            phone_back();
        return true;
    }
    case TwinEventMotion:
        return true;
    default:
        return false;
    }
}

/*
 * Navigation
 */
static twin_window_t *_top_app(void)
{
    twin_window_t *top = NULL;
    if (!g_screen)
        return NULL;
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;
         w = twin_window_next(w))
        if (is_app_window(w))
            top = w;
    return top;
}

ui_view_t *phone_top(void)
{
    twin_window_t *w = _top_app();
    return w ? ui_view_of(w) : NULL;
}

int phone_depth(void)
{
    int n = 0;
    if (!g_screen)
        return 0;
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;
         w = twin_window_next(w))
        if (is_app_window(w) && !is_home(w))
            n++;
    return n;
}

void phone_open(int app)
{
    const phone_app_t *a = phone_app_at(app);
    if (!a)
        return;
    if (a->demo) {
        /* Its own windows: the policy stacks them over the work area */
        a->demo->start(g_screen, 0, PHONE_STATUS_H, PHONE_W, PHONE_CONTENT_H);
        phone_view_changed();
        return;
    }
    if (!a->open)
        return;
    a->open();
    /* Tag the view the app just pushed */
    ui_view_t *top = phone_top();
    if (top && top != g_home && top->app < 0)
        top->app = app;
    phone_view_changed();
}

void phone_back(void)
{
    twin_window_t *top = _top_app();
    if (top && !is_home(top))
        twin_window_back(top);
}

void phone_home(void)
{
    twin_window_t *top = _top_app();
    if (!top || is_home(top))
        return;
    /* Hidden screens go first (no animation), then the visible one pops */
    _drop_apps(top);
    twin_window_close(top);
}

static ui_view_t *lock_view(void)
{
    if (!g_screen)
        return NULL;
    for (twin_window_t *w = twin_screen_first_window(g_screen); w;
         w = twin_window_next(w))
        if (ui_view_of(w) && twin_window_style(w) == TwinWindowFullscreen &&
            twin_window_is_mapped(w))
            return ui_view_of(w);
    return NULL;
}

bool phone_locked(void)
{
    return lock_view() != NULL;
}

void phone_lock(void)
{
    if (phone_locked())
        return;
    ui_view_t *lock = lock_create();
    if (lock)
        ui_view_show(lock);
}

/*
 * Boot
 */
/* Whoever destroys the bars (shutdown, screen teardown) must not leave the
 * shell pointing at them */
static void bar_destroyed(twin_window_t *w)
{
    if (w == g_status) {
        g_status = NULL;
        if (g_tick)
            twin_clear_timeout(g_tick);
        g_tick = NULL;
    }
    if (w == g_nav)
        g_nav = NULL;
}

static void home_destroyed(ui_view_t *v)
{
    if (v == g_home)
        g_home = NULL;
}

static twin_window_t *bar_create(twin_coord_t y,
                                 twin_coord_t h,
                                 twin_draw_func_t draw,
                                 twin_event_func_t event)
{
    twin_window_t *w = twin_window_create(g_screen, PHONE_FORMAT,
                                          TwinWindowPlain, 0, y, PHONE_W, h);
    if (!w)
        return NULL;
    twin_window_set_callbacks(w, draw, event, bar_destroyed);
    twin_window_set_opaque(w, true);
    twin_window_draw(w);
    twin_window_show(w);
    return w;
}

bool phone_boot(twin_screen_t *screen)
{
    if (!ui_init())
        return false;
    g_screen = screen;
    _list_demos();

    g_status = bar_create(0, PHONE_STATUS_H, status_draw, NULL);
    g_nav = bar_create(PHONE_H - PHONE_NAV_H, PHONE_NAV_H, nav_draw, nav_event);
    twin_screen_set_bars(screen, g_status, g_nav);
    if (g_status)
        g_tick = twin_set_timeout(status_tick, 1000, g_status);

    g_home = home_create();
    if (!g_status || !g_nav || !g_home) {
        /* No shell without its bars and home: let the caller fall back */
        phone_shutdown();
        return false;
    }
    g_home->destroy = home_destroyed;
    ui_view_show(g_home);
    phone_lock();
    return true;
}

void phone_shutdown(void)
{
    if (!g_screen)
        return;
    if (g_tick)
        twin_clear_timeout(g_tick);
    g_tick = NULL;
    /* Views and the windows of apps opened from home first (top-down),
     * then the bars */
    for (;;) {
        twin_window_t *last = NULL;
        for (twin_window_t *w = twin_screen_first_window(g_screen); w;
             w = twin_window_next(w))
            if (ui_view_of(w) || twin_window_style(w) == TwinWindowApplication)
                last = w;
        if (!last)
            break;
        twin_window_destroy(last);
    }
    twin_screen_set_bars(g_screen, NULL, NULL);
    if (g_status)
        twin_window_destroy(g_status);
    if (g_nav)
        twin_window_destroy(g_nav);
    g_status = g_nav = NULL;
    g_home = NULL;
    g_screen = NULL;
    g_status_minute = -1;
    g_nav_pressed = -1;
    theme_idx = 0;
    g_wallpaper = 0;
    ui_fini();
}
