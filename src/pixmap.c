/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

#define IS_ALIGNED(p, alignment) ((p % alignment) == 0)
#define ALIGN_UP(sz, alignment)                            \
    (((alignment) & ((alignment) - 1)) == 0                \
         ? (((sz) + (alignment) - 1) & ~((alignment) - 1)) \
         : ((((sz) + (alignment) - 1) / (alignment)) * (alignment)))

static bool twin_pixmap_alloc_size(twin_format_t format,
                                   twin_coord_t width,
                                   twin_coord_t height,
                                   size_t *size_out)
{
    if (width <= 0 || height <= 0)
        return false;

    size_t bpp = (size_t) twin_bytes_per_pixel(format);
    if (bpp == 0 || (size_t) width > SIZE_MAX / bpp)
        return false;
    size_t stride = bpp * (size_t) width;
    if (stride > SIZE_MAX - 3)
        return false;
    stride = ALIGN_UP(stride, 4);
    /* Stride must fit in twin_coord_t (int16_t) for the pixmap struct. */
    if (stride > INT16_MAX)
        return false;
    if ((size_t) height > (SIZE_MAX - sizeof(twin_pixmap_t)) / stride)
        return false;

    *size_out = sizeof(twin_pixmap_t) + stride * (size_t) height;
    return true;
}

void _twin_pixmap_init(twin_pixmap_t *pixmap,
                       twin_format_t format,
                       twin_coord_t width,
                       twin_coord_t height,
                       twin_coord_t stride,
                       void *pixels)
{
    memset(pixmap, 0, sizeof(*pixmap));
    pixmap->format = format;
    pixmap->width = width;
    pixmap->height = height;
    pixmap->stride = stride;
    twin_matrix_identity(&pixmap->transform);
    pixmap->clip.right = width;
    pixmap->clip.bottom = height;
    pixmap->scan_height = height;
    pixmap->opacity = 0xff;
    /* RGB16 has no alpha channel, so every pixel is opaque. */
    if (format == TWIN_RGB16)
        pixmap->opaque_rect = (twin_rect_t) {0, width, 0, height};
    pixmap->p.v = pixels;
}

twin_pixmap_t *twin_pixmap_create(twin_format_t format,
                                  twin_coord_t width,
                                  twin_coord_t height)
{
    size_t size;
    if (!twin_pixmap_alloc_size(format, width, height, &size))
        return NULL;

    twin_coord_t stride = (twin_coord_t) ALIGN_UP(
        (size_t) twin_bytes_per_pixel(format) * width, 4);

    twin_pixmap_t *pixmap = twin_malloc(size);
    if (!pixmap)
        return NULL;
    _twin_pixmap_init(pixmap, format, width, height, stride, pixmap + 1);
    memset(pixmap->p.v, 0, size - sizeof(twin_pixmap_t));
    return pixmap;
}

twin_pixmap_t *_twin_pixmap_create_stub(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height)
{
    twin_pixmap_t *pixmap = twin_malloc(sizeof(twin_pixmap_t));
    if (!pixmap)
        return NULL;
    _twin_pixmap_init(pixmap, format, width, height, 0, NULL);
    /* An empty clip turns every drawing operation into a no-op. */
    pixmap->clip = (twin_rect_t) {0, 0, 0, 0};
    pixmap->opaque_rect = (twin_rect_t) {0, 0, 0, 0};
    return pixmap;
}

void _twin_pixmap_init_a8(twin_pixmap_t *pixmap,
                          twin_coord_t width,
                          twin_coord_t height,
                          twin_coord_t stride,
                          void *pixels)
{
    _twin_pixmap_init(pixmap, TWIN_A8, width, height, stride, pixels);
}

twin_pixmap_t *twin_pixmap_create_const(twin_format_t format,
                                        twin_coord_t width,
                                        twin_coord_t height,
                                        twin_coord_t stride,
                                        twin_pointer_t pixels)
{
    twin_pixmap_t *pixmap = twin_malloc(sizeof(twin_pixmap_t));
    if (!pixmap)
        return NULL;
    _twin_pixmap_init(pixmap, format, width, height, stride, pixels.v);
    return pixmap;
}

void twin_pixmap_destroy(twin_pixmap_t *pixmap)
{
    if (pixmap->screen)
        twin_pixmap_hide(pixmap);
    twin_pixmap_reset_xform_cache(pixmap);
    _twin_animation_destroy(pixmap->animation);
    twin_free(pixmap);
}

void twin_pixmap_reset_xform_cache(twin_pixmap_t *pixmap)
{
    if (!pixmap)
        return;

    twin_free(pixmap->xform_cache);
    pixmap->xform_cache = NULL;
    pixmap->xform_cache_size = 0;
}

void _twin_pixmap_damage_extents(twin_pixmap_t *pixmap)
{
    if (!pixmap->screen)
        return;
    twin_rect_t e = _twin_pixmap_extents(pixmap);
    twin_screen_damage(pixmap->screen, e.left, e.top, e.right, e.bottom);
}

