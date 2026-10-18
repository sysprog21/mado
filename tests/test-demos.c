/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Every app in the registry (apps.h) under the window manager this build
 * has, which is one the app runs under: the registry lists no other.  Each
 * app is started on a fresh screen, run for a while, poked, and then
 * closed window by window the way a user would (Back under the mobile
 * policy, close otherwise).  Its windows must stay where the policy can
 * reach them, frames must match a full recomposite, and closing must return
 * every byte.
 */

#include "apps.h"
#include "harness.h"

#if defined(CONFIG_WM_MOBILE)
#define SCENE "mobile"
#define W 240
#define H 320
#elif defined(CONFIG_WM_DESKTOP)
#define SCENE "desktop"
#define W 640
#define H 480
#else
#define SCENE "none"
#define W 640
#define H 480
#endif

/* Where the policy must keep each window: inside the screen, and covering
 * the work area exactly under the policies that do not place windows */
static void check_placement(th_t *t, const char *what)
{
    twin_rect_t area = twin_screen_work_area(t->screen);
#if defined(CONFIG_WM_DESKTOP)
    const bool fills = false;
#else
    const bool fills = true;
#endif
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
            CHECK(p->x >= 0 && p->y >= 0 && p->x + p->width <= t->width &&
                      p->y + p->height <= t->height,
                  "%s: window on screen (%d,%d %dx%d)", what, p->x, p->y,
                  p->width, p->height);
        }
    }
}

static void run(const apps_entry_t *d)
{
    char what[96];
    snprintf(what, sizeof(what), "%s under %s", d->name, SCENE);

    th_t *t = th_create(W, H);
    th_settle(t, 500);
    /* First pass warms the glyph and coverage caches; the second measures */
    size_t base = 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass)
            base = th_heap_net(t);
        CHECK(d->start(t->screen, d->name, d->x, d->y, d->w, d->h),
              "%s: started", what);
        th_settle(t, 2000);
        int n;
        CHECK(th_top_app(t->screen, &n) && n > 0, "%s: opened a window", what);
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
            twin_window_t *top = th_top_app(t->screen, NULL);
            if (!top)
                break;
#if defined(CONFIG_WM_MOBILE)
            twin_window_back(top);
#else
            twin_window_close(top);
#endif
            th_settle(t, 2000);
            twin_window_t *next = th_top_app(t->screen, NULL);
            if (next && next->pixmap && _twin_window_shown(next))
                CHECK(th_painted(t, next->pixmap),
                      "%s: revealed window painted", what);
        }
        CHECK(!th_top_app(t->screen, &n), "%s: every window closed (%d left)",
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

#if defined(CONFIG_MEMORY_STATS)
/* Out of memory from the first allocation: the start reports failure and
 * leaves neither a window nor a byte behind.  (Failures deeper in, while
 * text and paths are painted, are not handled by the core yet.) */
static void run_out_of_memory(const apps_entry_t *d)
{
    th_t *t = th_create(W, H);
    th_settle(t, 500);
    size_t base = th_heap_net(t);
    _twin_alloc_fail_next = 1;
    bool ok = d->start(t->screen, d->name, d->x, d->y, d->w, d->h);
    _twin_alloc_fail_next = 0;
    int n;
    th_top_app(t->screen, &n);
    /* An app of several windows may still open the others */
    CHECK(ok || !n, "%s: failed start left %d windows", d->name, n);
    th_settle(t, 500);
    for (twin_window_t *w; (w = th_top_app(t->screen, NULL));)
        twin_window_destroy(w);
    th_settle(t, 500);
    CHECK(th_heap_net(t) <= base, "%s: failed start leaks (%zu > %zu)", d->name,
          th_heap_net(t), base);
    th_destroy(t);
}
#endif

int main(void)
{
    for (int i = 0; i < apps_count(); i++) {
        const apps_entry_t *d = apps_at(i);
        /* Shells own the screen and have suites of their own */
        if (d->boot)
            continue;
        run(d);
#if defined(CONFIG_MEMORY_STATS)
        run_out_of_memory(d);
#endif
    }
    twin_cache_flush();
    return th_report("demos");
}
