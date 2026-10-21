/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Window core
 *
 * A window is a pixmap plus client callbacks.  Geometry, decorations,
 * stacking and pointer policy belong to the active window manager
 * (screen->wm); this file only keeps the bookkeeping consistent:
 *
 *  - screen->windows lists every window bottom-to-top.  Policies treat that
 *    order as the stacking order and raise a window by moving it to the end.
 *  - window->request is what the application asked for; window->client is
 *    where the policy put the client area inside the pixmap.
 *  - A window whose pixels are not needed (hidden under a full-screen app)
 *    can be evicted: its pixmap becomes a storage-less stub and is redrawn
 *    from client state when it is shown again.
 */

#include <stdlib.h>
#include <string.h>

#include "twin_private.h"

static void _twin_window_link(twin_window_t *window)
{
    twin_window_t **pp = &window->screen->windows;
    while (*pp)
        pp = &(*pp)->wm_next;
    window->wm_next = NULL;
    *pp = window;
}

static void _twin_window_unlink(twin_window_t *window)
{
    for (twin_window_t **pp = &window->screen->windows; *pp;
         pp = &(*pp)->wm_next) {
        if (*pp == window) {
            /* window->wm_next stays: a walk standing on a window that is
             * destroyed under it can still step to the rest of the list */
            *pp = window->wm_next;
            return;
        }
    }
}

#if TWIN_WM_BARS
bool _twin_window_is_bar(const twin_window_t *window)
{
    return window == window->screen->bar_top ||
           window == window->screen->bar_bottom;
}
#endif

bool _twin_window_shown(const twin_window_t *window)
{
    return window->pixmap && window->pixmap->screen &&
           !_twin_pixmap_is_stub(window->pixmap);
}

bool _twin_window_managed(const twin_window_t *window)
{
    return window->mapped && window->style != TwinWindowPlain &&
           !_twin_window_is_bar(window);
}

/* Keep the compositor's notion of opaque pixels in sync with the window */
static void _twin_window_update_opaque(twin_window_t *window)
{
    twin_pixmap_t *pixmap = window->pixmap;
    if (!pixmap || _twin_pixmap_is_stub(pixmap))
        return;
    if (pixmap->format == TWIN_RGB16)
        pixmap->opaque_rect =
            (twin_rect_t) {0, pixmap->width, 0, pixmap->height};
    else if (window->opaque)
        pixmap->opaque_rect = window->client;
    else
        pixmap->opaque_rect = (twin_rect_t) {0, 0, 0, 0};
    /* A solid client casts the shadow; a see-through one casts none and,
     * masked by its alpha, shows what is behind instead */
    twin_pixmap_set_shadow_cast(
        pixmap, 0, window->opaque ? window->client : (twin_rect_t) {0});
}

/* Clip to @r (pixmap coordinates) and put the origin at the client corner */
static void _twin_window_clip(twin_window_t *window, const twin_rect_t *r)
{
    twin_pixmap_t *pixmap = window->pixmap;
    twin_pixmap_reset_clip(pixmap);
    pixmap->origin_x = pixmap->origin_y = 0;
    twin_pixmap_clip(pixmap, r->left, r->top, r->right, r->bottom);
    pixmap->origin_x = window->client.left;
    pixmap->origin_y = window->client.top;
}

static void _twin_window_reset_clip(twin_window_t *window)
{
    _twin_window_clip(window, &window->client);
}

static void _twin_window_damage_all(twin_window_t *window)
{
    window->damage[0] = window->client;
    window->n_damage = 1;
    window->frame_dirty = true;
}

twin_window_t *twin_window_create(twin_screen_t *screen,
                                  twin_format_t format,
                                  twin_window_style_t style,
                                  twin_coord_t x,
                                  twin_coord_t y,
                                  twin_coord_t width,
                                  twin_coord_t height)
{
    twin_window_t *window = twin_calloc(1, sizeof(twin_window_t));
    twin_rect_t geom, border;

    if (!window)
        return NULL;
    window->screen = screen;
    window->style = style;
    window->format = format;
    window->request = (twin_rect_t) {x, x + width, y, y + height};
    window->opaque = format == TWIN_RGB16;

    _twin_wm_layout(window, &geom, &border);
    window->pixmap = twin_pixmap_create(format, geom.right - geom.left,
                                        geom.bottom - geom.top);
    if (!window->pixmap) {
        twin_free(window);
        return NULL;
    }
    window->pixmap->window = window;
    window->pixmap->x = geom.left;
    window->pixmap->y = geom.top;
    window->client = (twin_rect_t) {
        border.left,
        window->pixmap->width - border.right,
        border.top,
        window->pixmap->height - border.bottom,
    };
    _twin_window_reset_clip(window);
    _twin_window_update_opaque(window);
    _twin_window_damage_all(window);
    _twin_window_link(window);
    return window;
}

