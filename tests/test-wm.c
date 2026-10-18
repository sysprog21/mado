/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Window manager tests: placement and input under each policy, mobile
 * transitions and eviction, and switching policies at run time.  Every step
 * checks that incremental scan-out matches a from-scratch recomposite.
 */

#include <stdlib.h>
#include <string.h>

#include "harness.h"

#define W 240
#define H 320
#define TOP_BAR 20
#define BOTTOM_BAR 32

typedef struct {
    twin_argb32_t color;
    int draws, closes, presses, destroyed, keys, ups, backs;
    bool refuse_close;
    bool handle_back;  /* consume TwinEventBack (in-app history) */
    bool pass_presses; /* leave presses to the window manager */
    twin_coord_t last_x, last_y;
} app_t;

static void app_draw(twin_window_t *w)
{
    app_t *a = w->client_data;
    a->draws++;
    twin_coord_t cw = w->client.right - w->client.left;
    twin_coord_t ch = w->client.bottom - w->client.top;
    twin_fill(w->pixmap, a->color, TWIN_SOURCE, 0, 0, cw, ch);
    twin_fill(w->pixmap, 0xff000000, TWIN_SOURCE, 4, 4, 14, 14);
}

static bool app_event(twin_window_t *w, twin_event_t *ev)
{
    app_t *a = w->client_data;
    switch (ev->kind) {
    case TwinEventClose:
        a->closes++;
        return a->refuse_close;
    case TwinEventBack:
        a->backs++;
        return a->handle_back;
    case TwinEventButtonDown:
        a->presses++;
        a->last_x = ev->u.pointer.x;
        a->last_y = ev->u.pointer.y;
        return !a->pass_presses;
    case TwinEventButtonUp:
        a->ups++;
        return false;
    case TwinEventUcs4:
        if (ev->u.ucs4.ucs4 == 27)
            return false; /* Escape is left to the window manager */
        a->keys++;
        return true;
    default:
        return false;
    }
}

static void app_destroy(twin_window_t *w)
{
    app_t *a = w->client_data;
    a->destroyed++;
}

static twin_window_t *mk(th_t *t,
                         app_t *a,
                         twin_window_style_t style,
                         twin_coord_t x,
                         twin_coord_t y,
                         twin_coord_t w,
                         twin_coord_t h)
{
    twin_window_t *win =
        twin_window_create(t->screen, TWIN_ARGB32, style, x, y, w, h);
    win->client_data = a;
    win->draw = app_draw;
    win->event = app_event;
    win->destroy = app_destroy;
    twin_window_set_opaque(win, true);
    twin_window_show(win);
    twin_window_queue_paint(win);
    return win;
}

static bool shown(const twin_window_t *w)
{
    return w->pixmap && w->pixmap->screen && !_twin_pixmap_is_stub(w->pixmap);
}

#define CONSISTENT(t, what) CHECK_EQ(th_damage_check(t), 0, what)

static void test_none(void)
{
    th_t *t = th_create(W, H, TwinWmNone);
    app_t a = {.color = 0xffcc3333}, p = {.color = 0xff3333cc};
    twin_window_t *app = mk(t, &a, TwinWindowApplication, 30, 40, 100, 100);
    twin_window_t *pop = mk(t, &p, TwinWindowPlain, 30, 40, 50, 60);
    th_frame(t);

    CHECK(app->pixmap->x == 0 && app->pixmap->width == W &&
              app->pixmap->height == H,
          "application covers the screen without a WM");
    CHECK(pop->pixmap->x == 30 && pop->pixmap->y == 40 &&
              pop->pixmap->width == 50,
          "plain window maps where requested");
    CHECK(app->client.top == 0, "no decorations");
    CHECK(th_pixel(t, 100, 100) == 0xffcc3333, "app pixels on screen");
    CHECK(t->screen->active == pop->pixmap,
          "the newest mapped window takes the keyboard");

    th_tap(t, 35, 45);
    CHECK(p.presses == 1 && p.last_x == 5 && p.last_y == 5,
          "plain window gets client coordinates (%d,%d)", p.last_x, p.last_y);
    th_tap(t, 200, 300);
    CHECK(a.presses == 1 && a.last_x == 200, "app gets its own presses");
    CHECK(t->screen->active == app->pixmap, "press focuses the window");

    twin_window_close(pop);
    CHECK(p.closes == 1 && p.destroyed == 1, "close destroys by default");
    th_frame(t);
    CONSISTENT(t, "none: after close");
    twin_window_destroy(app);
    th_destroy(t);
}

static void test_desktop(void)
{
#if defined(CONFIG_WM_DESKTOP)
    th_t *t = th_create(W, H, TwinWmDesktop);
    app_t a = {.color = 0xffcc3333}, b = {.color = 0xff33cc33};
    twin_window_t *wa = mk(t, &a, TwinWindowApplication, 10, 10, 120, 100);
    twin_window_t *wb = mk(t, &b, TwinWindowApplication, 60, 80, 120, 100);
    th_frame(t);

    CHECK(wa->client.top > 0, "desktop decorates application windows");
    CHECK(wa->pixmap->x == 10 && wa->pixmap->y == 10, "frame at request");
    CHECK(wa->pixmap->height == 100 + wa->client.top, "title bar added");
    CHECK(wb->active && !wa->active, "last shown window is active");
#if defined(CONFIG_DROP_SHADOW)
    CHECK(wb->pixmap->shadow.blur && !wa->pixmap->shadow.blur,
          "only the active window casts a shadow");
#endif
    CONSISTENT(t, "desktop: initial");

    /* Click in a's client area raises and activates it */
    th_tap(t, 20, 10 + wa->client.top + 10);
    CHECK(a.presses == 1 && a.last_x == 10 && a.last_y == 10,
          "client coordinates exclude the title (%d,%d)", a.last_x, a.last_y);
    CHECK(wa->active && !wb->active, "click activates");
    CHECK(t->screen->top == wa->pixmap, "click raises");
    CONSISTENT(t, "desktop: raise");

    /* Drag by the title bar */
    th_drag(t, 50, 15, 70, 45, 4);
    CHECK(wa->pixmap->x == 30 && wa->pixmap->y == 40,
          "title drag moves the window (%d,%d)", wa->pixmap->x, wa->pixmap->y);
    CONSISTENT(t, "desktop: drag");

    /* Refused close keeps the window */
    a.refuse_close = true;
    twin_window_close(wa);
    CHECK(a.closes == 1 && a.destroyed == 0, "refused close keeps the window");
    a.refuse_close = false;
    twin_window_close(wa);
    CHECK(a.destroyed == 1, "accepted close destroys");
    th_frame(t);
    CHECK(wb->active, "focus moves to the remaining window");
    CONSISTENT(t, "desktop: close");
    twin_window_destroy(wb);
    th_destroy(t);
#endif
}

#if defined(CONFIG_WM_DESKTOP)
/* Tap along @w's title bar from the right edge leftwards until @found holds;
 * returns the x that did it, or -1.  Close requests must be refused. */
