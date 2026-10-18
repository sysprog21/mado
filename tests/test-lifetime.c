/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Lifetime and re-entrancy regressions: callbacks that run while an object
 * is being torn down, or that act on the object currently calling them.
 * Each test checks state rather than relying on a crash, so a regression
 * fails the check in plain builds and shows up under ASan as well.
 */

#include <stdlib.h>
#include <string.h>

#include "harness.h"

#define W 160
#define H 120

typedef struct {
    int draws, activates, deactivates;
} probe_t;

static void probe_draw(twin_window_t *w)
{
    probe_t *p = w->client_data;
    p->draws++;
    twin_fill(w->pixmap, 0xff336699, TWIN_SOURCE, 0, 0,
              w->client.right - w->client.left,
              w->client.bottom - w->client.top);
}

static bool probe_event(twin_window_t *w, twin_event_t *ev)
{
    probe_t *p = w->client_data;
    if (ev->kind == TwinEventActivate)
        p->activates++;
    else if (ev->kind == TwinEventDeactivate)
        p->deactivates++;
    return false;
}

static twin_window_t *probe_window(th_t *t,
                                   probe_t *p,
                                   twin_window_style_t style,
                                   twin_coord_t x,
                                   twin_coord_t y)
{
    twin_window_t *w =
        twin_window_create(t->screen, TWIN_ARGB32, style, x, y, 60, 40);
    w->client_data = p;
    w->draw = probe_draw;
    w->event = probe_event;
    return w;
}

/*
 * A window's destroy callback schedules a timeout and a work item on the
 * window itself.  Neither may fire once the window is freed.
 */
static int late_timeouts, late_works;

static twin_time_t late_timeout(twin_time_t now, void *closure)
{
    (void) now;
    (void) closure; /* never dereferenced: it may be freed */
    late_timeouts++;
    return -1;
}

static bool late_work(void *closure)
{
    (void) closure;
    late_works++;
    return false;
}

static void schedule_on_destroy(twin_window_t *w)
{
    twin_set_timeout(late_timeout, 0, w);
    twin_set_work(late_work, TWIN_WORK_PAINT, w);
}

static void test_window_destroy_schedules(void)
{
    th_t *t = th_create(W, H);
    probe_t p = {0};
    twin_window_t *w = probe_window(t, &p, TwinWindowApplication, 0, 0);
    w->destroy = schedule_on_destroy;
    twin_window_show(w);
    th_frame(t);

    void *addr = w;
    late_timeouts = late_works = 0;
    twin_window_destroy(w);
    CHECK(!_twin_closure_is_valid(addr),
          "destroyed window is no longer a live closure");
    th_advance(t, 100);
    CHECK_EQ(late_timeouts, 0, "timeout from the destroy callback dropped");
    CHECK_EQ(late_works, 0, "work from the destroy callback dropped");
    th_destroy(t);
}

#if defined(CONFIG_WIDGETS)
/*
 * A widget callback repaints on Destroy (e.g. it updates a label).  The
 * toplevel being torn down must not queue paint work that outlives it.
 */
static int destroy_repaints;

static twin_dispatch_result_t repaint_on_destroy(twin_widget_t *widget,
                                                 twin_event_t *event,
                                                 void *data)
{
    (void) data;
    if (event->kind == TwinEventDestroy) {
        destroy_repaints++;
        twin_widget_queue_paint(widget);
    }
    return TwinDispatchContinue;
}

static void test_toplevel_destroy_repaint(void)
{
    th_t *t = th_create(W, H);
    twin_toplevel_t *top = twin_toplevel_create(t->screen, 0, 0, W, H, "t");
    twin_label_t *label = twin_label_create(
        &top->box, "label", 0xff000000, twin_int_to_fixed(12), TwinStyleRoman);
    twin_widget_set_callback(&label->widget, repaint_on_destroy, NULL);
    twin_toplevel_show(top);
    th_frame(t); /* painted: no paint pending anywhere */

    void *addr = top;
    destroy_repaints = 0;
    twin_window_destroy(top->box.widget.window);
    CHECK_EQ(destroy_repaints, 1, "callback saw the destroy");
    CHECK(!_twin_closure_is_valid(addr),
          "no paint work registered on the freed toplevel");
    th_frame(t);
    th_destroy(t);
}
#endif

#if defined(CONFIG_WM_DESKTOP)
/*
 * Destroying a window that was never shown must leave focus alone: here a
 * popup holds it, and no other window may be activated in its place.
 */