void twin_window_destroy(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;

    /* Stop queued repaints from touching freed memory.  Clearing the work
     * item matters: a later object at the same address would otherwise
     * look like a live owner to the closure tracker. */
    twin_clear_work(window->paint_work);
    window->paint_work = NULL;
    /* ...and refuse new ones from the policy or the destroy callback */
    window->draw_queued = true;
    _twin_closure_mark_for_free(window);
    _twin_closure_unregister(window);

    /* A destroyed bar unregisters itself; its rows go back to the work
     * area once it is gone (below) */
    bool was_bar = _twin_window_is_bar(window);
#if TWIN_WM_BARS
    if (screen->bar_top == window)
        screen->bar_top = NULL;
    if (screen->bar_bottom == window)
        screen->bar_bottom = NULL;
#endif

    /* Always tell the policy, even for unmapped windows, so it can drop any
     * reference it still holds (e.g. a window taking part in a transition).
     * A closing screen's policy has let go of every window already. */
    window->mapped = false;
    if (screen->closing)
        twin_pixmap_hide(window->pixmap);
    else
        _twin_wm_unmap(window, true);

    /* Call the destroy callback if set to clean up window contents */
    if (window->destroy)
        (*window->destroy)(window);

    /* The callback may have scheduled work or timeouts on the window; the
     * unregister above dropped the mark, so drop the entry again now that
     * nothing legitimate can use it. */
    twin_clear_work(window->paint_work);
    window->paint_work = NULL;
    _twin_closure_unregister(window);

    _twin_window_unlink(window);

    /* The policy may have adopted the pixmap to animate the departure. */
    if (window->pixmap) {
        _twin_screen_forget(screen, window->pixmap);
        twin_pixmap_destroy(window->pixmap);
    }
#if defined(CONFIG_WM_DESKTOP)
    twin_free(window->name);
    window->name = NULL;
#endif
    window->pixmap = NULL;
    window->event = NULL;
    window->draw = NULL;
    window->destroy = NULL;
    window->dead = true;

    if (was_bar && !screen->closing)
        _twin_screen_relayout(screen);
    /* Whoever is running callbacks may still hold the pointer */
    if (screen->dispatching) {
        window->zombie_next = screen->zombies;
        screen->zombies = window;
    } else {
        twin_free(window);
    }
}

void twin_window_show(twin_window_t *window)
{
    window->mapped = true;
    /* Nothing is shown any more on a closing screen */
    if (!window->screen->closing)
        _twin_wm_map(window);
}

void twin_window_hide(twin_window_t *window)
{
    if (!window->mapped)
        return;
    window->mapped = false;
    if (window->screen->closing)
        twin_pixmap_hide(window->pixmap);
    else
        _twin_wm_unmap(window, false);
}

/*
 * Swap window->pixmap for @fresh (already positioned) and free the old one.
 * The compositing attributes, the stacking position and pending update
 * disables move across.  With @keep_focus the screen's active pixmap and
 * pointer target follow the window; without it they are dropped, as when
 * the surface is evicted.
 */
static void _twin_window_replace_pixmap(twin_window_t *window,
                                        twin_pixmap_t *fresh,
                                        bool keep_focus)
{
    twin_pixmap_t *old = window->pixmap;
    twin_screen_t *screen = window->screen;

    fresh->window = window;
    fresh->opacity = old->opacity;
#if defined(CONFIG_DROP_SHADOW)
    fresh->shadow = old->shadow;
#endif
    if (old->scan_height < old->height)
        fresh->scan_height = min(old->scan_height, fresh->height);
    if (old->screen)
        twin_pixmap_show(fresh, old->screen, old);
    for (int i = 0; i < old->disable; i++)
        twin_pixmap_disable_update(fresh);
    if (keep_focus) {
        if (screen->active == old)
            screen->active = fresh;
        if (screen->target == old)
            screen->target = fresh;
    } else {
        _twin_screen_forget(screen, old);
    }
    twin_pixmap_destroy(old);
    window->pixmap = fresh;
}