static twin_coord_t tap_title_until(th_t *t,
                                    twin_window_t *w,
                                    bool (*found)(const twin_window_t *))
{
    twin_coord_t left = w->pixmap->x, y = w->pixmap->y + w->client.top / 2;
    for (twin_coord_t x = left + w->pixmap->width - 2; x > left; x--) {
        th_advance(t, 400); /* never a double-click */
        th_tap(t, x, y);
        if (found(w))
            return x;
    }
    return -1;
}

static bool is_iconified(const twin_window_t *w)
{
    return w->pixmap->scan_height < w->pixmap->height;
}

static bool is_restored(const twin_window_t *w)
{
    return w->pixmap->scan_height == w->pixmap->height;
}
#endif

/* Iconify shows only the title bar: the rest neither scans out nor takes
 * the pointer, and restoring brings the retained pixels back */
static void test_desktop_iconify(void)
{
#if defined(CONFIG_WM_DESKTOP)
    th_t *t = th_create(W, H, TwinWmDesktop);
    app_t a = {.color = 0xffcc3333, .refuse_close = true};
    app_t b = {.color = 0xff33cc33, .refuse_close = true};
    /* Lower and offset, so a's title bar stays clear when b is raised */
    twin_window_t *wb = mk(t, &b, TwinWindowApplication, 20, 60, 160, 140);
    twin_window_t *wa = mk(t, &a, TwinWindowApplication, 20, 30, 160, 140);
    th_frame(t);
    twin_coord_t below_y = wa->pixmap->y + wa->client.top + 40;
    CHECK(th_pixel(t, 100, below_y) == 0xffcc3333, "top window covers");

    CHECK(tap_title_until(t, wa, is_iconified) > 0, "iconify button found");
    th_frame(t);
    CHECK_EQ(wa->pixmap->scan_height, wa->client.top,
             "iconified down to the title bar");
    CHECK(th_pixel(t, 100, below_y) == 0xff33cc33,
          "window below shows under an iconified one");
    CONSISTENT(t, "desktop: iconified");

    int presses = b.presses;
    th_tap(t, 100, below_y);
    CHECK_EQ(b.presses, presses + 1, "click below the title falls through");
    CHECK(t->screen->top == wb->pixmap, "and raises the window below");

    /* Bring the iconified window back on top, then restore it */
    th_tap(t, wa->pixmap->x + 30, wa->pixmap->y + wa->client.top / 2);
    CHECK(tap_title_until(t, wa, is_restored) > 0, "restore button found");
    th_frame(t);
    CHECK(th_pixel(t, 100, below_y) == 0xffcc3333, "restored window covers");
    presses = a.presses;
    th_tap(t, 100, below_y);
    CHECK_EQ(a.presses, presses + 1, "restored client takes the click");
    CONSISTENT(t, "desktop: restored");

    twin_window_destroy(wa);
    twin_window_destroy(wb);
    th_destroy(t);
#endif
}

/*
 * Restoring an evicted surface keeps everything that describes the window
 * on screen: focus, pointer grab, opacity, shadow, the shown rows and the
 * update-disable count all move to the fresh pixmap.
 */
static void test_restore_handoff(void)
{
    th_t *t = th_create(W, H, TwinWmNone);
    app_t a = {.color = 0xffcc3333};
    twin_window_t *w = mk(t, &a, TwinWindowPlain, 20, 20, 100, 80);
    th_frame(t);

    _twin_window_evict(w);
    twin_pixmap_t *stub = w->pixmap;
    CHECK(_twin_pixmap_is_stub(stub) && stub->screen,
          "evicted surface keeps its place in the stack");
    t->screen->active = stub;
    t->screen->target = stub;
    twin_pixmap_set_opacity(stub, 0x80);
    twin_pixmap_set_shadow(stub, 2, 3, 6, 0x90);
    stub->scan_height = 50;
    twin_pixmap_disable_update(stub);

    CHECK(_twin_window_restore(w), "restore");
    twin_pixmap_t *p = w->pixmap;
    CHECK(!_twin_pixmap_is_stub(p) && p->screen, "restored and shown");
    CHECK(t->screen->active == p, "focus follows the restored surface");
    CHECK(t->screen->target == p, "pointer target follows");
    CHECK_EQ(p->opacity, 0x80, "opacity carried");
    CHECK_EQ(p->shadow.blur, 6, "shadow carried");
    CHECK_EQ(p->scan_height, 50, "shown rows carried");
    CHECK_EQ(p->disable, 1, "update disable carried");
    twin_pixmap_enable_update(p);
    t->screen->target = NULL;
    t->screen->clicklock = false;
    th_frame(t);
    CONSISTENT(t, "restored surface consistent");

    twin_window_destroy(w);
    th_destroy(t);
}

#if defined(CONFIG_WM_MOBILE)
typedef struct {
    th_t *t;
    app_t top, bottom;
    twin_window_t *bar_top, *bar_bottom;
} phone_t;

static void phone_init(phone_t *ph)
{
    memset(ph, 0, sizeof(*ph));
    ph->t = th_create(W, H, TwinWmMobile);
    ph->top.color = 0xff202020;
    ph->bottom.color = 0xff404040;
    ph->bar_top = mk(ph->t, &ph->top, TwinWindowPlain, 0, 0, W, TOP_BAR);
    ph->bar_bottom =
        mk(ph->t, &ph->bottom, TwinWindowPlain, 0, 0, W, BOTTOM_BAR);
    twin_screen_set_bars(ph->t->screen, ph->bar_top, ph->bar_bottom);
    th_frame(ph->t);
}

static void phone_fini(phone_t *ph)
{
    twin_window_destroy(ph->bar_top);
    twin_window_destroy(ph->bar_bottom);
    th_destroy(ph->t);
}

static twin_window_t *launch(phone_t *ph, app_t *a)
{
    twin_rect_t work = twin_screen_work_area(ph->t->screen);
    return mk(ph->t, a, TwinWindowApplication, work.left, work.top,
              work.right - work.left, work.bottom - work.top);
}

static void test_mobile_layout(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    twin_rect_t work = twin_screen_work_area(t->screen);
    CHECK(work.top == TOP_BAR && work.bottom == H - BOTTOM_BAR,
          "work area between the bars");
    CHECK(ph.bar_bottom->pixmap->y == H - BOTTOM_BAR, "bottom bar pinned");

    app_t a = {.color = 0xffcc3333};
    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
    CHECK(wa->pixmap->y == TOP_BAR &&
              wa->pixmap->height == H - TOP_BAR - BOTTOM_BAR,
          "app fills the work area");
    CHECK(th_pixel(t, 100, 100) == 0xffcc3333, "app visible");
    CHECK(th_pixel(t, 100, 5) == 0xff202020, "status bar above it");
    CHECK(t->screen->active == wa->pixmap, "top app has focus");

    th_tap(t, 30, TOP_BAR + 30);
    CHECK(a.presses == 1 && a.last_y == 30, "app gets work-area coordinates");
    CONSISTENT(t, "mobile: layout");
    twin_window_destroy(wa);
    phone_fini(&ph);
}