static void test_desktop_destroy_unshown(void)
{
    th_t *t = th_create(W, H);
    probe_t pa = {0}, pp = {0}, pb = {0};
    twin_window_t *a = probe_window(t, &pa, TwinWindowApplication, 10, 10);
    twin_window_show(a);
    twin_window_t *popup = probe_window(t, &pp, TwinWindowPlain, 80, 60);
    twin_window_show(popup);
    twin_screen_set_active(t->screen, popup->pixmap);
    th_settle(t, 500);
    CHECK(t->screen->active == popup->pixmap, "popup has focus");

    twin_window_t *b = probe_window(t, &pb, TwinWindowApplication, 30, 30);
    int activates = pa.activates, deactivates = pp.deactivates;
    twin_window_destroy(b);
    th_settle(t, 500);
    CHECK(t->screen->active == popup->pixmap,
          "focus stays where it was after destroying a never-shown window");
    CHECK_EQ(pa.activates, activates, "no Activate sent to another window");
    CHECK_EQ(pp.deactivates, deactivates, "no Deactivate sent to the popup");

    twin_window_destroy(popup);
    twin_window_destroy(a);
    th_destroy(t);
}
#endif

#if defined(CONFIG_WM_MOBILE)
/*
 * Under the mobile policy a destroyed app's pixels are adopted to animate
 * the pop, leaving window->pixmap NULL while the destroy callback runs.
 * Re-laying out every window from that callback must skip the window, not
 * crash.
 */
static void relayout_on_destroy(twin_window_t *w)
{
    CHECK(!w->pixmap, "pixels adopted for the pop animation");
    twin_screen_set_inset(w->screen, 30);
}

static void test_relayout_from_destroy(void)
{
    th_t *t = th_create(W, H);
    probe_t pa = {0}, pb = {0};
    twin_window_t *a = probe_window(t, &pa, TwinWindowApplication, 0, 0);
    twin_window_show(a);
    th_settle(t, 1000);
    twin_window_t *b = probe_window(t, &pb, TwinWindowApplication, 0, 0);
    twin_window_show(b);
    th_settle(t, 1000);
    CHECK(b->pixmap->screen && b->pixmap->x == 0, "second app on top");

    b->destroy = relayout_on_destroy;
    twin_window_destroy(b);
    th_settle(t, 1000);
    CHECK(a->pixmap && a->pixmap->screen && a->pixmap->height == H - 30,
          "remaining app shown above the inset");
    CHECK_EQ(th_damage_check(t), 0, "screen consistent after the relayout");

    twin_window_destroy(a);
    th_destroy(t);
}
#endif

/*
 * A tween that finishes itself from its own step callback still promises
 * the final TWIN_FIXED_ONE frame, delivered once, and then stops.
 */
typedef struct {
    twin_tween_t *self;
    int calls, ones;
    twin_fixed_t last;
} self_finish_t;

static bool self_finish(twin_fixed_t value, void *closure)
{
    self_finish_t *s = closure;
    s->calls++;
    s->last = value;
    if (value == TWIN_FIXED_ONE)
        s->ones++;
    if (s->calls == 2)
        twin_tween_finish(s->self);
    return true;
}

static void test_tween_finish_from_proc(void)
{
    th_t *t = th_create(W, H);
    self_finish_t s = {0};
    s.self = twin_tween_start(1000, TwinEaseLinear, self_finish, &s);
    th_advance(t, 100);
    CHECK_EQ(s.last, TWIN_FIXED_ONE, "final frame delivered");
    CHECK_EQ(s.ones, 1, "final frame delivered exactly once");
    CHECK_EQ(s.calls, 3, "no frames after finishing");
    int calls = s.calls;
    th_advance(t, 1000);
    CHECK_EQ(s.calls, calls, "finished tween is gone");
    th_destroy(t);
}

/*
 * A path whose cache key collides with an entry of another size must be
 * rasterized uncached: inserting a duplicate key would create entries no
 * lookup can reach, and repeat the waste on every draw.
 *
 * The collision is forged by planting an entry under the path's key.  The
 * key formula is mirrored from _twin_path_key(); a sanity check proves the
 * mirror matches, so a change there fails loudly instead of passing.
 */
static uint64_t mirror_path_key(const twin_path_t *path, const twin_rect_t *b)
{
    uint64_t h = TWIN_HASH_INIT ^ 0x50;
    twin_sfixed_t ox = twin_int_to_sfixed(b->left);
    twin_sfixed_t oy = twin_int_to_sfixed(b->top);
    for (int i = 0; i < path->npoints; i++) {
        twin_sfixed_t xy[2] = {(twin_sfixed_t) (path->points[i].x - ox),
                               (twin_sfixed_t) (path->points[i].y - oy)};
        h = _twin_hash(h, xy, sizeof(xy));
    }
    h = _twin_hash(h, path->sublen, sizeof(int) * path->nsublen);
    h = _twin_hash(h, &path->npoints, sizeof(path->npoints));
    return h;
}

