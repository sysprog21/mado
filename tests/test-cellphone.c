/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Cellphone scenario test: boots the phone headless with a fixed clock and
 * drives the user journey (unlock, every app, navigation, swipe-back,
 * paging, keyboard input, theme switch, apps opened from the launcher).
 *
 * Gates:
 *  - structure: stack depth, top view, focus after each step
 *  - pixels: golden CRCs of key screens (skipped if build options differ)
 *  - consistency: incremental scan-out equals a full recomposite
 *  - memory: heap returns to baseline after app cycles; peak bounded
 *  - work: cache hit rate and composited pixels per transition frame
 */

#include <stdlib.h>
#include <string.h>

#include "phone-test.h"

static void fixed_clock(phone_clock_t *c)
{
    *c = (phone_clock_t) {
        .hour = 12, .min = 3, .sec = 0, .wday = 6, .mday = 9, .mon = 4};
}

const char *app_calc_eval(const char *typed);
bool app_home_icon_rect(int app, ui_rect_t *out);

th_t *phone_th;
static th_t *t;

void ph_wait(int ms)
{
    th_settle(t, ms);
}

void ph_check(const char *name)
{
    th_settle(t, 2000);
    CHECK_EQ(th_damage_check(t), 0, name);
}

void ph_shot(const char *name)
{
    ph_check(name);
    th_golden("cellphone", name, th_crc(t), t);
}

ui_view_t *ph_open(const char *name)
{
    int app = phone_app_find(name);
    CHECK(app >= 0, "app %s registered", name);
    phone_open(app);
    th_settle(t, 2000);
    ui_view_t *top = phone_top();
    CHECK(top && top->app == app, "%s on top", name);
    return top;
}

void ph_home(void)
{
    phone_home();
    th_settle(t, 2000);
    CHECK_EQ(phone_depth(), 0, "back on the home screen");
}

void ph_tap(twin_coord_t x, twin_coord_t y)
{
    th_tap(t, x, CY(y));
    th_settle(t, 2000);
}

void ph_drag(twin_coord_t x0, twin_coord_t y0, twin_coord_t x1, twin_coord_t y1)
{
    th_drag(t, x0, CY(y0), x1, CY(y1), 8);
    th_settle(t, 2000);
}

void ph_key(int code)
{
    /* Letters first, then the digits/symbols layout */
    ui_rect_t r, area = ui_kbd_rect(PHONE_CONTENT_H);
    for (int symbols = 0; symbols < 2; symbols++) {
        ui_kbd_t k;
        ui_kbd_init(&k);
        k.symbols = symbols;
        if (ui_kbd_find(&k, area, code, &r)) {
            ph_tap((r.left + r.right) / 2, (r.top + r.bottom) / 2);
            return;
        }
    }
    CHECK(0, "no key for '%c'", code);
}

static void unlock(void)
{
    /* Drag the knob all the way along the track */
    th_drag(t, 33, 295, 230, 295, 10);
    th_settle(t, 2000);
}

static void test_boot_and_unlock(void)
{
    CHECK(phone_locked(), "boots locked");
    ph_shot("lock");
    CHECK(t->screen->active && t->screen->active->window &&
              t->screen->active->window->style == TwinWindowFullscreen,
          "lock screen focused");

    /* A short drag springs back */
    th_drag(t, 33, 295, 90, 295, 4);
    th_settle(t, 2000);
    CHECK(phone_locked(), "short slide stays locked");

    unlock();
    CHECK(!phone_locked(), "full slide unlocks");
    CHECK_EQ(phone_depth(), 0, "home after unlock");
    ph_shot("home");
}

static void test_wrap(void)
{
    twin_coord_t widest = 0;
    int n = ui_text_wrap(NULL, FONT_SM, 0, 0, 100, 14, 0,
                         "Hey, you coming to "
                         "the meeting?",
                         &widest);
    CHECK(n >= 2 && n <= 4, "wrapped into %d lines", n);
    CHECK(widest <= 100 && widest > 40, "widest line %d", widest);
    CHECK_EQ(ui_text_wrap(NULL, FONT_SM, 0, 0, 200, 14, 0, "short", NULL), 1,
             "short text is one line");
    CHECK(ui_text_wrap(NULL, FONT_SM, 0, 0, 30, 14, 0, "Supercalifragilistic",
                       NULL) >= 1,
          "overlong word still terminates");
}

#if defined(CONFIG_DEMO_CALCULATOR)
static void test_calculator(void)
{
    /* Opened from its icon, wherever the apps built put it */
    ui_rect_t r;
    bool shown = app_home_icon_rect(phone_app_find("Calculator"), &r);
    CHECK(shown, "calculator icon on the first page");
    if (!shown)
        return;
    ph_tap((r.left + r.right) / 2, (r.top + r.bottom) / 2);
    CHECK_EQ(phone_depth(), 1, "calculator opened");
    ph_shot("calculator");

    CHECK_STR(app_calc_eval("C12+30="), "42", "12+30");
    CHECK_STR(app_calc_eval("C1.5x4="), "6", "1.5x4");
    CHECK_STR(app_calc_eval("C7/0="), "Error", "divide by zero");
    CHECK_STR(app_calc_eval("C10/4="), "2.5", "10/4");
    CHECK_STR(app_calc_eval("C12.5\b\b+1="), "13", "erase digits and dot");
    CHECK_STR(app_calc_eval("C7\b"), "0", "erase the last digit");
    CHECK_STR(app_calc_eval("C5-\b"), "5", "nothing typed after the operator");
    CHECK_STR(app_calc_eval("C3+4=\b"), "7", "a result is not erased");
    CHECK_STR(app_calc_eval("C0~3"), "-3", "sign typed before the digits");
    CHECK_STR(app_calc_eval("C5.~3"), "-5.3", "sign keeps a trailing dot");
    /* Ten 12-digit terms overflow the 64-bit fixed-point accumulator */
    char big[160];
    for (int sign = 0; sign < 2; sign++) {
        strcpy(big, "C0");
        for (int i = 0; i < 10; i++)
            strcat(big, sign ? "-999999999999" : "+999999999999");
        strcat(big, "=");
        CHECK_STR(app_calc_eval(big), "Error",
                  sign ? "subtraction overflow" : "addition overflow");
    }
    CHECK_STR(app_calc_eval("C0-999999999999="), "-999999999999",
              "large negative still fits");
    /* Typed keys: U+0131 would narrow to '1' */
    app_calc_eval("C");
    th_key(t, '7');
    th_key(t, 0x131);
    th_settle(t, 100);
    CHECK_STR(app_calc_eval(""), "7", "non-ASCII key ignored");
    th_settle(t, 500);
    CHECK_EQ(th_damage_check(t), 0, "calculator display consistent");

    phone_back();
    th_settle(t, 2000);
    CHECK_EQ(phone_depth(), 0, "back to home");
}
#else
static void test_calculator(void) {}
#endif