static void test_mobile_push_pop(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t a = {.color = 0xffcc3333}, b = {.color = 0xff33cc33};

    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
#if defined(CONFIG_WM_MOBILE_EVICT)
    size_t one_app = th_heap_net(t);
#endif

    twin_window_t *wb = launch(&ph, &b);
    th_advance(t, 64);
    CHECK(wb->pixmap->x > 0 && wb->pixmap->x < W,
          "push: incoming app mid-slide (x=%d)", wb->pixmap->x);
    CHECK(wa->pixmap->x < 0, "push: outgoing app parallaxes left");
    CHECK(wa->pixmap->opacity < 0xff, "push: outgoing app dims");
#if defined(CONFIG_DROP_SHADOW)
    CHECK(wb->pixmap->shadow.blur > 0, "push: incoming app casts a shadow");
#endif
    CONSISTENT(t, "mobile: mid push");

    /* Taps are ignored while the transition runs */
    int presses = b.presses;
    th_tap(t, 200, 150);
    CHECK_EQ(b.presses, presses, "no input during transitions");

    th_settle(t, 1000);
    CHECK(shown(wb) && wb->pixmap->x == 0, "push settles at the origin");
    CHECK(!wa->pixmap->screen, "covered app hidden");
#if defined(CONFIG_WM_MOBILE_EVICT)
    CHECK(_twin_pixmap_is_stub(wa->pixmap), "covered app evicted");
    size_t two_apps = th_heap_net(t);
    CHECK(two_apps < one_app + 4096, "eviction keeps one surface (%zu -> %zu)",
          one_app, two_apps);
#endif
    CHECK(th_pixel(t, 100, 100) == 0xff33cc33, "new app on screen");
    CONSISTENT(t, "mobile: after push");

    /* Pop: destroy the top app; its pixels leave while 'a' repaints */
    int draws = a.draws;
    twin_window_destroy(wb);
    CHECK_EQ(b.destroyed, 1, "app state freed immediately");
    th_advance(t, 64);
#if defined(CONFIG_WM_MOBILE_EVICT)
    CHECK(a.draws > draws, "revealed app repainted after eviction");
#else
    CHECK_EQ(a.draws, draws, "retained app revealed without a repaint");
#endif
    CHECK(wa->pixmap->x < 0 && wa->pixmap->x > -W, "pop: revealed app slides");
    CONSISTENT(t, "mobile: mid pop");
    th_settle(t, 1000);
    CHECK(shown(wa) && wa->pixmap->x == 0 && wa->pixmap->opacity == 0xff,
          "pop settles");
    CHECK(th_pixel(t, 100, 100) == 0xffcc3333, "previous app back");
    CHECK(t->screen->active == wa->pixmap, "focus returns");
    CONSISTENT(t, "mobile: after pop");

    twin_window_destroy(wa);
    th_settle(t, 1000);
    phone_fini(&ph);
}

static void test_mobile_swipe(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t a = {.color = 0xffcc3333}, b = {.color = 0xff33cc33};
    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
    twin_window_t *wb = launch(&ph, &b);
    th_settle(t, 1000);

    /* Short swipe: springs back */
    th_press(t, 3, 150);
    th_move(t, 30, 150);
    CHECK(wb->pixmap->x == 27, "swipe tracks the finger (x=%d)", wb->pixmap->x);
    CHECK(shown(wa), "app below revealed during the swipe");
    CONSISTENT(t, "mobile: mid swipe");
    th_release(t, 30, 150);
    th_settle(t, 1000);
    CHECK(b.closes == 0 && shown(wb) && wb->pixmap->x == 0,
          "short swipe cancels");
    CHECK(b.presses == 0, "edge swipe never reached the app");

    /* Long swipe closes the top app */
    th_drag(t, 3, 150, 180, 150, 8);
    th_settle(t, 1000);
    CHECK(b.closes == 1 && b.destroyed == 1, "long swipe closes");
    CHECK(shown(wa) && wa->pixmap->x == 0, "swipe pop settles");
    CONSISTENT(t, "mobile: after swipe");

    /* An app that refuses to close slides back */
    app_t c = {.color = 0xff3333cc, .refuse_close = true};
    twin_window_t *wc = launch(&ph, &c);
    th_settle(t, 1000);
    th_drag(t, 3, 150, 200, 150, 8);
    th_settle(t, 1000);
    CHECK(c.closes == 1 && c.destroyed == 0 && shown(wc) && wc->pixmap->x == 0,
          "refused swipe-close springs back");
    CONSISTENT(t, "mobile: refused swipe");

    /* Single app: no swipe, the press reaches the app */
    twin_window_destroy(wc);
    th_settle(t, 1000);
    th_tap(t, 3, 150);
    CHECK(a.presses == 1, "edge press reaches a lone app");
    twin_window_destroy(wa);
    phone_fini(&ph);
}

static void test_mobile_fullscreen(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t home = {.color = 0xffcccccc}, lock = {.color = 0xff000040};
    twin_window_t *wh = launch(&ph, &home);
    twin_window_t *wl = mk(t, &lock, TwinWindowFullscreen, 0, 0, W, H);
    th_settle(t, 1000);
    CHECK(wl->pixmap->y == 0 && wl->pixmap->height == H, "lock covers all");
    CHECK(th_pixel(t, 100, 5) == 0xff000040, "lock above the status bar");
    CHECK(t->screen->active == wl->pixmap, "lock has focus");
    CHECK(!wh->pixmap->screen && !ph.bar_top->pixmap->screen,
          "app and bars hidden under an opaque full-screen window");
#if defined(CONFIG_WM_MOBILE_EVICT)
    CHECK(_twin_pixmap_is_stub(wh->pixmap), "covered app released");
#endif
    CONSISTENT(t, "mobile: locked");
    th_tap(t, 3, 150);
    CHECK(lock.presses == 1 && home.presses == 0, "no edge swipe under lock");

    twin_window_destroy(wl);
    th_advance(t, 80);
    CONSISTENT(t, "mobile: mid dismiss");
    th_settle(t, 1000);
    CHECK(th_pixel(t, 100, 100) == 0xffcccccc, "home after unlock");
    CHECK(shown(ph.bar_top) && shown(ph.bar_bottom), "bars back after unlock");
    CHECK(t->screen->active == wh->pixmap, "home focused");
    twin_window_destroy(wh);
    phone_fini(&ph);
}

/* Popping while an opaque full-screen window covers everything must not
 * link the departing pixels above a released (off-screen) window */
