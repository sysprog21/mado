/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Settings scenarios: every sub-screen opens and closes, theme, wallpaper,
 * sliders and switches change state, and About refreshes only its values.
 */

#include <string.h>

#include "phone-test.h"

bool app_settings_item_rect(const char *label, ui_rect_t *out);
int app_settings_pref(const char *name);
int app_settings_about_refreshes(void);

/* View coordinates of the top view to screen coordinates; works under any
 * window manager (title bars, full-screen views) */
static void to_screen(twin_coord_t *x, twin_coord_t *y)
{
    twin_window_t *w = phone_top()->window;
    *x += w->pixmap->x + w->client.left;
    *y += w->pixmap->y + w->client.top;
}

static void view_tap(twin_coord_t x, twin_coord_t y)
{
    to_screen(&x, &y);
    th_tap(phone_th, x, y);
    ph_wait(2000);
}

static void view_drag(twin_coord_t x0,
                      twin_coord_t y0,
                      twin_coord_t x1,
                      twin_coord_t y1)
{
    to_screen(&x0, &y0);
    to_screen(&x1, &y1);
    th_drag(phone_th, x0, y0, x1, y1, 8);
    ph_wait(2000);
}

/* Find an item on the Settings screen on top, scrolling it into view */
static bool find_item(const char *label, ui_rect_t *r)
{
    for (int tries = 0; tries < 4; tries++) {
        if (!app_settings_item_rect(label, r))
            break;
        twin_coord_t h = ui_view_height(phone_top());
        if (r->top >= PHONE_HEADER_H && r->bottom <= h)
            return true;
        twin_coord_t cx = ui_view_width(phone_top()) / 2;
        if (r->bottom > h)
            view_drag(cx, h - 20, cx, h - 120);
        else
            view_drag(cx, PHONE_HEADER_H + 20, cx, PHONE_HEADER_H + 120);
    }
    CHECK(0, "settings item '%s' reachable", label);
    return false;
}

static bool tap_item(const char *label)
{
    ui_rect_t r;
    if (!find_item(label, &r))
        return false;
    view_tap((r.left + r.right) / 2, (r.top + r.bottom) / 2);
    return true;
}

static bool top_is(const char *title)
{
    ui_view_t *v = phone_top();
    return v && v->title && !strcmp(v->title, title);
}

/* Open a sub-screen from the root list and check it is on top */
static bool open_sub(const char *label)
{
    if (!tap_item(label))
        return false;
    CHECK(top_is(label), "%s opened", label);
    CHECK_EQ(phone_depth(), 2, "sub-screen above the root list");
    return top_is(label);
}

static void back_to_root(void)
{
    phone_back();
    ph_wait(2000);
    CHECK(top_is("Settings"), "back on the Settings list");
    CHECK_EQ(phone_depth(), 1, "root list depth");
}

static void test_navigation(void)
{
    static const char *const subs[] = {"Display", "Sound", "Wallpaper",
                                       "Date & time", "About"};
    static const char *const shots[] = {"settings-display", "settings-sound",
                                        "settings-wallpaper", "settings-time",
                                        "settings-about"};
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
        if (!open_sub(subs[i]))
            continue;
        /* About shows live counters: consistency only, no golden */
        if (!strcmp(subs[i], "About"))
            ph_check(shots[i]);
        else
            ph_shot(shots[i]);
        if (i == 0) {
            /* The nav bar's middle button is Back above home */
            th_tap(phone_th, PHONE_W / 2, PHONE_H - PHONE_NAV_H / 2);
            ph_wait(2000);
            CHECK(top_is("Settings"), "nav Back returns to Settings");
        } else {
            back_to_root();
        }
    }
}

