/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#define TWIN_ACTIVE_BG 0xd03b80ae
#define TWIN_INACTIVE_BG 0xff808080
#define TWIN_FRAME_TEXT 0xffffffff
#define TWIN_ACTIVE_BORDER 0xff606060
#define TWIN_BW 0
#define TWIN_TITLE_HEIGHT 20
#define TWIN_RESIZE_SIZE ((TWIN_TITLE_HEIGHT + 4) / 5)
#define TWIN_TITLE_BW ((TWIN_TITLE_HEIGHT + 11) / 12)

twin_window_t *twin_window_create(twin_screen_t *screen,
                                  twin_format_t format,
                                  twin_window_style_t style,
                                  twin_coord_t x,
                                  twin_coord_t y,
                                  twin_coord_t width,
                                  twin_coord_t height,
                                  bool shadow)
{
    twin_window_t *window = malloc(sizeof(twin_window_t));
    twin_coord_t left, top, right, bottom;

    if (!window)
        return NULL;
    window->screen = screen;
    window->style = style;
    switch (window->style) {
    case TwinWindowApplication:
        left = TWIN_BW;
        top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
        right = TWIN_BW + TWIN_RESIZE_SIZE;
        bottom = TWIN_BW + TWIN_RESIZE_SIZE;
        break;
    case TwinWindowPlain:
    default:
        left = 0;
        top = 0;
        right = 0;
        bottom = 0;
        break;
    }
    width += left + right;
    height += top + bottom;
    window->client.left = left;
    window->client.top = top;
    window->client.right = width - right;
    window->client.bottom = height - bottom;
    window->pixmap = twin_pixmap_create(format, width, height);
    twin_pixmap_clip(window->pixmap, window->client.left, window->client.top,
                     window->client.right, window->client.bottom);
    twin_pixmap_origin_to_clip(window->pixmap);
    window->pixmap->window = window;
    twin_pixmap_move(window->pixmap, x, y);
    window->shadow = shadow;
    if (window->shadow) {
        window->shadow_offset_x = 50, window->shadow_offset_y = 50;
        window->shadow_color = 0x55000000;
        window->shadow_pixmap = twin_pixmap_create(format, width, height);
        if (!window->shadow_pixmap)
            return NULL;
        window->shadow_pixmap->shadow = true;
        twin_pixmap_move(window->shadow_pixmap, x + window->shadow_offset_x,
                         y + window->shadow_offset_y);
        twin_shadow_visible(window->shadow_pixmap, window);
    } else
        window->shadow_pixmap = NULL;
    window->damage = window->client;
    window->client_grab = false;
    window->want_focus = false;
    window->draw_queued = false;
    window->client_data = 0;
    window->name = 0;

    window->draw = 0;
    window->event = 0;
    window->destroy = 0;
    return window;
}

void twin_window_destroy(twin_window_t *window)
{
    twin_window_hide(window);
    twin_pixmap_destroy(window->pixmap);
    if (window->shadow)
        twin_pixmap_destroy(window->shadow_pixmap);
    free(window->name);
    free(window);
}

void twin_window_show(twin_window_t *window)
{
    if (window->shadow)
        twin_pixmap_show(window->shadow_pixmap, window->screen,
                         window->screen->top);
    if (window->pixmap != window->screen->top)
        twin_pixmap_show(window->pixmap, window->screen, window->screen->top);
}

void twin_shadow_visible(twin_pixmap_t *pixmap, twin_window_t *window)
{
    twin_fill(window->shadow_pixmap, 0x55000000, TWIN_OVER, 0, 0, pixmap->width,
              pixmap->height);
}

void twin_window_hide(twin_window_t *window)
{
    twin_pixmap_hide(window->pixmap);
    if (window->shadow)
        twin_pixmap_hide(window->shadow_pixmap);
}