static void test_mobile_pop_under_fullscreen(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t a = {.color = 0xffaa0000}, b = {.color = 0xff00aa00},
          f = {.color = 0xff0000aa};
    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
    twin_window_t *wb = launch(&ph, &b);
    th_settle(t, 1000);
    twin_window_t *wf = mk(t, &f, TwinWindowFullscreen, 0, 0, W, H);
    twin_window_set_opaque(wf, false);
    th_settle(t, 1000);
    twin_window_set_opaque(wf, true); /* now covers everything */
    twin_window_destroy(wb);
    bool reachable = false;
    for (twin_pixmap_t *p = t->screen->top; p; p = p->down)
        if (p == wf->pixmap)
            reachable = true;
    CHECK(reachable, "full-screen window still in the display list");
    th_settle(t, 1000);
    CONSISTENT(t, "pop under full-screen");
    twin_window_hide(wf);
    th_settle(t, 1000);
    CHECK(shown(wa), "app revealed after the cover goes away");
    CONSISTENT(t, "uncovered after pop");

    /* Style change of a mapped window re-runs the policy */
    twin_window_configure(wa, TwinWindowFullscreen, 0, 0, W, H);
    th_settle(t, 1000);
    CHECK(wa->pixmap->y == 0 && wa->pixmap->height == H,
          "restyled window covers the screen");
    CONSISTENT(t, "after restyle");
    twin_window_destroy(wf);
    twin_window_destroy(wa);
    th_settle(t, 1000);
    phone_fini(&ph);
}

static void test_mobile_memory(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t apps[6];
    twin_window_t *win[6];
    size_t base = th_heap_net(t);

    for (int i = 0; i < 6; i++) {
        apps[i] = (app_t) {.color = 0xff000000 | (0x2a * (i + 1))};
        win[i] = launch(&ph, &apps[i]);
        th_settle(t, 1000);
    }
#if defined(CONFIG_WM_MOBILE_EVICT)
    size_t deep = th_heap_net(t);
    size_t surface = (size_t) W * (H - TOP_BAR - BOTTOM_BAR) * 4;
    CHECK(deep - base < surface + 6 * 1024,
          "six stacked apps cost one surface (%zu bytes)", deep - base);
#endif

    /* Unwind bottom-up below the top, then pop the top once */
    for (int i = 1; i < 5; i++)
        twin_window_destroy(win[i]);
    twin_window_destroy(win[5]);
    th_settle(t, 1000);
    CHECK(shown(win[0]), "back to the root");
    CONSISTENT(t, "mobile: unwound");
    twin_window_destroy(win[0]);
    th_settle(t, 1000);
    CHECK(th_heap_net(t) <= base, "no leak after the stack unwinds (%zu > %zu)",
          th_heap_net(t), base);
    phone_fini(&ph);
}
#endif /* CONFIG_WM_MOBILE */

static void test_switching(void)
{
    th_t *t = th_create(W, H, TwinWmNone);
    app_t bars[2] = {{.color = 0xff111111}, {.color = 0xff222222}};
    twin_window_t *bt = mk(t, &bars[0], TwinWindowPlain, 0, 0, W, TOP_BAR);
    twin_window_t *bb = mk(t, &bars[1], TwinWindowPlain, 0, 0, W, BOTTOM_BAR);
    twin_screen_set_bars(t->screen, bt, bb);
    th_frame(t);
    size_t base = th_heap_net(t);

    app_t apps[3] = {
        {.color = 0xffaa0000}, {.color = 0xff00aa00}, {.color = 0xff0000aa}};
    twin_window_t *w[3];
    for (int i = 0; i < 3; i++)
        w[i] = mk(t, &apps[i], TwinWindowApplication, 10, 30, 160, 200);
    th_settle(t, 1000);
    CHECK(!bt->pixmap->screen, "bars hidden without the mobile WM");
    CONSISTENT(t, "switch: none");

    const twin_wm_mode_t cycle[] = {TwinWmMobile,  TwinWmDesktop, TwinWmNone,
                                    TwinWmDesktop, TwinWmMobile,  TwinWmNone};
    for (size_t i = 0; i < sizeof(cycle) / sizeof(cycle[0]); i++) {
        twin_wm_mode_t m = cycle[i];
        bool ok = twin_screen_set_wm(t->screen, m);
#if !defined(CONFIG_WM_MOBILE)
        if (m == TwinWmMobile) {
            CHECK(!ok, "compiled-out mobile WM refused");
            continue;
        }
#endif
#if !defined(CONFIG_WM_DESKTOP)
        if (m == TwinWmDesktop) {
            CHECK(!ok, "compiled-out desktop WM refused");
            continue;
        }
#endif
        CHECK(ok, "switch to mode %d", m);
        CHECK(twin_screen_get_wm(t->screen) == m, "mode %d active", m);
        th_settle(t, 1000);
        switch (m) {
        case TwinWmMobile:
            CHECK(shown(bt) && shown(bb), "mobile shows the bars");
            CHECK(shown(w[2]) && !w[0]->pixmap->screen,
                  "mobile shows only the top app");
            CHECK(w[2]->pixmap->y == TOP_BAR, "top app in the work area");
            break;
        case TwinWmDesktop:
            CHECK(!bt->pixmap->screen, "desktop hides the bars");
            CHECK(shown(w[0]) && shown(w[1]) && shown(w[2]),
                  "desktop shows every app");
            CHECK(w[2]->client.top > 0, "desktop decorations");
            CHECK(w[1]->pixmap->x != w[0]->pixmap->x,
                  "identical requests cascade");
            CHECK(w[2]->active, "topmost app active");
            break;
        case TwinWmNone:
            CHECK(w[2]->client.top == 0 && w[2]->pixmap->width == W,
                  "none: undecorated full screen");
            CHECK(t->screen->top == w[2]->pixmap, "stacking order kept");
            break;
        }
        CONSISTENT(t, "switch: after mode change");
        CHECK(th_pixel(t, W / 2, H / 2) == 0xff0000aa || m == TwinWmDesktop,
              "top app visible after switching to %d", m);
    }

#if defined(CONFIG_WM_MOBILE)
    /* Switching in the middle of a transition */
    twin_screen_set_wm(t->screen, TwinWmMobile);
    th_settle(t, 1000);
    app_t extra = {.color = 0xffaaaa00};
    twin_window_t *we = mk(t, &extra, TwinWindowApplication, 0, 0, W, H);
    th_advance(t, 48);
    twin_screen_set_wm(t->screen, TwinWmDesktop);
    th_settle(t, 1000);
    CONSISTENT(t, "switch: mid transition");
    twin_screen_set_wm(t->screen, TwinWmMobile);
    twin_window_destroy(we);
    th_advance(t, 48);
    twin_screen_set_wm(t->screen, TwinWmNone);
    th_settle(t, 1000);
    CONSISTENT(t, "switch: mid pop");
#endif

    for (int i = 0; i < 3; i++)
        twin_window_destroy(w[i]);
    th_settle(t, 1000);
    CHECK(th_heap_net(t) <= base, "switching leaks nothing (%zu > %zu)",
          th_heap_net(t), base);
    twin_window_destroy(bt);
    twin_window_destroy(bb);
    th_destroy(t);
}

