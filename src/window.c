/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#define TWIN_ACTIVE_BG 0xd03b80ae
#define TWIN_INACTIVE_BG 0xffb0b0b0
#define TWIN_FRAME_TEXT 0xffffffff
#define TWIN_ACTIVE_BORDER 0xff606060
#define TWIN_INACTIVE_BORDER 0xff909090
#define TWIN_BW 0
#define TWIN_TITLE_HEIGHT 20
#define TWIN_RESIZE_SIZE ((TWIN_TITLE_HEIGHT + 4) / 5)
#define TWIN_TITLE_BW ((TWIN_TITLE_HEIGHT + 11) / 12)
#define SHADOW_COLOR 0xff000000

twin_window_t *twin_window_create(twin_screen_t *screen,
                                  twin_format_t format,
                                  twin_window_style_t style,
                                  twin_coord_t x,
                                  twin_coord_t y,
                                  twin_coord_t width,
                                  twin_coord_t height)
{
    twin_window_t *window = malloc(sizeof(twin_window_t));
    twin_coord_t left, top, right, bottom;

    if (!window)
        return NULL;
    window->screen = screen;
    window->style = style;
    window->active = false;
    window->iconify = false;
    switch (window->style) {
    case TwinWindowApplication:
        left = TWIN_BW;
        top = TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW;
        right = TWIN_BW;
        bottom = TWIN_BW;
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
#if defined(CONFIG_DROP_SHADOW)
    /* Handle drop shadow. */
    /*
     * Add a shadowed area to the pixel map of the window to create a drop
     * shadow effect. This calculation method is based on the range of
     * CONFIG_HORIZONTAL_OFFSET, CONFIG_VERTICAL_OFFSET, and CONFIG_SHADOW_BLUR.
     */
    window->shadow_x = 2 * CONFIG_HORIZONTAL_OFFSET + CONFIG_SHADOW_BLUR;
    window->shadow_y = 2 * CONFIG_VERTICAL_OFFSET + CONFIG_SHADOW_BLUR;
    window->pixmap = twin_pixmap_create(format, width + window->shadow_x,
                                        height + window->shadow_y);
#else
    window->pixmap = twin_pixmap_create(format, width, height);
#endif
    if (!window->pixmap)
        return NULL;
    twin_pixmap_clip(window->pixmap, window->client.left, window->client.top,
                     window->client.right, window->client.bottom);
    twin_pixmap_origin_to_clip(window->pixmap);
    window->pixmap->window = window;
    twin_pixmap_move(window->pixmap, x, y);
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

    /* Call the destroy callback if set to clean up window contents */
    if (window->destroy)
        (*window->destroy)(window);

    twin_pixmap_destroy(window->pixmap);
    free(window->name);
    free(window);
}

void twin_window_show(twin_window_t *window)
{
    if (window->pixmap != window->screen->top)
        twin_pixmap_show(window->pixmap, window->screen, window->screen->top);
}

void twin_window_hide(twin_window_t *window)
{
    twin_pixmap_hide(window->pixmap);
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
    }
    if (x != window->pixmap->x || y != window->pixmap->y)
        twin_pixmap_move(window->pixmap, x, y);
    if (need_repaint)
        twin_window_draw(window);
    twin_pixmap_enable_update(window->pixmap);
}

