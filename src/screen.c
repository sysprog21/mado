/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

twin_screen_t *twin_screen_create(twin_coord_t width,
                                  twin_coord_t height,
                                  twin_put_begin_t put_begin,
                                  twin_put_span_t put_span,
                                  void *closure)
{
    twin_screen_t *screen = calloc(1, sizeof(twin_screen_t));
    if (!screen)
        return NULL;

    screen->top = 0;
    screen->bottom = 0;
    screen->width = width;
    screen->height = height;
    screen->damage.left = screen->damage.right = 0;
    screen->damage.top = screen->damage.bottom = 0;
    screen->damaged = NULL;
    screen->damaged_closure = NULL;
    screen->disable = 0;
    screen->background = 0;
    screen->put_begin = put_begin;
    screen->put_span = put_span;
    screen->closure = closure;

    screen->button_x = screen->button_y = -1;
    return screen;
}

void twin_screen_destroy(twin_screen_t *screen)
{
    while (screen->bottom)
        twin_pixmap_hide(screen->bottom);
    free(screen);
}

void twin_screen_register_damaged(twin_screen_t *screen,
                                  void (*damaged)(void *),
                                  void *closure)
{
    screen->damaged = damaged;
    screen->damaged_closure = closure;
}

void twin_screen_enable_update(twin_screen_t *screen)
{
    if (--screen->disable == 0) {
        if (screen->damage.left < screen->damage.right &&
            screen->damage.top < screen->damage.bottom) {
            if (screen->damaged)
                (*screen->damaged)(screen->damaged_closure);
        }
    }
}

void twin_screen_disable_update(twin_screen_t *screen)
{
    screen->disable++;
}

void twin_screen_damage(twin_screen_t *screen,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom)
{
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > screen->width)
        right = screen->width;
    if (bottom > screen->height)
        bottom = screen->height;

    if (screen->damage.left == screen->damage.right) {
        screen->damage.left = left;
        screen->damage.right = right;
        screen->damage.top = top;
        screen->damage.bottom = bottom;
    } else {
        if (left < screen->damage.left)
            screen->damage.left = left;
        if (top < screen->damage.top)
            screen->damage.top = top;
        if (screen->damage.right < right)
            screen->damage.right = right;
        if (screen->damage.bottom < bottom)
            screen->damage.bottom = bottom;
    }
    if (screen->damaged && !screen->disable)
        (*screen->damaged)(screen->damaged_closure);
}

void twin_screen_resize(twin_screen_t *screen,
                        twin_coord_t width,
                        twin_coord_t height)
{
    screen->width = width;
    screen->height = height;
    twin_screen_damage(screen, 0, 0, screen->width, screen->height);
}

bool twin_screen_damaged(twin_screen_t *screen)
{
    return (screen->damage.left < screen->damage.right &&
            screen->damage.top < screen->damage.bottom);
}

static void twin_screen_span_pixmap(twin_screen_t maybe_unused *screen,
                                    twin_argb32_t *span,
                                    twin_pixmap_t *p,
                                    twin_coord_t y,
                                    twin_coord_t left,
                                    twin_coord_t right,
                                    twin_src_op op16,
                                    twin_src_op op32)
{
    twin_pointer_t dst;
    twin_source_u src;
    twin_coord_t p_left, p_right;

    /* bounds check in y */
    if (y < p->y)
        return;
    if (p->y + p->height <= y)
        return;
    /* bounds check in x*/
    p_left = left;
    if (p_left < p->x)
        p_left = p->x;
    p_right = right;
    if (p_right > p->x + p->width)
        p_right = p->x + p->width;
    if (p_left >= p_right)
        return;
    dst.argb32 = span + (p_left - left);
    src.p = twin_pixmap_pointer(p, p_left - p->x, y - p->y);
    if (p->format == TWIN_RGB16)
        op16(dst, src, p_right - p_left);
    else
        op32(dst, src, p_right - p_left);
}

