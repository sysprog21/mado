/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Registry apps from the launcher: every app in apps.h marked for the
 * mobile policy has a home-screen icon that opens the same program the
 * desktop runs, stacked by the mobile window manager over the work area.
 * Edge swipe and the nav bar's Back close its windows one at a time, the
 * nav bar follows the stack, and nothing is left behind.
 */

#include "phone-test.h"

/* Home screen grid (home.c): 3x3 icons per page below a 6 px margin */
#define COLS 3
#define PER_PAGE 9
#define CELL_W (PHONE_W / COLS)
#define CELL_H 74
#define GRID_TOP 6

static uint32_t nav_crc(void)
{
    uint32_t crc = 0;
    for (twin_coord_t y = PHONE_H - PHONE_NAV_H; y < PHONE_H; y++)
        for (twin_coord_t x = 0; x < PHONE_W; x++)
            crc = crc * 31 + th_pixel(phone_th, x, y);
    return crc;
}

static void tap_icon(int app)
{
    int k = app % PER_PAGE;
    ph_tap((k % COLS) * CELL_W + CELL_W / 2,
           GRID_TOP + (k / COLS) * CELL_H + CELL_H / 2 - 8);
}

static twin_window_t *top_window(void)
{
    twin_window_t *top = NULL;
    for (twin_window_t *w = phone_screen()->windows; w; w = w->wm_next)
        if (w->mapped && w->style == TwinWindowApplication)
            top = w;
    return top;
}

/* Swipe from the left edge across the screen: the mobile back gesture */
static void edge_swipe(void)
{
    th_press(phone_th, 2, CY(120));
    for (twin_coord_t x = 30; x <= 210; x += 30) {
        th_move(phone_th, x, CY(120));
        th_advance(phone_th, 16);
    }
    th_release(phone_th, 210, CY(120));
    th_settle(phone_th, 2000);
}

void test_demos(void)
{
    int first = -1, count = 0;
    for (int i = 0; i < phone_app_count(); i++)
        if (phone_app_at(i)->demo) {
            if (first < 0)
                first = i;
            count++;
        }
    if (!count)
        return;

    uint32_t home_nav = nav_crc();
    /* The demos follow the phone apps: page to the first one */
    for (int p = 0; p < first / PER_PAGE; p++)
        ph_drag(200, 120, 20, 120);
    /* The page lists whichever apps are built: the golden frame holds the
     * default set */
#if defined(CONFIG_DEMO_CLOCK) && defined(CONFIG_DEMO_CALCULATOR) && \
    defined(CONFIG_DEMO_SPLINE) && defined(CONFIG_DEMO_ANIMATION) && \
    defined(CONFIG_DEMO_IMAGE) && defined(CONFIG_DEMO_MULTI)
    ph_shot("home-demos");
#else
    ph_check("home-demos");
#endif

    size_t base = 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass)
            base = th_heap_net(phone_th); /* first pass warms the caches */
        for (int i = first; i < phone_app_count(); i++) {
            const char *name = phone_app_at(i)->name;
            if (i / PER_PAGE != first / PER_PAGE)
                break; /* the icons below are tested on this page only */
            tap_icon(i);
            CHECK(phone_depth() > 0, "%s opened from the launcher", name);
            CHECK(!phone_top(), "%s: its own window is on top", name);
            twin_window_t *w = top_window();
            twin_pixmap_t *p = w ? w->pixmap : NULL;
            CHECK(p && p->x == 0 && p->y == PHONE_STATUS_H &&
                      p->width == PHONE_W && p->height == PHONE_CONTENT_H,
                  "%s fills the work area", name);
            CHECK(nav_crc() != home_nav, "%s: nav bar offers Back", name);
            ph_check(name);

            /* The back gesture closes the window on top ... */
            int depth = phone_depth();
            edge_swipe();
            CHECK_EQ(phone_depth(), depth - 1, name);
            /* ... and the nav bar's Back closes the rest */
            for (int guard = 0; phone_depth() > 0 && guard < 16; guard++) {
                phone_back();
                th_settle(phone_th, 2000);
            }
            CHECK_EQ(phone_depth(), 0, name);
            CHECK(nav_crc() == home_nav, "%s: nav bar back to home", name);
            CHECK_EQ(th_damage_check(phone_th), 0, name);
        }
    }
#if defined(CONFIG_MEMORY_STATS)
    CHECK(th_heap_net(phone_th) <= base, "demos leak (%zu > %zu)",
          th_heap_net(phone_th), base);
#else
    (void) base;
#endif

    /* Home also closes every window a demo opened */
    phone_open(first);
    th_settle(phone_th, 2000);
    ph_home();

    for (int p = 0; p < first / PER_PAGE; p++)
        ph_drag(20, 120, 200, 120);
}