void _twin_window_relayout(twin_window_t *window)
{
    twin_rect_t geom, border;
    twin_pixmap_t *pixmap = window->pixmap;

    /* A window being destroyed may have handed its pixmap to the policy;
     * a closing screen lays nothing out */
    if (!pixmap || window->screen->closing)
        return;
    _twin_wm_layout(window, &geom, &border);
    twin_coord_t width = geom.right - geom.left;
    twin_coord_t height = geom.bottom - geom.top;
    twin_rect_t old_client = window->client;
    twin_rect_t client = {border.left, width - border.right, border.top,
                          height - border.bottom};

    /* Same place, same size, same decorations: nothing to repaint */
    if (width == pixmap->width && height == pixmap->height &&
        window->format == pixmap->format && geom.left == pixmap->x &&
        geom.top == pixmap->y && !memcmp(&client, &old_client, sizeof(client)))
        return;

    if (width != pixmap->width || height != pixmap->height ||
        window->format != pixmap->format) {
        twin_pixmap_t *fresh =
            _twin_pixmap_is_stub(pixmap)
                ? _twin_pixmap_create_stub(window->format, width, height)
                : twin_pixmap_create(window->format, width, height);
        if (!fresh)
            return;
        fresh->x = geom.left;
        fresh->y = geom.top;
        _twin_window_replace_pixmap(window, fresh, true);
        pixmap = fresh;
    }

    window->client = client;
    _twin_window_reset_clip(window);
    _twin_window_update_opaque(window);
    twin_pixmap_move(pixmap, geom.left, geom.top);
    _twin_window_damage_all(window);

    /* Tell the client its size changed, then repaint everything. */
    if (window->event && (old_client.right - old_client.left !=
                              window->client.right - window->client.left ||
                          old_client.bottom - old_client.top !=
                              window->client.bottom - window->client.top)) {
        twin_event_t ev = {.kind = TwinEventConfigure};
        ev.u.configure.extents =
            (twin_rect_t) {0, window->client.right - window->client.left, 0,
                           window->client.bottom - window->client.top};
        (*window->event)(window, &ev);
        if (window->dead)
            return;
    }
    twin_window_draw(window);
}

void twin_window_configure(twin_window_t *window,
                           twin_window_style_t style,
                           twin_coord_t x,
                           twin_coord_t y,
                           twin_coord_t width,
                           twin_coord_t height)
{
    twin_screen_t *screen = window->screen;
    twin_rect_t area = twin_screen_work_area(screen);
    window->style = style;
    window->request = (twin_rect_t) {x, x + width, y, y + height};
#if defined(CONFIG_WM_DESKTOP)
    window->wm_maximized = false;
#endif
    /* The Configure handler may destroy the window: keep it readable */
    _twin_screen_busy(screen);
    _twin_window_relayout(window);
    /* A bar's height is part of everyone's work area */
    twin_rect_t now = twin_screen_work_area(screen);
    if (memcmp(&area, &now, sizeof(area)))
        _twin_screen_relayout(screen);
    else if (window->mapped && !screen->closing)
        /* The style may move the window between policy layers */
        _twin_wm_refresh(screen);
    _twin_screen_idle(screen);
}

void _twin_window_evict(twin_window_t *window)
{
    twin_pixmap_t *pixmap = window->pixmap;
    if (!pixmap || _twin_pixmap_is_stub(pixmap))
        return;
    /* Without a draw callback the pixels cannot be reconstructed */
    if (!window->draw)
        return;
    twin_pixmap_t *stub =
        _twin_pixmap_create_stub(pixmap->format, pixmap->width, pixmap->height);
    if (!stub)
        return;
    stub->x = pixmap->x;
    stub->y = pixmap->y;
    _twin_window_replace_pixmap(window, stub, false);
    _twin_window_damage_all(window);
}

bool _twin_window_restore(twin_window_t *window)
{
    twin_pixmap_t *stub = window->pixmap;
    if (!_twin_pixmap_is_stub(stub))
        return true;
    twin_pixmap_t *pixmap =
        twin_pixmap_create(stub->format, stub->width, stub->height);
    if (!pixmap)
        return false;
    pixmap->x = stub->x;
    pixmap->y = stub->y;
    _twin_window_replace_pixmap(window, pixmap, true);
    _twin_window_reset_clip(window);
    _twin_window_update_opaque(window);
    _twin_window_damage_all(window);
    /* Painted before the next redisplay; a window covered and evicted
     * again first (closing a stack of apps) is never painted at all */
    twin_window_queue_paint(window);
    return true;
}

void twin_window_set_name(twin_window_t *window, const char *name)
{
#if defined(CONFIG_WM_DESKTOP)
    /* Only title bars show it */
    if (!name)
        return;
    char *new_name = twin_malloc(strlen(name) + 1);
    if (!new_name)
        return;
    strcpy(new_name, name);
    twin_free(window->name);
    window->name = new_name;
    window->frame_dirty = true;
    twin_window_queue_paint(window);
#else
    (void) window;
    (void) name;
#endif
}