void twin_window_configure(twin_window_t *window,
                           twin_window_style_t style,
                           twin_coord_t x,
                           twin_coord_t y,
                           twin_coord_t width,
                           twin_coord_t height)
{
    bool need_repaint = false;

    twin_pixmap_disable_update(window->pixmap);
    if (window->shadow)
        twin_pixmap_disable_update(window->shadow_pixmap);

    if (style != window->style) {
        window->style = style;
        need_repaint = true;
    }
    if (width != window->pixmap->width || height != window->pixmap->height) {
        twin_pixmap_t *old = window->pixmap;
        int i;

        window->pixmap = twin_pixmap_create(old->format, width, height);
        window->pixmap->window = window;
        twin_pixmap_move(window->pixmap, x, y);
        if (window->shadow)
            twin_pixmap_move(window->shadow_pixmap, x + window->shadow_offset_x,
                             y + window->shadow_offset_y);
        if (old->screen)
            twin_pixmap_show(window->pixmap, window->screen, old);
        for (i = 0; i < old->disable; i++)
            twin_pixmap_disable_update(window->pixmap);
        twin_pixmap_destroy(old);
        twin_pixmap_reset_clip(window->pixmap);
        twin_pixmap_clip(window->pixmap, window->client.left,
                         window->client.top, window->client.right,
                         window->client.bottom);
        twin_pixmap_origin_to_clip(window->pixmap);

        if (window->shadow) {
            window->shadow_pixmap =
                twin_pixmap_create(old->format, width, height);
            if (!window->shadow_pixmap)
                return;
            window->shadow_pixmap->shadow = true;
            twin_pixmap_move(window->shadow_pixmap, x + window->shadow_offset_x,
                             y + window->shadow_offset_y);
            twin_shadow_visible(window->shadow_pixmap, window);
        }
    }
    if (x != window->pixmap->x || y != window->pixmap->y) {
        twin_pixmap_move(window->pixmap, x, y);
        if (window->shadow)
            twin_pixmap_move(window->shadow_pixmap, x + window->shadow_offset_x,
                             y + window->shadow_offset_y);
    }
    if (need_repaint)
        twin_window_draw(window);
    twin_pixmap_enable_update(window->pixmap);
    if (window->shadow)
        twin_pixmap_enable_update(window->shadow_pixmap);
}

void twin_window_style_size(twin_window_style_t style, twin_rect_t *size)
{
    switch (style) {
    case TwinWindowPlain:
    default:
        size->left = size->right = size->top = size->bottom = 0;
        break;
    case TwinWindowApplication:
        size->left = TWIN_BW;
        size->right = TWIN_BW;
        size->top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
        size->bottom = TWIN_BW;
        break;
    }
}

void twin_window_set_name(twin_window_t *window, const char *name)
{
    free(window->name);
    window->name = malloc(strlen(name) + 1);
    if (window->name)
        strcpy(window->name, name);
    twin_window_draw(window);
}

