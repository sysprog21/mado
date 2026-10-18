/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Every app in the registry (apps.h) under each window manager it is marked
 * for that this build has: the built policy (desktop or mobile) and 'none'.
 * Each app is started on a fresh screen, run for a while, poked, and then
 * closed window by window the way a user would (Back under the mobile
 * policy, close otherwise).  Its windows must stay where the policy can
 * reach them, frames must match a full recomposite, and closing must return
 * every byte.
 */

#include "apps.h"
#include "harness.h"

#if defined(CONFIG_WM_MOBILE)
#define BUILT_WM TwinWmMobile
#define W 240
#define H 320
#elif defined(CONFIG_WM_DESKTOP)
#define BUILT_WM TwinWmDesktop
#define W 640
#define H 480
#else
#define W 640
#define H 480
#endif

/* Topmost shown application window, and how many there are */
static twin_window_t *top_app(twin_screen_t *screen, int *count)
{
    twin_window_t *top = NULL;
    int n = 0;
    for (twin_window_t *w = screen->windows; w; w = w->wm_next)
        if (w->mapped && w->style == TwinWindowApplication) {
            top = w;
            n++;
        }
    if (count)
        *count = n;
    return top;
}

/* Where the policy must keep each window: inside the screen, and covering
 * the work area exactly under the policies that do not place windows */
static void check_placement(th_t *t, const char *what)
{
    twin_rect_t area = twin_screen_work_area(t->screen);
    bool fills = t->screen->wm->mode != TwinWmDesktop;
    for (twin_window_t *w = t->screen->windows; w; w = w->wm_next) {
        /* Windows a policy keeps hidden (covered on a phone) do not count */
        if (w->style != TwinWindowApplication || !_twin_window_shown(w))
            continue;
        twin_pixmap_t *p = w->pixmap;
        if (fills) {
            CHECK(p->x == area.left && p->y == area.top &&
                      p->width == area.right - area.left &&
                      p->height == area.bottom - area.top,
                  "%s: window covers the work area (%d,%d %dx%d)", what, p->x,
                  p->y, p->width, p->height);
        } else {
            CHECK(p->x >= 0 && p->y >= 0 && p->x < t->width && p->y < t->height,
                  "%s: window on screen (%d,%d)", what, p->x, p->y);
        }
    }
}

/* A window the policy just revealed was painted again: a released
 * (evicted) window that nobody repainted comes back as one flat colour */
static bool painted(th_t *t, const twin_window_t *w)
{
    twin_pixmap_t *p = w->pixmap;
    twin_argb32_t first = th_pixel(t, p->x, p->y);
    for (twin_coord_t y = p->y; y < p->y + p->height && y < t->height; y++)
        for (twin_coord_t x = p->x; x < p->x + p->width && x < t->width; x++)
            if (th_pixel(t, x, y) != first)
                return true;
    return false;
}

static void run(const apps_entry_t *d, twin_wm_mode_t mode, const char *scene)
{
    char what[96];
    snprintf(what, sizeof(what), "%s under %s", d->name, scene);

    th_t *t = th_create(W, H, mode);
    th_settle(t, 500);
    /* First pass warms the glyph and coverage caches; the second measures */
    size_t base = 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass)
            base = th_heap_net(t);
        CHECK(d->start(t->screen, d->x, d->y, d->w, d->h), "%s: started", what);
        th_settle(t, 2000);
        int n;
        CHECK(top_app(t->screen, &n) && n > 0, "%s: opened a window", what);
        check_placement(t, what);
        CHECK_EQ(th_damage_check(t), 0, what);

        /* Let timers run (clock hands, animation frames), then poke it */
        th_advance(t, 1500);
        th_settle(t, 1000);
        th_tap(t, W / 2, H / 2);
        th_settle(t, 1000);
        CHECK_EQ(th_damage_check(t), 0, what);

        /* Close what the demo opened, one window at a time */
        for (int guard = 0; guard < 16; guard++) {
            twin_window_t *top = top_app(t->screen, NULL);
            if (!top)
                break;
            if (mode == TwinWmMobile)
                twin_window_back(top);
            else
                twin_window_close(top);
            th_settle(t, 2000);
            twin_window_t *next = top_app(t->screen, NULL);
            if (next && next->pixmap && _twin_window_shown(next))
                CHECK(painted(t, next), "%s: revealed window painted", what);
        }
        CHECK(!top_app(t->screen, &n), "%s: every window closed (%d left)",
              what, n);
        CHECK_EQ(th_damage_check(t), 0, what);
    }
#if defined(CONFIG_MEMORY_STATS)
    CHECK(th_heap_net(t) <= base, "%s: closing leaks (%zu > %zu)", what,
          th_heap_net(t), base);
#else
    (void) base;
#endif
    th_destroy(t);
}

int main(void)
{
    for (int i = 0; i < apps_count(); i++) {
        const apps_entry_t *d = apps_at(i);
        /* Shells own the screen and have suites of their own */
        if (d->shell)
            continue;
#if defined(BUILT_WM)
        if (apps_runs_under(d, BUILT_WM))
            run(d, BUILT_WM, BUILT_WM == TwinWmMobile ? "mobile" : "desktop");
#endif
        if (apps_runs_under(d, TwinWmNone))
            run(d, TwinWmNone, "none");
    }
    twin_cache_flush();
    return th_report("demos");
}