void twin_window_set_opaque(twin_window_t *window, bool opaque)
{
    window->opaque = opaque || window->format == TWIN_RGB16;
    _twin_window_update_opaque(window);
    /* Opacity decides what an opaque full-screen window covers */
    if (window->mapped && !window->screen->closing)
        _twin_wm_refresh(window->screen);
}

void twin_window_draw(twin_window_t *window)
{
    twin_pixmap_t *pixmap = window->pixmap;

    /* Evicted: keep the damage, the restore will repaint everything. */
    if (_twin_pixmap_is_stub(pixmap))
        return;

    if (window->frame_dirty) {
        window->frame_dirty = false;
        if (TWIN_WM_DECORATES) {
            _twin_wm_decorate(window);
            _twin_window_reset_clip(window);
        }
    }

    /* if no draw function or no damage, return */
    if (window->draw == NULL || !window->n_damage)
        return;

    /* Paint each damaged rectangle under its own clip: two small updates
     * at opposite ends of a window stay two small repaints. */
    twin_rect_t rects[TWIN_WINDOW_DAMAGE_RECTS];
    int n = window->n_damage;
    memcpy(rects, window->damage, sizeof(rects[0]) * n);
    window->n_damage = 0;

    twin_screen_disable_update(window->screen);
    for (int i = 0; i < n; i++) {
        _twin_window_clip(window, &rects[i]);
        (*window->draw)(window);
        twin_pixmap_damage(pixmap, rects[i].left, rects[i].top, rects[i].right,
                           rects[i].bottom);
    }
    twin_screen_enable_update(window->screen);

    _twin_window_reset_clip(window);
}

/* window keep track of local damage */
void twin_window_damage(twin_window_t *window,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom)
{
    if (left < window->client.left)
        left = window->client.left;
    if (top < window->client.top)
        top = window->client.top;
    if (right > window->client.right)
        right = window->client.right;
    if (bottom > window->client.bottom)
        bottom = window->client.bottom;
    if (left >= right || top >= bottom)
        return;

    _twin_rect_list_add(window->damage, &window->n_damage,
                        TWIN_WINDOW_DAMAGE_RECTS,
                        (twin_rect_t) {left, right, top, bottom});
    twin_window_queue_paint(window);
}

static bool _twin_window_repaint(void *closure)
{
    twin_window_t *window = closure;

    window->draw_queued = false;
    window->paint_work = NULL;
    twin_window_draw(window);

    return false;
}

void twin_window_queue_paint(twin_window_t *window)
{
    if (!window->draw_queued) {
        window->draw_queued = true;
        window->paint_work =
            twin_set_work(_twin_window_repaint, TWIN_WORK_PAINT, window);
        if (!window->paint_work)
            window->draw_queued = false;
    }
}

void twin_window_close(twin_window_t *window)
{
    twin_screen_t *screen = window->screen;
    twin_event_t ev = {.kind = TwinEventClose};
    /* The handler may destroy the window itself: keep it readable */
    _twin_screen_busy(screen);
    if (!_twin_window_deliver(window, &ev) && !window->dead)
        twin_window_destroy(window);
    _twin_screen_idle(screen);
}