void twin_screen_update(twin_screen_t *screen)
{
    twin_coord_t left = screen->damage.left;
    twin_coord_t top = screen->damage.top;
    twin_coord_t right = screen->damage.right;
    twin_coord_t bottom = screen->damage.bottom;
    twin_src_op pop16, pop32, bop32;

    pop16 = _twin_rgb16_source_argb32;
    pop32 = _twin_argb32_over_argb32;
    bop32 = _twin_argb32_source_argb32;

    if (right > screen->width)
        right = screen->width;
    if (bottom > screen->height)
        bottom = screen->height;

    if (!screen->disable && left < right && top < bottom) {
        twin_argb32_t *span;
        twin_pixmap_t *p;
        twin_coord_t y;
        twin_coord_t width = right - left;

        screen->damage.left = screen->damage.right = 0;
        screen->damage.top = screen->damage.bottom = 0;
        /* FIXME: what is the maximum number of lines? */
        span = malloc(width * sizeof(twin_argb32_t));
        if (!span)
            return;

        if (screen->put_begin)
            (*screen->put_begin)(left, top, right, bottom, screen->closure);
        for (y = top; y < bottom; y++) {
            if (screen->background) {
                twin_pointer_t dst;
                twin_source_u src;
                twin_coord_t p_left;
                twin_coord_t m_left;
                twin_coord_t p_this;
                twin_coord_t p_width = screen->background->width;
                twin_coord_t p_y = y % screen->background->height;

                for (p_left = left; p_left < right; p_left += p_this) {
                    dst.argb32 = span + (p_left - left);
                    m_left = p_left % p_width;
                    p_this = p_width - m_left;
                    if (p_left + p_this > right)
                        p_this = right - p_left;
                    src.p =
                        twin_pixmap_pointer(screen->background, m_left, p_y);
                    bop32(dst, src, p_this);
                }
            } else
                memset(span, 0xff, width * sizeof(twin_argb32_t));

            for (p = screen->bottom; p; p = p->up)
                twin_screen_span_pixmap(screen, span, p, y, left, right, pop16,
                                        pop32);

#if defined(CONFIG_CURSOR)
            if (screen->cursor)
                twin_screen_span_pixmap(screen, span, screen->cursor, y, left,
                                        right, pop16, pop32);
#endif

            (*screen->put_span)(left, y, right, span, screen->closure);
        }
        free(span);
    }
}

void twin_screen_set_active(twin_screen_t *screen, twin_pixmap_t *pixmap)
{
    twin_event_t ev;
    twin_pixmap_t *old = screen->active;
    screen->active = pixmap;
    if (old) {
        ev.kind = TwinEventDeactivate;
        twin_pixmap_dispatch(old, &ev);
    }
    if (pixmap) {
        ev.kind = TwinEventActivate;
        twin_pixmap_dispatch(pixmap, &ev);
    }
}

twin_pixmap_t *twin_screen_get_active(twin_screen_t *screen)
{
    return screen->active;
}

void twin_screen_set_background(twin_screen_t *screen, twin_pixmap_t *pixmap)
{
    if (screen->background)
        twin_pixmap_destroy(screen->background);
    screen->background = pixmap;
    twin_screen_damage(screen, 0, 0, screen->width, screen->height);
}

twin_pixmap_t *twin_screen_get_background(twin_screen_t *screen)
{
    return screen->background;
}

#if defined(CONFIG_CURSOR)
static void twin_screen_damage_cursor(twin_screen_t *screen)
{
    twin_screen_damage(screen, screen->cursor->x, screen->cursor->y,
                       screen->cursor->x + screen->cursor->width,
                       screen->cursor->y + screen->cursor->height);
}