static void test_display(void)
{
    if (!open_sub("Display"))
        return;
    CHECK_EQ(phone_theme_index(), 0, "default theme");
    tap_item("Dark");
    CHECK_EQ(phone_theme_index(), 1, "Dark theme selected");
    ph_shot("settings-display-dark");

    /* Brightness slider: drag the knob toward the left end */
    ui_rect_t r;
    if (find_item("Brightness", &r)) {
        int before = app_settings_pref("brightness");
        twin_coord_t cy = (r.top + r.bottom) / 2;
        twin_coord_t x0 = r.left + 8 + (ui_w(r) - 16) * before / 1000;
        view_drag(x0, cy, r.left + 8 + (ui_w(r) - 16) / 4, cy);
        int after = app_settings_pref("brightness");
        CHECK(after < before && after >= 200 && after <= 300,
              "brightness dragged from %d to %d", before, after);
        CHECK(top_is("Display"), "slider drag does not navigate");
    }

    /* The theme reaches the home screen too */
    ph_home();
    ph_shot("settings-home-dark");

    ph_open("Settings");
    if (open_sub("Display")) {
        CHECK_EQ(app_settings_pref("brightness") < 700, 1,
                 "brightness kept across visits");
        tap_item("Olive");
        CHECK_EQ(phone_theme_index(), 0, "theme restored");
    }
    back_to_root();
}

static void test_sound(void)
{
    if (!open_sub("Sound"))
        return;
    int vib = app_settings_pref("vibrate");
    tap_item("Vibrate");
    CHECK_EQ(app_settings_pref("vibrate"), !vib, "vibrate toggled");
    int keypad = app_settings_pref("keypad");
    tap_item("Keypad tones");
    CHECK_EQ(app_settings_pref("keypad"), !keypad, "keypad tones toggled");

    ui_rect_t r;
    if (find_item("Volume", &r)) {
        twin_coord_t cy = (r.top + r.bottom) / 2;
        view_drag(r.left + 8 + (ui_w(r) - 16) / 2, cy, r.right - 2, cy);
        CHECK_EQ(app_settings_pref("volume"), 1000, "volume dragged to max");
    }
    ph_shot("settings-sound-changed");
    CHECK_EQ(th_damage_check(phone_th), 0, "sound screen consistent");

    /* Restore the switches; the slider value is kept for the next check */
    tap_item("Vibrate");
    tap_item("Keypad tones");
    CHECK_EQ(app_settings_pref("vibrate"), vib, "vibrate restored");
    CHECK_EQ(app_settings_pref("keypad"), keypad, "keypad restored");
    back_to_root();
    if (open_sub("Sound")) {
        CHECK_EQ(app_settings_pref("volume"), 1000, "volume kept");
        back_to_root();
    }
}

static void test_wallpaper(void)
{
    if (!open_sub("Wallpaper"))
        return;
    CHECK_EQ(phone_wallpaper(), 0, "default wallpaper");
    tap_item("Dusk");
    CHECK_EQ(phone_wallpaper(), 1, "Dusk selected");
    ph_shot("settings-wallpaper-dusk");
    tap_item(phone_wallpaper_name(0));
    CHECK_EQ(phone_wallpaper(), 0, "wallpaper restored");
    back_to_root();
}

static void test_about(void)
{
    if (!open_sub("About"))
        return;
    /* Bring the live values into view */
    ui_rect_t r;
    find_item("Pixels", &r);
    ph_wait(1100);

    twin_screen_t *screen = phone_screen();
    int ticks = app_settings_about_refreshes();
    uint64_t pixels = screen->stats.pixels;
    uint32_t frames = screen->stats.frames;
    ph_wait(1100);
    uint64_t delta = screen->stats.pixels - pixels;
    CHECK(app_settings_about_refreshes() > ticks, "about values refreshed");
    CHECK(screen->stats.frames > frames, "refresh reached the screen");
    CHECK(delta > 0 && delta < (uint64_t) PHONE_W * PHONE_H / 4,
          "refresh repainted %llu px, full frame is %d",
          (unsigned long long) delta, PHONE_W * PHONE_H);
    ph_check("settings-about-stats");
    back_to_root();
}

void test_settings(void)
{
    ph_open("Settings");
    CHECK(top_is("Settings"), "settings root on top");
    ph_shot("settings");

    test_navigation();
    test_display();
    test_sound();
    test_wallpaper();
    test_about();

    ph_home();
    CHECK_EQ(phone_theme_index(), 0, "theme 0 at exit");
    CHECK_EQ(phone_wallpaper(), 0, "wallpaper 0 at exit");
}