#if defined(CONFIG_WM_DESKTOP) && defined(CONFIG_WM_MOBILE)
static twin_wm_mode_t switch_to;
static bool dispatch_result;

static bool switching_event(twin_window_t *w, twin_event_t *ev)
{
    if (ev->kind == TwinEventButtonDown) {
        /* Switch while our own event is being delivered */
        dispatch_result = twin_screen_set_wm(w->screen, switch_to);
        return true;
    }
    return app_event(w, ev);
}
#endif

static void test_switch_from_handler(void)
{
#if defined(CONFIG_WM_DESKTOP) && defined(CONFIG_WM_MOBILE)
    th_t *t = th_create(W, H, TwinWmMobile);
    app_t a = {.color = 0xffaa3333};
    twin_window_t *w = mk(t, &a, TwinWindowApplication, 0, 20, W, 258);
    w->event = switching_event;
    th_settle(t, 1000);

    switch_to = TwinWmDesktop;
    th_press(t, 100, 100);
    CHECK(twin_screen_get_wm(t->screen) == TwinWmDesktop,
          "pending switch reported at once");
    th_release(t, 100, 100);
    th_settle(t, 1000);
    CHECK(t->screen->wm->mode == TwinWmDesktop,
          "switch applied after the event");
    CHECK(w->client.top > 0, "window decorated after the switch");
    CONSISTENT(t, "switch from handler");

    switch_to = TwinWmMobile;
    th_tap(t, 100, 150);
    th_settle(t, 1000);
    CHECK(t->screen->wm->mode == TwinWmMobile, "switched back from a handler");
    twin_window_destroy(w);

    /* Phone-sized screens built for the work area cascade and stay on screen */
    twin_screen_set_wm(t->screen, TwinWmDesktop);
    app_t apps[3] = {
        {.color = 0xff111111}, {.color = 0xff222222}, {.color = 0xff333333}};
    twin_window_t *ws[3];
    for (int i = 0; i < 3; i++)
        ws[i] = mk(t, &apps[i], TwinWindowApplication, 0, 20, W, 258);
    twin_screen_set_wm(t->screen, TwinWmMobile);
    twin_screen_set_wm(t->screen, TwinWmDesktop);
    th_settle(t, 1000);
    for (int i = 0; i < 3; i++) {
        twin_pixmap_t *p = ws[i]->pixmap;
        CHECK(p->x >= 0 && p->y >= 0 && p->x + p->width <= W &&
                  p->y + p->height <= H,
              "window %d on screen (%d,%d %dx%d)", i, p->x, p->y, p->width,
              p->height);
    }
    CHECK(ws[0]->pixmap->y != ws[1]->pixmap->y, "second window cascaded");
    CONSISTENT(t, "cascade on a small screen");
    for (int i = 0; i < 3; i++)
        twin_window_destroy(ws[i]);
    th_destroy(t);
#endif
}


/* Without a window manager, hiding the focused window hands the keyboard
 * to the topmost window still shown */
static int self_destroyed;

/* Destroys its own window from the event named by the app's color */
static bool destroy_on(twin_window_t *w, twin_event_t *ev)
{
    app_t *a = w->client_data;
    if ((twin_event_kind_t) a->color == ev->kind) {
        self_destroyed++;
        twin_window_destroy(w);
        return false;
    }
    return false;
}

/* Windows destroyed from their own callbacks while the screen is busy are
 * freed only once it is idle; nothing touches them after */
static void test_destroy_in_callbacks(void)
{
    th_t *t = th_create(W, H, TwinWmNone);
    app_t a = {.color = 0xffcc3333};
    app_t b = {.color = TwinEventActivate};
    twin_window_t *wa = mk(t, &a, TwinWindowPlain, 0, 0, 100, 100);
    twin_window_t *wb = mk(t, &b, TwinWindowPlain, 120, 120, 100, 100);
    wb->draw = NULL;
    wb->destroy = NULL;
    twin_screen_set_active(t->screen, wa->pixmap);
    wb->event = destroy_on;
    self_destroyed = 0;
    size_t before = th_heap_net(t);
    th_tap(t, 150, 150); /* focusing b runs its Activate handler */
    CHECK_EQ(self_destroyed, 1, "destroyed from Activate");
    /* A Close handler that destroys the window itself */
    app_t e = {.color = TwinEventClose};
    twin_window_t *we = mk(t, &e, TwinWindowPlain, 50, 50, 40, 40);
    we->event = destroy_on;
    we->destroy = NULL;
    twin_window_close(we);
    CHECK_EQ(self_destroyed, 2, "destroyed from Close, once");
    CHECK(t->screen->windows == wa && !t->screen->zombies,
          "freed once the press is delivered");
#if defined(CONFIG_MEMORY_STATS)
    CHECK(th_heap_net(t) < before, "memory returned");
#else
    (void) before;
#endif
    CONSISTENT(t, "none: destroyed from Activate");
    twin_window_destroy(wa);
    th_destroy(t);

#if defined(CONFIG_WM_MOBILE)
    /* Configure during a relayout of every window */
    phone_t ph;
    phone_init(&ph);
    app_t c = {.color = 0xff3333cc}, d = {.color = TwinEventConfigure};
    twin_window_t *wc = launch(&ph, &c);
    twin_window_t *wd = launch(&ph, &d);
    th_settle(ph.t, 1000);
    wd->event = destroy_on;
    wd->destroy = NULL;
    self_destroyed = 0;
    twin_screen_set_inset(ph.t->screen, 40);
    th_settle(ph.t, 1000);
    CHECK_EQ(self_destroyed, 1, "destroyed from Configure");
    CHECK(shown(wc) && wc->pixmap->height == H - TOP_BAR - BOTTOM_BAR - 40,
          "the rest of the relayout went on");
    CONSISTENT(ph.t, "mobile: destroyed from Configure");
    twin_window_destroy(wc);
    phone_fini(&ph);
#endif
}

static void test_none_focus(void)
{
    th_t *t = th_create(W, H, TwinWmNone);
    app_t a = {.color = 0xffcc3333}, b = {.color = 0xff3333cc};
    twin_window_t *wa = mk(t, &a, TwinWindowPlain, 10, 10, 80, 80);
    twin_window_t *wb = mk(t, &b, TwinWindowPlain, 100, 100, 80, 80);
    th_frame(t);
    CHECK(t->screen->active == wb->pixmap, "newest window focused");
    twin_window_hide(wb);
    CHECK(t->screen->active == wa->pixmap, "focus falls back on hide");
    th_key(t, 'x');
    CHECK_EQ(a.keys, 1, "keys reach the remaining window");
    twin_window_destroy(wb);
    twin_window_destroy(wa);
    th_destroy(t);
}

#if defined(CONFIG_WM_DESKTOP)
/* Title bar point clear of the buttons (the name sits there) */
static void title_point(const twin_window_t *w,
                        twin_coord_t *x,
                        twin_coord_t *y)
{
    *x = w->pixmap->x + 40;
    *y = w->pixmap->y + w->client.top / 2;
}