void twin_pixmap_show(twin_pixmap_t *pixmap,
                      twin_screen_t *screen,
                      twin_pixmap_t *lower)
{
    if (pixmap->disable)
        twin_screen_disable_update(screen);

    /* Linking above a pixmap shown elsewhere (or nowhere) would corrupt
     * the display list: stack on top instead. */
    if (lower && lower->screen != screen)
        lower = screen->top;
    if (lower == pixmap)
        lower = pixmap->down;

    if (pixmap->screen)
        _twin_pixmap_unlink(pixmap);

    pixmap->screen = screen;

    if (lower) {
        pixmap->down = lower;
        pixmap->up = lower->up;
        lower->up = pixmap;
        if (!pixmap->up)
            screen->top = pixmap;
        else
            pixmap->up->down = pixmap;
    } else {
        pixmap->down = NULL;
        pixmap->up = screen->bottom;
        if (screen->bottom)
            screen->bottom->down = pixmap;
        screen->bottom = pixmap;
        if (!pixmap->up)
            screen->top = pixmap;
    }

    _twin_pixmap_damage_extents(pixmap);
}

void _twin_pixmap_unlink(twin_pixmap_t *pixmap)
{
    twin_screen_t *screen = pixmap->screen;
    twin_pixmap_t **up, **down;

    if (!screen)
        return;

    _twin_pixmap_damage_extents(pixmap);

    if (pixmap->up)
        down = &pixmap->up->down;
    else
        down = &screen->top;

    if (pixmap->down)
        up = &pixmap->down->up;
    else
        up = &screen->bottom;

    *down = pixmap->down;
    *up = pixmap->up;

    pixmap->screen = 0;
    pixmap->up = 0;
    pixmap->down = 0;
    if (pixmap->disable)
        twin_screen_enable_update(screen);
}

void twin_pixmap_hide(twin_pixmap_t *pixmap)
{
    twin_screen_t *screen = pixmap->screen;
    if (!screen)
        return;
    _twin_pixmap_unlink(pixmap);
    /* Never leave the screen pointing at a pixmap it no longer shows. */
    _twin_screen_forget(screen, pixmap);
}

void _twin_screen_forget(twin_screen_t *screen, const twin_pixmap_t *pixmap)
{
    if (screen->active == pixmap)
        screen->active = NULL;
    if (screen->target == pixmap) {
        screen->target = NULL;
        screen->clicklock = false;
    }
}

void _twin_pixmap_set_scan_height(twin_pixmap_t *pixmap, twin_coord_t rows)
{
    if (rows < 0)
        rows = 0;
    if (rows > pixmap->height)
        rows = pixmap->height;
    if (pixmap->scan_height == rows)
        return;
    /* Damage both the old and the new extent */
    _twin_pixmap_damage_extents(pixmap);
    pixmap->scan_height = rows;
    _twin_pixmap_damage_extents(pixmap);
}

void twin_pixmap_set_opacity(twin_pixmap_t *pixmap, twin_a8_t opacity)
{
    if (pixmap->opacity == opacity)
        return;
    pixmap->opacity = opacity;
    _twin_pixmap_damage_extents(pixmap);
}

void twin_pixmap_set_shadow(twin_pixmap_t *pixmap,
                            twin_coord_t dx,
                            twin_coord_t dy,
                            twin_coord_t blur,
                            twin_a8_t alpha)
{
#if !defined(CONFIG_DROP_SHADOW)
    /* Built without shadow support */
    (void) pixmap, (void) dx, (void) dy, (void) blur, (void) alpha;
#else
    twin_shadow_t s = {0};
    if (blur > 0 && alpha) {
        s.dx = (int8_t) (dx > 127 ? 127 : dx < -127 ? -127 : dx);
        s.dy = (int8_t) (dy > 127 ? 127 : dy < -127 ? -127 : dy);
        s.blur = (uint8_t) (blur > 63 ? 63 : blur);
        s.alpha = alpha;
    }
    if (!memcmp(&s, &pixmap->shadow, sizeof(s)))
        return;
    _twin_pixmap_damage_extents(pixmap);
    pixmap->shadow = s;
    _twin_pixmap_damage_extents(pixmap);
#endif
}

twin_pointer_t twin_pixmap_pointer(twin_pixmap_t *pixmap,
                                   twin_coord_t x,
                                   twin_coord_t y)
{
    twin_pointer_t p;

    p.b = (pixmap->p.b + (int32_t) y * pixmap->stride +
           (int32_t) x * twin_bytes_per_pixel(pixmap->format));
    return p;
}

void twin_pixmap_enable_update(twin_pixmap_t *pixmap)
{
    if (--pixmap->disable == 0) {
        if (pixmap->screen)
            twin_screen_enable_update(pixmap->screen);
    }
}

void twin_pixmap_disable_update(twin_pixmap_t *pixmap)
{
    if (pixmap->disable++ == 0) {
        if (pixmap->screen)
            twin_screen_disable_update(pixmap->screen);
    }
}

void twin_pixmap_set_origin(twin_pixmap_t *pixmap,
                            twin_coord_t ox,
                            twin_coord_t oy)
{
    pixmap->origin_x = ox;
    pixmap->origin_y = oy;
}