void twin_screen_set_cursor(twin_screen_t *screen,
                            twin_pixmap_t *pixmap,
                            twin_fixed_t hotspot_x,
                            twin_fixed_t hotspot_y)
{
    twin_screen_disable_update(screen);

    if (screen->cursor)
        twin_screen_damage_cursor(screen);

    screen->cursor = pixmap;
    screen->curs_hx = hotspot_x;
    screen->curs_hy = hotspot_y;
    if (pixmap) {
        pixmap->x = screen->curs_x - hotspot_x;
        pixmap->y = screen->curs_y - hotspot_y;
        twin_screen_damage_cursor(screen);
    }

    twin_screen_enable_update(screen);
}

static void twin_screen_update_cursor(twin_screen_t *screen,
                                      twin_coord_t x,
                                      twin_coord_t y)
{
    twin_screen_disable_update(screen);

    if (screen->cursor)
        twin_screen_damage_cursor(screen);

    screen->curs_x = x;
    screen->curs_y = y;

    if (screen->cursor) {
        screen->cursor->x = screen->curs_x - screen->curs_hx;
        screen->cursor->y = screen->curs_y - screen->curs_hy;
        twin_screen_damage_cursor(screen);
    }

    twin_screen_enable_update(screen);
}
#endif /* CONFIG_CURSOR */

static void _twin_adj_mouse_evt(twin_event_t *event,
                                const twin_pixmap_t *pixmap)
{
    event->u.pointer.x = event->u.pointer.screen_x - pixmap->x;
    event->u.pointer.y = event->u.pointer.screen_y - pixmap->y;
}

bool twin_screen_dispatch(twin_screen_t *screen, twin_event_t *event)
{
    twin_pixmap_t *pixmap, *ntarget;

    if (screen->event_filter && screen->event_filter(screen, event))
        return true;

    switch (event->kind) {
    case TwinEventMotion:
    case TwinEventButtonDown:
    case TwinEventButtonUp:
#if defined(CONFIG_CURSOR)
        /* update mouse cursor */
        twin_screen_update_cursor(screen, event->u.pointer.screen_x,
                                  event->u.pointer.screen_y);
#endif

        /* if target is tracking the mouse, check for mouse up, if not,
         * just pass the event along
         */
        pixmap = screen->target;
        if (screen->clicklock && event->kind != TwinEventButtonUp)
            goto mouse_passup;

        /* if event is mouse up, stop tracking if any */
        if (event->kind == TwinEventButtonUp)
            screen->clicklock = 0;

        /* check who the mouse is over now */
        for (ntarget = screen->top; ntarget; ntarget = ntarget->down)
            if (!twin_pixmap_transparent(ntarget, event->u.pointer.screen_x,
                                         event->u.pointer.screen_y))
                break;

        /* ah, somebody new ... send leave/enter events and set new target */
        if (pixmap != ntarget) {
            twin_event_t evt;

            if (pixmap) {
                evt = *event;
                evt.kind = TwinEventLeave;
                _twin_adj_mouse_evt(&evt, pixmap);
                if (!pixmap->shadow)
                    twin_pixmap_dispatch(pixmap, &evt);
            }

            pixmap = screen->target = ntarget;

            if (pixmap) {
                evt = *event;
                _twin_adj_mouse_evt(&evt, pixmap);
                evt.kind = TwinEventEnter;
                if (!pixmap->shadow)
                    twin_pixmap_dispatch(pixmap, &evt);
            }
        }

        /* check for new click locking */
        if (pixmap && event->kind == TwinEventButtonDown)
            screen->clicklock = 1;

    mouse_passup:
        /* adjust event to pixmap coordinates before passing up */
        if (pixmap)
            _twin_adj_mouse_evt(event, pixmap);
        break;

    case TwinEventKeyDown:
    case TwinEventKeyUp:
    case TwinEventUcs4:
        pixmap = screen->active;
        break;

    default:
        pixmap = NULL;
        break;
    }
    if (pixmap && !pixmap->shadow)
        return twin_pixmap_dispatch(pixmap, event);
    return false;
}