static bool down_destroys(twin_window_t *w, twin_event_t *ev)
{
    if (ev->kind == TwinEventButtonDown) {
        twin_window_destroy(w);
        return false; /* gone, and the press is not claimed */
    }
    return false;
}

static twin_window_t *leave_victim;

static bool leave_destroys(twin_window_t *w, twin_event_t *ev)
{
    if (ev->kind == TwinEventLeave && leave_victim == w) {
        leave_victim = NULL;
        app_t *a = w->client_data;
        twin_window_destroy(w);
        a->destroyed++; /* no destroy callback: count it here */
        return true;
    }
    return app_event(w, ev);
}
#endif

static void test_desktop_focus(void)
{
#if defined(CONFIG_WM_DESKTOP)
    th_t *t = th_create(W, H, TwinWmDesktop);
    app_t a = {.color = 0xffcc3333, .refuse_close = true};
    app_t b = {.color = 0xff33cc33, .refuse_close = true};
    app_t c = {.color = 0xff3333cc, .refuse_close = true};
    twin_window_t *wa = mk(t, &a, TwinWindowApplication, 0, 0, 100, 60);
    twin_window_t *wb = mk(t, &b, TwinWindowApplication, 20, 100, 160, 100);
    twin_window_t *wc = mk(t, &c, TwinWindowApplication, 20, 60, 160, 100);
    th_frame(t);
    CHECK(wc->active && t->screen->active == wc->pixmap, "newest active");

    /* Closing a background window leaves the focus alone */
    twin_window_destroy(wa);
    CHECK(wc->active && t->screen->active == wc->pixmap,
          "unmapping an inactive window keeps the focus");

    /* Iconify the focused window: the keyboard moves to one with content */
    CHECK(tap_title_until(t, wc, is_iconified) > 0, "iconify button found");
    CHECK(!wc->active && wb->active && t->screen->active == wb->pixmap,
          "iconified window gives up the keyboard");
    th_key(t, 'k');
    CHECK(b.keys == 1 && c.keys == 0, "keys go to the visible window");

    /* Clicking the iconified title raises it but leaves the keyboard */
    twin_coord_t x, y;
    title_point(wc, &x, &y);
    th_tap(t, x, y);
    th_advance(t, 400);
    CHECK(t->screen->top == wc->pixmap && t->screen->active == wb->pixmap,
          "raised iconified window stays unfocused");

    /* Explicit activation restores it */
    twin_window_activate(wc);
    CHECK(is_restored(wc) && wc->active && t->screen->active == wc->pixmap,
          "twin_window_activate restores and focuses");

    /* Focus cycling walks the stack */
    twin_screen_focus_next(t->screen);
    CHECK(wb->active && t->screen->top == wb->pixmap, "focus_next raises b");
    twin_screen_focus_next(t->screen);
    CHECK(wc->active && t->screen->top == wc->pixmap, "and back to c");
    CONSISTENT(t, "desktop: focus");

    twin_window_destroy(wc);
    CHECK(wb->active, "closing the focused window hands focus on");
    twin_window_destroy(wb);
    CHECK(t->screen->active == NULL, "no window, no focus");
    th_destroy(t);
#endif
}

static void test_desktop_moves(void)
{
#if defined(CONFIG_WM_DESKTOP)
    th_t *t = th_create(W, H, TwinWmDesktop);
    app_t a = {.color = 0xffcc3333, .pass_presses = true};
    app_t b = {.color = 0xff33cc33};
    twin_window_t *wa = mk(t, &a, TwinWindowApplication, 10, 10, 120, 100);
    twin_window_t *wb = mk(t, &b, TwinWindowApplication, 10, 10, 120, 100);
    th_frame(t);
    CHECK(wb->pixmap->x != wa->pixmap->x && wb->pixmap->y != wa->pixmap->y,
          "a new window at an occupied spot cascades");
    twin_window_destroy(wb);
    twin_coord_t x, y;

    /* Dragging never strands the title bar off screen */
    title_point(wa, &x, &y);
    th_drag(t, x, y, -300, -300, 6);
    CHECK(wa->pixmap->y >= 0 && wa->pixmap->x + wa->pixmap->width >= 32,
          "title stays reachable (%d,%d)", wa->pixmap->x, wa->pixmap->y);
    th_advance(t, 400);
    title_point(wa, &x, &y);
    if (x < 1)
        x = wa->pixmap->x + wa->pixmap->width - 10;
    th_drag(t, x, y, x + 400, y + 400, 6);
    CHECK(wa->pixmap->x <= W - 32 && wa->pixmap->y <= H - 20,
          "title stays on screen (%d,%d)", wa->pixmap->x, wa->pixmap->y);
    CONSISTENT(t, "desktop: clamped drags");
    twin_window_configure(wa, TwinWindowApplication, 10, 10, 120, 100);
    th_advance(t, 400);

    /* Double-click the title: maximize, and back */
    title_point(wa, &x, &y);
    th_tap(t, x, y);
    th_tap(t, x, y);
    CHECK(wa->pixmap->x == 0 && wa->pixmap->y == 0 && wa->pixmap->width == W &&
              wa->pixmap->height == H,
          "double-click maximizes (%d,%d %dx%d)", wa->pixmap->x, wa->pixmap->y,
          wa->pixmap->width, wa->pixmap->height);
    CONSISTENT(t, "desktop: maximized");
    th_advance(t, 400);
    title_point(wa, &x, &y);
    th_tap(t, x, y);
    th_tap(t, x, y);
    CHECK(
        wa->pixmap->x == 10 && wa->pixmap->y == 10 && wa->pixmap->width == 120,
        "second double-click restores (%d,%d %d)", wa->pixmap->x, wa->pixmap->y,
        wa->pixmap->width);
    th_advance(t, 400);

    /* Drop on the left edge: snap to the left half */
    title_point(wa, &x, &y);
    th_drag(t, x, y, 0, 150, 6);
    CHECK(wa->pixmap->x == 0 && wa->pixmap->width == W / 2 &&
              wa->pixmap->height == H,
          "left-edge drop snaps (%d %dx%d)", wa->pixmap->x, wa->pixmap->width,
          wa->pixmap->height);
    /* Dragging a snapped window gives it back its size */
    th_advance(t, 400);
    title_point(wa, &x, &y);
    th_drag(t, x, y, x + 30, y + 40, 4);
    CHECK_EQ(wa->pixmap->width, 120, "unsnapped by dragging");
    CONSISTENT(t, "desktop: snapped and back");

    /* Unclaimed press in the bottom-right corner resizes */
    twin_coord_t cw = wa->client.right - wa->client.left;
    twin_coord_t ch = wa->client.bottom - wa->client.top;
    twin_coord_t gx = wa->pixmap->x + wa->client.right - 3;
    twin_coord_t gy = wa->pixmap->y + wa->client.bottom - 3;
    twin_coord_t px = wa->pixmap->x, py = wa->pixmap->y;
    th_drag(t, gx, gy, gx + 20, gy + 10, 4);
    CHECK_EQ(wa->client.right - wa->client.left, cw + 20, "resize width");
    CHECK_EQ(wa->client.bottom - wa->client.top, ch + 10, "resize height");
    CHECK(wa->pixmap->x == px && wa->pixmap->y == py, "resize keeps origin");
    th_drag(t, gx + 20, gy + 10, gx - 300, gy - 300, 4);
    CHECK(wa->client.right - wa->client.left >= 48, "resize keeps a minimum");
    CONSISTENT(t, "desktop: resized");
    twin_window_configure(wa, TwinWindowApplication, 10, 10, 120, 100);

    /* A release over another window still ends the drag */
    app_t p = {.color = 0xff3333cc};
    twin_window_t *wp = mk(t, &p, TwinWindowPlain, 150, 200, 60, 60);
    th_advance(t, 400);
    title_point(wa, &x, &y);
    th_drag(t, x, y, 170, 220, 6);
    twin_coord_t dropped_x = wa->pixmap->x;
    th_move(t, wa->pixmap->x + 10, wa->pixmap->y + 5);
    CHECK_EQ(wa->pixmap->x, dropped_x, "no drag after a release elsewhere");
    CHECK(p.presses == 0, "popup never saw the drag");
    twin_window_destroy(wp);

    /* Clients that destroy themselves from their own handlers */
    app_t d = {.color = 0xff777777};
    twin_window_t *wd = mk(t, &d, TwinWindowApplication, 60, 60, 80, 80);
    wd->event = down_destroys;
    wd->destroy = NULL;
    th_tap(t, wd->pixmap->x + 20, wd->pixmap->y + wd->client.top + 20);
    CHECK(t->screen->windows == wa && !wa->wm_next,
          "self-destroying press handled");
    app_t e = {.color = 0xff888888};
    twin_window_t *we = mk(t, &e, TwinWindowApplication, 60, 160, 80, 60);
    we->event = leave_destroys;
    we->destroy = NULL;
    leave_victim = we;
    th_move(t, we->pixmap->x + 20, we->pixmap->y + we->client.top + 10);
    th_move(t, 5, 5); /* leave: we destroys itself */
    th_move(t, 200, 300);
    CHECK(e.destroyed == 1, "destroyed from its own Leave");
    CONSISTENT(t, "desktop: self-destroyed");

    twin_window_destroy(wa);
    th_destroy(t);
#endif
}