void twin_pixmap_get_origin(twin_pixmap_t *pixmap,
                            twin_coord_t *ox,
                            twin_coord_t *oy)
{
    *ox = pixmap->origin_x;
    *oy = pixmap->origin_y;
}

void twin_pixmap_origin_to_clip(twin_pixmap_t *pixmap)
{
    pixmap->origin_x = pixmap->clip.left;
    pixmap->origin_y = pixmap->clip.top;
}

void twin_pixmap_clip(twin_pixmap_t *pixmap,
                      twin_coord_t left,
                      twin_coord_t top,
                      twin_coord_t right,
                      twin_coord_t bottom)
{
    left += pixmap->origin_x;
    right += pixmap->origin_x;
    top += pixmap->origin_y;
    bottom += pixmap->origin_y;

    if (left > pixmap->clip.left)
        pixmap->clip.left = left;
    if (top > pixmap->clip.top)
        pixmap->clip.top = top;
    if (right < pixmap->clip.right)
        pixmap->clip.right = right;
    if (bottom < pixmap->clip.bottom)
        pixmap->clip.bottom = bottom;

    if (pixmap->clip.left >= pixmap->clip.right)
        pixmap->clip.right = pixmap->clip.left = 0;
    if (pixmap->clip.top >= pixmap->clip.bottom)
        pixmap->clip.bottom = pixmap->clip.top = 0;

    if (pixmap->clip.left < 0)
        pixmap->clip.left = 0;
    if (pixmap->clip.top < 0)
        pixmap->clip.top = 0;
    if (pixmap->clip.right > pixmap->width)
        pixmap->clip.right = pixmap->width;
    if (pixmap->clip.bottom > pixmap->height)
        pixmap->clip.bottom = pixmap->height;
}

void twin_pixmap_set_clip(twin_pixmap_t *pixmap, twin_rect_t clip)
{
    twin_pixmap_clip(pixmap, clip.left, clip.top, clip.right, clip.bottom);
}


twin_rect_t twin_pixmap_get_clip(twin_pixmap_t *pixmap)
{
    twin_rect_t clip = pixmap->clip;

    clip.left -= pixmap->origin_x;
    clip.right -= pixmap->origin_x;
    clip.top -= pixmap->origin_y;
    clip.bottom -= pixmap->origin_y;

    return clip;
}

twin_rect_t twin_pixmap_save_clip(twin_pixmap_t *pixmap)
{
    return pixmap->clip;
}

void twin_pixmap_restore_clip(twin_pixmap_t *pixmap, twin_rect_t rect)
{
    pixmap->clip =
        _twin_pixmap_is_stub(pixmap) ? (twin_rect_t) {0, 0, 0, 0} : rect;
}

void twin_pixmap_reset_clip(twin_pixmap_t *pixmap)
{
    /* A stub has no storage: keep its clip empty so drawing is a no-op. */
    if (_twin_pixmap_is_stub(pixmap)) {
        pixmap->clip = (twin_rect_t) {0, 0, 0, 0};
        return;
    }
    pixmap->clip.left = 0;
    pixmap->clip.top = 0;
    pixmap->clip.right = pixmap->width;
    pixmap->clip.bottom = pixmap->height;
}

void twin_pixmap_damage(twin_pixmap_t *pixmap,
                        twin_coord_t left,
                        twin_coord_t top,
                        twin_coord_t right,
                        twin_coord_t bottom)
{
    if (pixmap->screen)
        twin_screen_damage(pixmap->screen, left + pixmap->x, top + pixmap->y,
                           right + pixmap->x, bottom + pixmap->y);
}

void twin_pixmap_move(twin_pixmap_t *pixmap, twin_coord_t x, twin_coord_t y)
{
    if (pixmap->x == x && pixmap->y == y)
        return;
    _twin_pixmap_damage_extents(pixmap);
    pixmap->x = x;
    pixmap->y = y;
    _twin_pixmap_damage_extents(pixmap);
}

bool twin_pixmap_dispatch(twin_pixmap_t *pixmap, twin_event_t *event)
{
    if (pixmap->window)
        return _twin_window_dispatch(pixmap->window, event);
    return false;
}

twin_coord_t twin_pixmap_width(const twin_pixmap_t *pixmap)
{
    return pixmap->width;
}

twin_coord_t twin_pixmap_height(const twin_pixmap_t *pixmap)
{
    return pixmap->height;
}

twin_format_t twin_pixmap_format(const twin_pixmap_t *pixmap)
{
    return pixmap->format;
}

void *twin_pixmap_pixels(twin_pixmap_t *pixmap, twin_coord_t *stride)
{
    if (stride)
        *stride = pixmap->stride;
    return pixmap->p.v;
}

twin_animation_t *twin_pixmap_animation(const twin_pixmap_t *pixmap)
{
    return pixmap->animation;
}

void twin_pixmap_set_transform(twin_pixmap_t *pixmap, const twin_matrix_t *m)
{
    if (m)
        pixmap->transform = *m;
    else
        twin_matrix_identity(&pixmap->transform);
}