void twin_window_scroll(twin_window_t *window,
                        twin_rect_t area,
                        twin_coord_t dx,
                        twin_coord_t dy)
{
    twin_pixmap_t *pixmap = window->pixmap;

    /* Client coordinates to pixmap coordinates, clipped to the client */
    area.left = max((twin_coord_t) (area.left + window->client.left),
                    window->client.left);
    area.right = min((twin_coord_t) (area.right + window->client.left),
                     window->client.right);
    area.top =
        max((twin_coord_t) (area.top + window->client.top), window->client.top);
    area.bottom = min((twin_coord_t) (area.bottom + window->client.top),
                      window->client.bottom);
    if (area.left >= area.right || area.top >= area.bottom || (!dx && !dy))
        return;

    /* Pending damage marks stale pixels.  The blit carries them along, so
     * the damage has to follow: repaint where they land.  (Painting first
     * would be wrong: callers update their state, e.g. the scroll offset,
     * before scrolling.) */
    int pending = window->n_damage;
    twin_rect_t moved[TWIN_WINDOW_DAMAGE_RECTS];
    for (int i = 0; i < pending; i++) {
        twin_rect_t d = window->damage[i];
        d.left = max(d.left, area.left) + dx;
        d.right = min(d.right, area.right) + dx;
        d.top = max(d.top, area.top) + dy;
        d.bottom = min(d.bottom, area.bottom) + dy;
        moved[i] = d;
    }
    for (int i = 0; i < pending; i++)
        twin_window_damage(
            window, max(moved[i].left, area.left), max(moved[i].top, area.top),
            min(moved[i].right, area.right), min(moved[i].bottom, area.bottom));

    twin_coord_t w = area.right - area.left, h = area.bottom - area.top;
    if (_twin_pixmap_is_stub(pixmap) || dx >= w || -dx >= w || dy >= h ||
        -dy >= h) {
        twin_window_damage(window, area.left, area.top, area.right,
                           area.bottom);
        twin_window_queue_paint(window);
        return;
    }

    /* Destination rectangle: the part of @area still covered after moving */
    twin_rect_t d = {
        .left = area.left + max(dx, (twin_coord_t) 0),
        .right = area.right + min(dx, (twin_coord_t) 0),
        .top = area.top + max(dy, (twin_coord_t) 0),
        .bottom = area.bottom + min(dy, (twin_coord_t) 0),
    };
    int bpp = twin_bytes_per_pixel(pixmap->format);
    size_t row_bytes = (size_t) (d.right - d.left) * bpp;
    if (dy > 0) {
        for (twin_coord_t y = d.bottom - 1; y >= d.top; y--)
            memmove(twin_pixmap_pointer(pixmap, d.left, y).v,
                    twin_pixmap_pointer(pixmap, d.left - dx, y - dy).v,
                    row_bytes);
    } else {
        for (twin_coord_t y = d.top; y < d.bottom; y++)
            memmove(twin_pixmap_pointer(pixmap, d.left, y).v,
                    twin_pixmap_pointer(pixmap, d.left - dx, y - dy).v,
                    row_bytes);
    }
    twin_pixmap_damage(pixmap, d.left, d.top, d.right, d.bottom);

    /* Exposed bands */
    if (dy > 0)
        twin_window_damage(window, area.left, area.top, area.right, d.top);
    else if (dy < 0)
        twin_window_damage(window, area.left, d.bottom, area.right,
                           area.bottom);
    if (dx > 0)
        twin_window_damage(window, area.left, area.top, d.left, area.bottom);
    else if (dx < 0)
        twin_window_damage(window, d.right, area.top, area.right, area.bottom);
    twin_window_queue_paint(window);
}

bool _twin_window_hit(twin_window_t *window, twin_coord_t x, twin_coord_t y)
{
#if TWIN_WM_DECORATES
    return _twin_wm_hit(window, x, y);
#else
    twin_pixmap_t *p = window->pixmap;
    return p->x <= x && x < p->x + p->width && p->y <= y &&
           y < p->y + p->scan_height;
#endif
}

/* Forward an event to the client, in client coordinates */
bool _twin_window_deliver(twin_window_t *window, twin_event_t *event)
{
    twin_event_t ev = *event;

    switch (ev.kind) {
    case TwinEventButtonDown:
    case TwinEventButtonUp:
    case TwinEventMotion:
    case TwinEventEnter:
    case TwinEventLeave:
        ev.u.pointer.x -= window->client.left;
        ev.u.pointer.y -= window->client.top;
        break;
    default:
        break;
    }
    if (window->event)
        return (*window->event)(window, &ev);
    return false;
}

bool _twin_window_dispatch(twin_window_t *window, twin_event_t *event)
{
    return _twin_wm_dispatch(window, event);
}

/* Move @window to the top of the stacking order */
void _twin_window_raise(twin_window_t *window)
{
    _twin_window_unlink(window);
    _twin_window_link(window);
}

twin_window_t *twin_screen_first_window(const twin_screen_t *screen)
{
    return screen ? screen->windows : NULL;
}

twin_window_t *twin_window_next(const twin_window_t *window)
{
    return window->wm_next;
}

twin_pixmap_t *twin_window_pixmap(const twin_window_t *window)
{
    return window->pixmap;
}

twin_rect_t twin_window_client(const twin_window_t *window)
{
    return window->client;
}

twin_window_style_t twin_window_style(const twin_window_t *window)
{
    return window->style;
}

bool twin_window_is_mapped(const twin_window_t *window)
{
    return window->mapped;
}

void twin_window_set_callbacks(twin_window_t *window,
                               twin_draw_func_t draw,
                               twin_event_func_t event,
                               twin_destroy_func_t destroy)
{
    window->draw = draw;
    window->event = event;
    window->destroy = destroy;
}

void twin_window_set_client_data(twin_window_t *window, void *data)
{
    window->client_data = data;
}

void *twin_window_client_data(const twin_window_t *window)
{
    return window->client_data;
}