/* Rotation or a resized host: every policy re-places its windows */
static void test_screen_resize(void)
{
#if defined(CONFIG_WM_MOBILE)
    {
        phone_t ph;
        phone_init(&ph);
        th_t *t = ph.t;
        app_t a = {.color = 0xffcc3333};
        twin_window_t *wa = launch(&ph, &a);
        th_settle(t, 1000);
        th_resize(t, H, W);
        th_settle(t, 1000);
        CHECK(wa->pixmap->width == H &&
                  wa->pixmap->height == W - TOP_BAR - BOTTOM_BAR,
              "app refits the rotated work area (%dx%d)", wa->pixmap->width,
              wa->pixmap->height);
        CHECK(ph.bar_bottom->pixmap->y == W - BOTTOM_BAR &&
                  ph.bar_bottom->pixmap->width == H,
              "bottom bar follows the new size");
        CONSISTENT(t, "mobile: rotated");
        twin_window_destroy(wa);
        phone_fini(&ph);
    }
#endif
#if defined(CONFIG_WM_DESKTOP)
    {
        th_t *t = th_create(W, H, TwinWmDesktop);
        app_t a = {.color = 0xffcc3333}, b = {.color = 0xff33cc33};
        twin_window_t *wa = mk(t, &a, TwinWindowApplication, 150, 200, 80, 80);
        twin_window_t *wb = mk(t, &b, TwinWindowApplication, 0, 0, 120, 60);
        twin_coord_t x, y;
        title_point(wb, &x, &y);
        th_tap(t, x, y);
        th_tap(t, x, y); /* maximize b */
        CHECK(wb->wm_maximized, "b maximized");
        th_resize(t, 200, 200);
        th_settle(t, 1000);
        twin_pixmap_t *p = wa->pixmap;
        CHECK(p->x >= 0 && p->y >= 0 && p->x + p->width <= 200 &&
                  p->y + p->height <= 200,
              "window back on the smaller screen (%d,%d %dx%d)", p->x, p->y,
              p->width, p->height);
        CHECK(wb->pixmap->width == 200 && wb->pixmap->height == 200,
              "maximized window refits (%dx%d)", wb->pixmap->width,
              wb->pixmap->height);
        CONSISTENT(t, "desktop: resized screen");
        twin_window_destroy(wa);
        twin_window_destroy(wb);
        th_destroy(t);
    }
#endif
}

#if defined(CONFIG_WM_MOBILE)
static void test_mobile_edge(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t a = {.color = 0xffcc3333}, b = {.color = 0xff33cc33};
    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
    twin_window_t *wb = launch(&ph, &b);
    th_settle(t, 1000);

    /* A tap at the edge is the app's, even with a stack to go back to */
    th_tap(t, 3, 150);
    CHECK(b.presses == 1 && b.last_x == 3 && b.ups == 1,
          "edge tap reaches the app (%d presses at %d)", b.presses, b.last_x);
    CHECK(wb->pixmap->x == 0 && shown(wb), "no swipe for a tap");
    /* So does a vertical drag along the edge */
    th_drag(t, 3, 100, 5, 200, 6);
    CHECK(b.presses == 2 && wb->pixmap->x == 0,
          "vertical edge drag is the app's");
    CONSISTENT(t, "mobile: edge tap");

    /* So does a press resting at the edge (long press) */
    th_press(t, 3, 150);
    CHECK_EQ(b.presses, 2, "edge press held at first");
    th_advance(t, 200);
    CHECK_EQ(b.presses, 3, "resting edge press handed to the app");
    th_release(t, 3, 150);
    CHECK(wb->pixmap->x == 0, "long press is not a swipe");

    /* An app with its own history takes the back gesture */
    b.handle_back = true;
    th_drag(t, 3, 150, 200, 150, 8);
    th_settle(t, 1000);
    CHECK(b.backs == 1 && b.closes == 0 && shown(wb) && wb->pixmap->x == 0,
          "handled back springs the window home");
    b.handle_back = false;

    /* Escape goes back when the app leaves it alone */
    th_key(t, 27);
    th_settle(t, 1000);
    CHECK(b.backs == 2 && b.destroyed == 1, "escape closes the top app");
    CHECK(shown(wa) && t->screen->active == wa->pixmap, "previous app back");
    th_key(t, 27);
    th_settle(t, 1000);
    CHECK(a.destroyed == 0 && shown(wa), "escape keeps the root app");

    /* Keys follow a push at once; the press dropped mid-push stays dropped */
    app_t c = {.color = 0xff3333cc};
    twin_window_t *wc = launch(&ph, &c);
    th_advance(t, 48);
    CHECK(t->screen->active == wc->pixmap, "keyboard moves with the push");
    th_press(t, 120, 150);
    th_settle(t, 1000);
    th_release(t, 120, 150);
    CHECK(c.presses == 0 && c.ups == 0 && a.ups == 0,
          "release of a swallowed press is swallowed too");
    th_tap(t, 120, 150);
    CHECK(c.presses == 1, "input back after the transition");
    CONSISTENT(t, "mobile: after edge tests");

    twin_window_destroy(wc);
    th_settle(t, 1000);
    twin_window_destroy(wa);
    phone_fini(&ph);
}