/*
 * Budgets for the whole phone: memory returns to baseline after visiting
 * every phone app, the peak stays bounded, transitions cost about one frame of
 * compositing per animation frame, and a warm pass is served by the cache.
 */
static void test_budgets(void)
{
    ph_home();
    /* One warm-up pass so caches and reuse pools reach steady state */
    for (int i = 0; i < phone_app_count(); i++) {
        if (phone_app_at(i)->demo)
            continue; /* own windows, covered by test_demos() */
        phone_open(i);
        th_settle(t, 2000);
        ph_home();
    }
    size_t base = th_heap_net(t);
    /* The allocator's peak counts every pool still held from earlier tests,
     * so measure its growth from the whole heap, not from the net figure */
    size_t raw_base = th_heap_raw();
    twin_memory_reset_peak();
    twin_cache_stats_t c0, c1;
    twin_cache_get_stats(&c0);

    uint32_t worst_frame = 0;
    for (int i = 0; i < phone_app_count(); i++) {
        if (phone_app_at(i)->demo)
            continue;
        twin_screen_stats_t s0 = t->screen->stats;
        phone_open(i);
        th_settle(t, 2000);
        twin_screen_stats_t s1 = t->screen->stats;
        uint32_t frames = s1.frames - s0.frames;
        uint32_t per =
            frames ? (uint32_t) ((s1.pixels - s0.pixels) / frames) : 0;
        if (per > worst_frame)
            worst_frame = per;
        CHECK_EQ(phone_depth(), 1, phone_app_at(i)->name);
        ph_home();
    }
    twin_cache_get_stats(&c1);
    size_t after = th_heap_net(t);
    twin_memory_info_t mem;
    twin_memory_get_info(&mem);

    uint32_t hits = c1.hits - c0.hits, misses = c1.misses - c0.misses;
    uint32_t rate = hits + misses ? hits * 100 / (hits + misses) : 100;
    /* Peak on the same footing as base: the growth over the whole heap,
     * added to the net baseline, so pools other tests left behind do not
     * count */
    size_t peak = base + (mem.peak_bytes - raw_base);
    printf("cellphone: heap at home %zu bytes, peak over all apps %zu bytes\n",
           base, peak);
    printf("cellphone: worst opening animation %u pixels/frame (screen %d)\n",
           worst_frame, PHONE_W * PHONE_H);
    printf(
        "cellphone: warm cache hit rate %u%% (%u hits, %u misses, %zu "
        "bytes resident)\n",
        rate, hits, misses, c1.bytes);

    CHECK(after <= base, "visiting every app leaks (%zu -> %zu)", base, after);
    /* Home + one app + bars + caches, not one surface per app */
    size_t surface = (size_t) PHONE_W * PHONE_CONTENT_H * 4;
    CHECK(peak < base + 3 * surface, "peak bounded (%zu over a %zu baseline)",
          peak, base);
    CHECK(worst_frame <= (uint32_t) (PHONE_W * PHONE_H) * 3 / 2,
          "transition frames composite about one screen (%u)", worst_frame);
    CHECK(rate >= 80, "warm pass mostly served by the cache (%u%%)", rate);
}

int main(void)
{
    phone_set_clock_source(fixed_clock);
    t = phone_th = th_create(PHONE_W, PHONE_H);
    phone_boot(t->screen);
    th_settle(t, 2000);
    /* The frames show whichever apps the launcher lists */
    char apps[512] = "";
    for (int i = 0; i < phone_app_count(); i++) {
        size_t n = strlen(apps);
        snprintf(apps + n, sizeof(apps) - n, "%s,", phone_app_at(i)->name);
    }
    th_golden_salt(apps);

    test_boot_and_unlock();
    test_wrap();
    test_calculator();

    /* App suites; each starts and ends on the home screen */
    void (*suites[])(void) = {test_dialer, test_calllog,  test_contacts,
                              test_sms,    test_music,    test_photos,
                              test_camera, test_settings, test_games,
                              test_scenes, test_demos,    test_regress};
    for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        suites[i]();
        if (phone_depth())
            ph_home();
    }
    test_budgets();

    /* Shutdown closes what the phone opened, demo windows included */
    for (int i = 0; i < phone_app_count(); i++)
        if (phone_app_at(i)->demo) {
            phone_open(i);
            th_settle(t, 2000);
            break;
        }
    phone_shutdown();
    CHECK(!t->screen->windows, "shutdown leaves no window behind");
    th_destroy(t);
    twin_cache_flush();
    return th_report("cellphone");
}