static void twin_window_frame(twin_window_t *window)
{
    twin_fixed_t bw = twin_int_to_fixed(TWIN_TITLE_BW);
    twin_path_t *path;
    twin_fixed_t bw_2 = bw / 2;
    twin_pixmap_t *pixmap = window->pixmap;
    twin_fixed_t w_top = bw_2;
    twin_fixed_t c_left = bw_2;
    twin_fixed_t t_h = twin_int_to_fixed(window->client.top) - bw;
    twin_fixed_t t_arc_1 = t_h / 3;
    twin_fixed_t t_arc_2 = t_h * 2 / 3;
    twin_fixed_t c_right = twin_int_to_fixed(window->client.right) - bw_2;
    twin_fixed_t c_top = twin_int_to_fixed(window->client.top) - bw_2;

    twin_fixed_t name_height = t_h - bw - bw_2;
    twin_fixed_t icon_size = name_height * 8 / 10;
    twin_fixed_t icon_y =
        (twin_int_to_fixed(window->client.top) - icon_size) / 2;
    twin_fixed_t menu_x = t_arc_2;
    twin_fixed_t text_x = menu_x + icon_size + bw;
    twin_fixed_t text_y = icon_y + icon_size;
    twin_fixed_t text_width;
    twin_fixed_t title_right;
    twin_fixed_t close_x;
    twin_fixed_t max_x;
    twin_fixed_t min_x;
    twin_fixed_t resize_x;
    twin_fixed_t resize_y;
    const char *name;

    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_origin_to_clip(pixmap);

    twin_fill(pixmap, 0x00000000, TWIN_SOURCE, 0, 0, pixmap->width,
              window->client.top);

    if (window->shadow)
        twin_shadow_visible(window->shadow_pixmap, window);
    path = twin_path_create();


    /* name */
    name = window->name;
    if (!name)
        name = "twin";
    twin_path_set_font_size(path, name_height);
    twin_path_set_font_style(path, TwinStyleOblique | TwinStyleUnhinted);
    text_width = twin_width_utf8(path, name);

    title_right = (text_x + text_width + bw + icon_size + bw + icon_size + bw +
                   icon_size + t_arc_2);

    if (title_right < c_right)
        c_right = title_right;


    close_x = c_right - t_arc_2 - icon_size;
    max_x = close_x - bw - icon_size;
    min_x = max_x - bw - icon_size;
    resize_x = twin_int_to_fixed(window->client.right);
    resize_y = twin_int_to_fixed(window->client.bottom);

    /* border */

    twin_path_move(path, c_left, c_top);
    twin_path_draw(path, c_right, c_top);
    twin_path_curve(path, c_right, w_top + t_arc_1, c_right - t_arc_1, w_top,
                    c_right - t_h, w_top);
    twin_path_draw(path, c_left + t_h, w_top);
    twin_path_curve(path, c_left + t_arc_1, w_top, c_left, w_top + t_arc_1,
                    c_left, c_top);
    twin_path_close(path);

    twin_paint_path(pixmap, TWIN_ACTIVE_BG, path);

    twin_paint_stroke(pixmap, TWIN_ACTIVE_BORDER, path, bw_2 * 2);

    twin_path_empty(path);

    twin_pixmap_clip(pixmap, twin_fixed_to_int(twin_fixed_floor(menu_x)), 0,
                     twin_fixed_to_int(twin_fixed_ceil(c_right - t_arc_2)),
                     window->client.top);
    twin_pixmap_origin_to_clip(pixmap);

    twin_path_move(path, text_x - twin_fixed_floor(menu_x), text_y);
    twin_path_utf8(path, window->name);
    twin_paint_path(pixmap, TWIN_FRAME_TEXT, path);

    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_origin_to_clip(pixmap);

    /* widgets */

    {
        twin_matrix_t m;

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, menu_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconMenu, m);

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, min_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconMinimize, m);

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, max_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconMaximize, m);

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, close_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconClose, m);

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, resize_x, resize_y);
        twin_matrix_scale(&m, twin_int_to_fixed(TWIN_TITLE_HEIGHT),
                          twin_int_to_fixed(TWIN_TITLE_HEIGHT));
        twin_icon_draw(pixmap, TwinIconResize, m);
    }

    twin_pixmap_clip(pixmap, window->client.left, window->client.top,
                     window->client.right, window->client.bottom);
    twin_pixmap_origin_to_clip(pixmap);

    twin_path_destroy(path);
}