static void test_mobile_bars(void)
{
    phone_t ph;
    phone_init(&ph);
    th_t *t = ph.t;
    app_t a = {.color = 0xffcc3333};
    twin_window_t *wa = launch(&ph, &a);
    th_settle(t, 1000);
    twin_coord_t full = H - TOP_BAR - BOTTOM_BAR;

    twin_window_hide(ph.bar_bottom);
    th_settle(t, 1000);
    CHECK(twin_screen_work_area(t->screen).bottom == H &&
              wa->pixmap->height == H - TOP_BAR,
          "a hidden bar gives its rows back (%d)", wa->pixmap->height);
    CONSISTENT(t, "mobile: bar hidden");
    twin_window_show(ph.bar_bottom);
    th_settle(t, 1000);
    CHECK(wa->pixmap->height == full && shown(ph.bar_bottom), "bar back");

    /* Keyboard inset */
    twin_screen_set_inset(t->screen, 60);
    th_settle(t, 1000);
    CHECK_EQ(wa->pixmap->height, full - 60, "inset shrinks the app");
    CONSISTENT(t, "mobile: inset");
    twin_screen_set_inset(t->screen, 0);
    CHECK_EQ(wa->pixmap->height, full, "inset released");

    /* Resizing a bar re-lays out the apps */
    twin_window_configure(ph.bar_bottom, TwinWindowPlain, 0, 0, W, 40);
    th_settle(t, 1000);
    CHECK(wa->pixmap->height == H - TOP_BAR - 40 &&
              ph.bar_bottom->pixmap->y == H - 40,
          "taller bar, shorter app (%d)", wa->pixmap->height);
    twin_window_configure(ph.bar_bottom, TwinWindowPlain, 0, 0, W, BOTTOM_BAR);

    /* An unregistered bar is unmapped, not left as a popup */
    twin_screen_set_bars(t->screen, ph.bar_top, NULL);
    th_settle(t, 1000);
    CHECK(!ph.bar_bottom->mapped && !ph.bar_bottom->pixmap->screen,
          "replaced bar unmapped");
    CHECK_EQ(wa->pixmap->height, H - TOP_BAR, "its rows go to the app");
    CONSISTENT(t, "mobile: bar dropped");

    /* A destroyed bar unregisters itself */
    twin_window_destroy(ph.bar_top);
    ph.bar_top = NULL;
    th_settle(t, 1000);
    CHECK(t->screen->bar_top == NULL && wa->pixmap->y == 0 &&
              wa->pixmap->height == H,
          "destroyed bar's space returns");
    CONSISTENT(t, "mobile: bar destroyed");

    twin_window_destroy(wa);
    twin_window_destroy(ph.bar_bottom);
    th_destroy(t);
}
#endif

static void test_switch_pending(void)
{
#if defined(CONFIG_WM_DESKTOP) && defined(CONFIG_WM_MOBILE)
    th_t *t = th_create(W, H, TwinWmMobile);
    app_t a = {.color = 0xffaa3333};
    twin_window_t *w = mk(t, &a, TwinWindowApplication, 0, 20, W, 258);
    w->event = switching_event;
    th_settle(t, 1000);

    /* A direct request replaces one queued by a handler */
    switch_to = TwinWmDesktop;
    twin_event_t ev = {.kind = TwinEventButtonDown};
    ev.u.pointer.screen_x = 100;
    ev.u.pointer.screen_y = 100;
    twin_screen_dispatch(t->screen, &ev);
    CHECK(twin_screen_get_wm(t->screen) == TwinWmDesktop, "queued");
    CHECK(twin_screen_set_wm(t->screen, TwinWmNone), "direct switch");
    th_settle(t, 1000);
    CHECK(t->screen->wm->mode == TwinWmNone, "stale queued switch dropped");
    th_release(t, 100, 100);

    /* Switching while a swipe is under the finger */
    twin_screen_set_wm(t->screen, TwinWmMobile);
    w->event = app_event;
    app_t b = {.color = 0xff33aa33};
    twin_window_t *wb = mk(t, &b, TwinWindowApplication, 0, 20, W, 258);
    th_settle(t, 1000);
    th_press(t, 3, 150);
    th_move(t, 60, 150);
    twin_screen_set_wm(t->screen, TwinWmDesktop);
    th_move(t, 90, 150);
    th_release(t, 90, 150);
    th_settle(t, 1000);
    CONSISTENT(t, "switch mid swipe");
    twin_window_destroy(wb);

#if defined(CONFIG_MEMORY_STATS)
    /* Out of memory: a handler's switch is refused, not forced */
    twin_screen_set_wm(t->screen, TwinWmMobile);
    th_settle(t, 1000);
    w->event = switching_event;
    switch_to = TwinWmDesktop;
    _twin_alloc_fail_next = 1;
    dispatch_result = true;
    th_tap(t, 100, 100);
    _twin_alloc_fail_next = 0;
    CHECK(!dispatch_result, "queuing failure reported");
    th_settle(t, 1000);
    CHECK(t->screen->wm->mode == TwinWmMobile, "current policy kept");

    /* A policy that cannot start falls back to none */
    _twin_alloc_fail_next = 1;
    CHECK(!twin_screen_set_wm(t->screen, TwinWmDesktop), "failed start");
    _twin_alloc_fail_next = 0;
    CHECK(twin_screen_get_wm(t->screen) == TwinWmNone, "fell back to none");
    th_settle(t, 1000);
    CONSISTENT(t, "after failed switches");
#endif
    twin_window_destroy(w);
    th_destroy(t);
#endif
}

int main(void)
{
    test_none();
    test_desktop();
    test_desktop_iconify();
    test_restore_handoff();
#if defined(CONFIG_WM_MOBILE)
    test_mobile_layout();
    test_mobile_push_pop();
    test_mobile_swipe();
    test_mobile_fullscreen();
    test_mobile_pop_under_fullscreen();
    test_mobile_memory();
    test_mobile_edge();
    test_mobile_bars();
#endif
    test_none_focus();
    test_destroy_in_callbacks();
    test_desktop_focus();
    test_desktop_moves();
    test_screen_resize();
    test_switching();
    test_switch_from_handler();
    test_switch_pending();
    twin_cache_flush();
    return th_report("wm");
}
