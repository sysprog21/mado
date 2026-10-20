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

bool app_home_icon_rect(int app, ui_rect_t *out);

static uint32_t nav_crc(void)
{
    return th_crc_rect(
        phone_th, (twin_rect_t) {0, PHONE_W, PHONE_H - PHONE_NAV_H, PHONE_H});
}

/* Tap @app's icon; false when it is not on the page shown */
static bool tap_icon(int app)
{
    ui_rect_t r;
    if (!app_home_icon_rect(app, &r))
        return false;
    ph_tap((r.left + r.right) / 2, (r.top + r.bottom) / 2);
    return true;
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
    ui_rect_t r;
    int pages = 0;
    while (!app_home_icon_rect(first, &r) && pages < 8) {
        ph_drag(200, 120, 20, 120);
        pages++;
    }
    /* Pressing tints the whole tile, so the whole tile is repainted: what
     * the press drew matches a repaint of the entire view.  Press a corner
     * outside the icon, then drag straight down to cancel. */
    if (app_home_icon_rect(first, &r)) {
        /* A margin past the tile catches damage that stops short of it */
        twin_rect_t tile = {r.left - 6, r.right + 6, CY(r.top - 6),
                            CY(r.bottom + 6)};
        th_press(phone_th, r.left + 3, CY(r.bottom - 3));
        ph_wait(50);
        uint32_t pressed = th_crc_rect(phone_th, tile);
        ui_view_invalidate_all(phone_top());
        ph_wait(50);
        CHECK_EQ(th_crc_rect(phone_th, tile), pressed,
                 "pressed tile repainted whole");
        th_move(phone_th, r.left + 3, CY(r.bottom + 40));
        th_release(phone_th, r.left + 3, CY(r.bottom + 40));
        ph_wait(300);
    }
    /* The page lists whichever apps are built: the golden frame holds the
     * default set */
#if defined(CONFIG_DEMO_CLOCK) && defined(CONFIG_DEMO_CALCULATOR) && \
    defined(CONFIG_DEMO_SPLINE) && defined(CONFIG_DEMO_ANIMATION) && \
    defined(CONFIG_DEMO_IMAGE)
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
            if (!tap_icon(i))
                break; /* the icons below are tested on this page only */
            CHECK(phone_depth() > 0, "%s opened from the launcher", name);
            CHECK(!phone_top(), "%s: its own window is on top", name);
            twin_window_t *w = th_top_app(phone_screen(), NULL);
            twin_pixmap_t *p = w ? w->pixmap : NULL;
            CHECK(p && p->x == 0 && p->y == PHONE_STATUS_H &&
                      p->width == PHONE_W && p->height == PHONE_CONTENT_H,
                  "%s fills the work area", name);
            CHECK(nav_crc() != home_nav, "%s: nav bar offers Back", name);
            ph_check(name);

            /* An on-screen keyboard shrinks the work area: the window
             * follows and repaints at the new size, then grows back */
            for (int k = 0; k < 2; k++) {
                twin_coord_t inset = k ? 0 : 96;
                twin_screen_set_inset(phone_screen(), inset);
                ph_wait(500);
                p = w ? w->pixmap : NULL;
                CHECK(p && p->height == PHONE_CONTENT_H - inset,
                      "%s: height with inset %d", name, inset);
                CHECK(p && th_painted(phone_th, p), "%s: painted with inset %d",
                      name, inset);
                CHECK_EQ(th_damage_check(phone_th), 0, name);
            }

            /* The back gesture closes the window on top ... */
            int depth = phone_depth();
            ph_drag(2, 120, 210, 120); /* swipe from the left edge */
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

    while (pages-- > 0)
        ph_drag(20, 120, 200, 120);
}