void twin_window_draw(twin_window_t *window)
{
    twin_pixmap_t *pixmap = window->pixmap;

    switch (window->style) {
    case TwinWindowPlain:
    default:
        break;
    case TwinWindowApplication:
        twin_window_frame(window);
        break;
    }

    /* if no draw function or no damage, return */
    if (window->draw == NULL || (window->damage.left >= window->damage.right ||
                                 window->damage.top >= window->damage.bottom))
        return;

    /* clip to damaged area and draw */
    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_clip(pixmap, window->damage.left, window->damage.top,
                     window->damage.right, window->damage.bottom);
    twin_screen_disable_update(window->screen);

    if (window->shadow)
        twin_pixmap_disable_update(window->shadow_pixmap);
    (*window->draw)(window);

    /* damage matching screen area */
    twin_pixmap_damage(pixmap, window->damage.left, window->damage.top,
                       window->damage.right, window->damage.bottom);

    if (window->shadow)
        twin_pixmap_damage(window->shadow_pixmap, window->damage.left,
                           window->damage.top, window->damage.right,
                           window->damage.bottom);
    twin_screen_enable_update(window->screen);

    /* clear damage and restore clip */
    window->damage.left = window->damage.right = 0;
    window->damage.top = window->damage.bottom = 0;
    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_clip(pixmap, window->client.left, window->client.top,
                     window->client.right, window->client.bottom);
    if (window->shadow)
        twin_pixmap_enable_update(window->shadow_pixmap);
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

    if (window->damage.left == window->damage.right) {
        window->damage.left = left;
        window->damage.right = right;
        window->damage.top = top;
        window->damage.bottom = bottom;
    } else {
        if (left < window->damage.left)
            window->damage.left = left;
        if (top < window->damage.top)
            window->damage.top = top;
        if (window->damage.right < right)
            window->damage.right = right;
        if (window->damage.bottom < bottom)
            window->damage.bottom = bottom;
    }
}

static bool _twin_window_repaint(void *closure)
{
    twin_window_t *window = closure;

    window->draw_queued = false;
    twin_window_draw(window);

    return false;
}

void twin_window_queue_paint(twin_window_t *window)
{
    if (!window->draw_queued) {
        window->draw_queued = true;
        twin_set_work(_twin_window_repaint, TWIN_WORK_PAINT, window);
    }
}

bool twin_window_dispatch(twin_window_t *window, twin_event_t *event)
{
    twin_event_t ev = *event;
    bool delegate = true;

    switch (ev.kind) {
    case TwinEventButtonDown:
        if (window->client.left <= ev.u.pointer.x &&
            ev.u.pointer.x < window->client.right &&
            window->client.top <= ev.u.pointer.y &&
            ev.u.pointer.y < window->client.bottom) {
            delegate = true;
            window->client_grab = true;
            ev.u.pointer.x -= window->client.left;
            ev.u.pointer.y -= window->client.top;
        } else
            delegate = false;
        break;
    case TwinEventButtonUp:
        if (window->client_grab) {
            delegate = true;
            window->client_grab = false;
            ev.u.pointer.x -= window->client.left;
            ev.u.pointer.y -= window->client.top;
        } else
            delegate = false;
        break;
    case TwinEventMotion:
        if (window->client_grab || (window->client.left <= ev.u.pointer.x &&
                                    ev.u.pointer.x < window->client.right &&
                                    window->client.top <= ev.u.pointer.y &&
                                    ev.u.pointer.y < window->client.bottom)) {
            delegate = true;
            ev.u.pointer.x -= window->client.left;
            ev.u.pointer.y -= window->client.top;
        } else
            delegate = false;
        break;
    default:
        break;
    }
    if (!window->event)
        delegate = false;

    if (delegate && (*window->event)(window, &ev))
        return true;

    /*
     * simple window management
     */
    switch (event->kind) {
    case TwinEventButtonDown:
        twin_window_show(window);
        window->screen->button_x = event->u.pointer.x;
        window->screen->button_y = event->u.pointer.y;
        return true;
    case TwinEventButtonUp:
        window->screen->button_x = -1;
        window->screen->button_y = -1;
        return true;
    case TwinEventMotion:
        if (window->screen->button_x >= 0) {
            twin_coord_t x, y;

            x = event->u.pointer.screen_x - window->screen->button_x;
            y = event->u.pointer.screen_y - window->screen->button_y;
            twin_window_configure(window, window->style, x, y,
                                  window->pixmap->width,
                                  window->pixmap->height);
        }
        return true;
    default:
        break;
    }
    return false;
}