static void test_cache_collision(void)
{
    if (!_twin_cache_enabled()) {
        printf("lifetime: cache collision test skipped (cache disabled)\n");
        return;
    }
    twin_pixmap_t *ref = twin_pixmap_create(TWIN_ARGB32, 48, 48);
    twin_pixmap_t *px = twin_pixmap_create(TWIN_ARGB32, 48, 48);
    twin_fill(ref, 0xffffffff, TWIN_SOURCE, 0, 0, 48, 48);
    twin_fill(px, 0xffffffff, TWIN_SOURCE, 0, 0, 48, 48);
    twin_path_t *path = twin_path_create();
    twin_path_circle(path, twin_int_to_fixed(20), twin_int_to_fixed(20),
                     twin_int_to_fixed(9));
    twin_rect_t b;
    twin_path_bounds(path, &b);
    twin_coord_t w = b.right - b.left, h = b.bottom - b.top;
    uint64_t key = mirror_path_key(path, &b);

    twin_cache_flush();
    twin_paint_path(ref, 0xff000000, path);
    twin_cache_entry_t *e = _twin_cache_lookup(key);
    CHECK(e && e->width == w && e->height == h,
          "mirrored key finds the cached mask");

    twin_cache_flush();
    CHECK(_twin_cache_insert(key, w + 3, h) != NULL, "collision planted");
    twin_cache_stats_t s0, s1;
    twin_cache_get_stats(&s0);
    twin_paint_path(px, 0xff000000, path);
    twin_paint_path(px, 0xff000000, path);
    twin_cache_get_stats(&s1);
    CHECK_EQ(s1.entries, s0.entries, "no duplicate key inserted");
    CHECK_EQ(s1.bytes, s0.bytes, "no cache bytes wasted");

    /* The uncached raster matches the cached one: paint the reference a
     * second time too, so both have the same edges blended twice */
    twin_paint_path(ref, 0xff000000, path);
    bool same = true;
    for (int y = 0; y < 48 && same; y++)
        same = !memcmp(twin_pixmap_pointer(ref, 0, y).v,
                       twin_pixmap_pointer(px, 0, y).v, 48 * 4);
    CHECK(same, "uncached raster equals the cached one");

    twin_path_destroy(path);
    twin_pixmap_destroy(px);
    twin_pixmap_destroy(ref);
    twin_cache_flush();
}

/*
 * Destroying a screen in the middle of whatever its window manager is doing
 * (a push transition on a phone, a drag on a desktop) returns every byte,
 * and destroy callbacks that act on a sibling during the teardown (show,
 * hide, reconfigure, activate) reach nothing that is gone.
 */
static twin_window_t *teardown_sibling;
static int teardown_pokes;

static void poke_sibling(twin_window_t *w)
{
    teardown_pokes++;
    twin_window_t *s = teardown_sibling;
    if (!s || s == w)
        return;
    twin_window_show(s);
    twin_window_hide(s);
    twin_window_configure(s, TwinWindowApplication, 0, 0, 50, 50);
    twin_window_activate(s);
    twin_window_show(s);
}

static void forget_sibling(twin_window_t *w)
{
    if (teardown_sibling == w)
        teardown_sibling = NULL;
}

static void test_screen_teardown(void)
{
    twin_cache_flush();
    size_t before = th_heap_raw();
    th_t *t = th_create(W, H);
    probe_t pa = {0}, pb = {0}, pc = {0};
    twin_window_t *a = probe_window(t, &pa, TwinWindowApplication, 0, 0);
    twin_window_show(a);
    th_settle(t, 500);
    twin_window_t *b = probe_window(t, &pb, TwinWindowApplication, 20, 20);
    twin_window_show(b);
    twin_window_t *c = probe_window(t, &pc, TwinWindowPlain, 5, 5);
    twin_window_show(c);
    /* Mid push on a phone; press and move on b's top edge (its title bar on
     * a desktop) to be mid drag there */
    th_advance(t, 48);
    th_press(t, b->pixmap->x + 30, b->pixmap->y + 4);
    th_move(t, b->pixmap->x + 50, b->pixmap->y + 14);

    a->destroy = poke_sibling;
    b->destroy = poke_sibling;
    c->destroy = forget_sibling;
    teardown_sibling = c;
    teardown_pokes = 0;
    th_destroy(t);
    twin_cache_flush();
    CHECK_EQ(teardown_pokes, 2, "destroy callbacks ran once each");
    CHECK(!teardown_sibling, "the sibling went too");
#if defined(CONFIG_MEMORY_STATS)
    CHECK(th_heap_raw() <= before, "teardown returns every byte (%zu > %zu)",
          th_heap_raw(), before);
#else
    (void) before;
#endif
}

int main(void)
{
    test_window_destroy_schedules();
#if defined(CONFIG_WIDGETS)
    test_toplevel_destroy_repaint();
#endif
#if defined(CONFIG_WM_DESKTOP)
    test_desktop_destroy_unshown();
#endif
#if defined(CONFIG_WM_MOBILE)
    test_relayout_from_destroy();
#endif
    test_tween_finish_from_proc();
    test_screen_teardown();
    test_cache_collision();
    twin_cache_flush();
    return th_report("lifetime");
}