bool twin_window_valid_range(twin_window_t *window,
                             twin_coord_t x,
                             twin_coord_t y)
{
    twin_coord_t offset_x = 0, offset_y = 0;
#if defined(CONFIG_DROP_SHADOW)
    /* Handle drop shadow. */
    /*
     * When the click coordinates fall within the drop shadow area, it will not
     * be in the valid range of the window.
     */
    offset_x = window->shadow_x, offset_y = window->shadow_y;
#endif

    switch (window->style) {
    case TwinWindowPlain:
    default:
        if (window->pixmap->x <= x &&
            x < window->pixmap->x + window->pixmap->width - offset_x &&
            window->pixmap->y <= y &&
            y < window->pixmap->y + window->pixmap->height - offset_y)
            return true;
        return false;
    case TwinWindowApplication:
        if (window->pixmap->x <= x &&
            x < window->pixmap->x + window->pixmap->width - offset_x &&
            window->pixmap->y <= y &&
            y < window->pixmap->y + window->pixmap->height - offset_y) {
            if (y < window->pixmap->y + (window->client.top))
                return !twin_pixmap_transparent(window->pixmap, x, y);
            return !window->iconify;
        }
        return false;
    }
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
    else
        window->name = NULL; /* Ensure consistent state on allocation failure */
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
    twin_fixed_t restore_x;
    twin_fixed_t iconify_x;
    twin_fixed_t resize_x;
    twin_fixed_t resize_y;
    const char *name;

    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_origin_to_clip(pixmap);

    twin_fill(pixmap, 0x00000000, TWIN_SOURCE, 0, 0, pixmap->width,
              window->client.top);

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
    restore_x = close_x - bw - icon_size;
    iconify_x = restore_x - bw - icon_size;
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

    if (window->active) {
        twin_paint_path(pixmap, TWIN_ACTIVE_BG, path);
        twin_paint_stroke(pixmap, TWIN_ACTIVE_BORDER, path, bw_2 * 2);
    } else {
        twin_paint_path(pixmap, TWIN_INACTIVE_BG, path);
        twin_paint_stroke(pixmap, TWIN_INACTIVE_BORDER, path, bw_2 * 2);
    }

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
        twin_matrix_translate(&m, iconify_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconIconify, m);

        twin_matrix_identity(&m);
        twin_matrix_translate(&m, restore_x, icon_y);
        twin_matrix_scale(&m, icon_size, icon_size);
        twin_icon_draw(pixmap, TwinIconRestore, m);

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

#if defined(CONFIG_DROP_SHADOW)
static void twin_window_drop_shadow(twin_window_t *window)
{
    twin_pixmap_t *prev_active_pix = window->screen->top,
                  *active_pix = window->pixmap;
    twin_source_u src;
    twin_coord_t y, ori_wid, ori_hei;

    /* Remove the drop shadow from the previously active pixel map. */
    if (prev_active_pix) {
        src.c = 0x00000000;
        for (y = 0; y < prev_active_pix->height; y++) {
            if (y < prev_active_pix->height - prev_active_pix->window->shadow_y)
                twin_cover(
                    prev_active_pix, src.c,
                    prev_active_pix->width - prev_active_pix->window->shadow_x,
                    y, prev_active_pix->window->shadow_x);
            else
                twin_cover(prev_active_pix, src.c, 0, y,
                           prev_active_pix->width);
        }
        prev_active_pix->shadow = false;
    }

    /* Mark the previously active pixel map as damaged to update its changes. */
    if (prev_active_pix && active_pix != prev_active_pix)
        twin_pixmap_damage(prev_active_pix, 0, 0, prev_active_pix->width,
                           prev_active_pix->height);

    /*
     * The shadow effect of the window only becomes visible when the window is
     * active.
     */
    active_pix->shadow = true;
    ori_wid = active_pix->width - active_pix->window->shadow_x;
    ori_hei = active_pix->height - active_pix->window->shadow_y;
    /*
     * Create a darker border of the active window that gives a more
     * dimensional appearance.
     */
    /* The shift offset and color of the shadow can be selected by the user. */
    twin_shadow_border(active_pix, SHADOW_COLOR, CONFIG_VERTICAL_OFFSET,
                       CONFIG_HORIZONTAL_OFFSET);

    /* Add a blur effect to the shadow of the active window. */
    /* Right side of the active window */
    twin_stack_blur(active_pix, CONFIG_SHADOW_BLUR, ori_wid,
                    ori_wid + active_pix->window->shadow_x, 0,
                    ori_hei + active_pix->window->shadow_y);
    /* Bottom side of the active window */
    twin_stack_blur(active_pix, CONFIG_SHADOW_BLUR, 0,
                    ori_wid + active_pix->window->shadow_x, ori_hei,
                    ori_hei + active_pix->window->shadow_y);
}
#endif

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

    (*window->draw)(window);

    /* damage matching screen area */
    twin_pixmap_damage(pixmap, window->damage.left, window->damage.top,
                       window->damage.right, window->damage.bottom);

    twin_screen_enable_update(window->screen);

    /* clear damage and restore clip */
    window->damage.left = window->damage.right = 0;
    window->damage.top = window->damage.bottom = 0;
    twin_pixmap_reset_clip(pixmap);
    twin_pixmap_clip(pixmap, window->client.left, window->client.top,
                     window->client.right, window->client.bottom);
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

    twin_fixed_t bw = twin_int_to_fixed(TWIN_TITLE_BW);
    twin_fixed_t t_h = twin_int_to_fixed(window->client.top) - bw;
    twin_fixed_t t_arc_2 = t_h * 2 / 3;
    twin_fixed_t c_right = twin_int_to_fixed(window->client.right) - bw / 2;
    twin_fixed_t name_height = t_h - bw - bw / 2;
    twin_fixed_t icon_size = name_height * 8 / 10;
    twin_fixed_t menu_x = t_arc_2;
    twin_fixed_t text_x = menu_x + icon_size + bw;
    twin_fixed_t text_width;
    twin_fixed_t title_right;
    twin_path_t *path = twin_path_create();
    const char *name = window->name;

    text_width = twin_width_utf8(path, name);
    twin_path_destroy(path);
    title_right = (text_x + text_width + bw + icon_size + bw + icon_size + bw +
                   icon_size + t_arc_2);

    if (title_right < c_right)
        c_right = title_right;

    twin_fixed_t close_x = c_right - t_arc_2 - icon_size;
    twin_fixed_t restore_x = close_x - bw - icon_size;
    twin_fixed_t iconify_x = restore_x - bw - icon_size;
    int local_x, local_y;

    switch (ev.kind) {
    case TwinEventButtonDown:
        local_y = ev.u.pointer.screen_y - window->pixmap->y;
        if (local_y >= 0 && local_y <= TWIN_BW + TWIN_TITLE_HEIGHT + TWIN_BW) {
            local_x = ev.u.pointer.screen_x - window->pixmap->x;
            if (local_x > twin_fixed_to_int(iconify_x) &&
                local_x < twin_fixed_to_int(restore_x)) {
                window->iconify = true;
                twin_pixmap_damage(window->pixmap, 0, 0, window->pixmap->width,
                                   window->pixmap->height);
            } else if (local_x > twin_fixed_to_int(restore_x) &&
                       local_x < twin_fixed_to_int(close_x)) {
                window->iconify = false;
                twin_pixmap_damage(window->pixmap, 0, 0, window->pixmap->width,
                                   window->pixmap->height);
            }
        }
    case TwinEventActivate:
        /* Set window active. */
        /*
         * A iconified window is inactive. When the box is triggered by
         * TwinEventButtonDown and the window is not iconified, it becomes
         * active. For a window to be considered active, it must be the topmost
         * window on the screen. The window's title bar turns blue to indicate
         * the active state.
         */
        if (window->iconify)
            window->active = false;
        else
            window->active = true;
        twin_window_frame(window);
        if (window != window->screen->top->window) {
            window->screen->top->window->active = false;
            twin_window_frame(window->screen->top->window);
        }
#if defined(CONFIG_DROP_SHADOW)
        /* Handle drop shadow. */
        twin_window_drop_shadow(window);
#endif
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
