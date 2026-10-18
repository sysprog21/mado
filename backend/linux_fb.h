/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Shared core of the Linux framebuffer backends (fbdev and DRM): the
 * virtual terminal, input, the mapped framebuffer and its span writers,
 * the redisplay work and the idle wait.  A backend's private struct embeds
 * twin_linux_fb_t as its first member and supplies how to map the
 * framebuffer again after a VT switch, plus an optional flush.
 */

#ifndef _LINUX_FB_H_
#define _LINUX_FB_H_

#include <string.h>
#include <sys/mman.h>

#include "linux_input.h"
#include "linux_vt.h"
#include "twin_private.h"

typedef struct twin_linux_fb {
    twin_linux_input_t *input;

    int vt_fd;
    volatile sig_atomic_t vt_away; /* switched away: set by the VT signal */
    struct vt_mode old_vtm;

    uint8_t *fb_base; /* MAP_FAILED while switched away */
    size_t fb_len;
    size_t stride; /* bytes per row */

    /* Map the framebuffer again on return to the VT */
    bool (*remap)(struct twin_linux_fb *fb);
    /* Push a finished frame to the display; NULL when writes show */
    void (*flush)(struct twin_linux_fb *fb);
} twin_linux_fb_t;

#define LINUX_FB(ctx) ((twin_linux_fb_t *) ((twin_context_t *) (ctx))->priv)

static inline uint8_t *_twin_linux_fb_row(void *closure,
                                          twin_coord_t left,
                                          twin_coord_t top,
                                          int bytes)
{
    twin_linux_fb_t *fb = LINUX_FB(closure);
    return fb->fb_base + (size_t) top * fb->stride + (size_t) left * bytes;
}

static void _twin_linux_fb_span16(twin_coord_t left,
                                  twin_coord_t top,
                                  twin_coord_t right,
                                  twin_argb32_t *pixels,
                                  void *closure)
{
    uint16_t *d = (uint16_t *) _twin_linux_fb_row(closure, left, top, 2);
    for (twin_coord_t i = 0; i < right - left; i++)
        d[i] = twin_argb32_to_rgb16(pixels[i]);
}

/* Blue first, as little-endian RGB888 framebuffers lay it out */
static void _twin_linux_fb_span24(twin_coord_t left,
                                  twin_coord_t top,
                                  twin_coord_t right,
                                  twin_argb32_t *pixels,
                                  void *closure)
{
    uint8_t *d = _twin_linux_fb_row(closure, left, top, 3);
    for (twin_coord_t i = 0; i < right - left; i++, d += 3) {
        d[0] = (uint8_t) pixels[i];
        d[1] = (uint8_t) (pixels[i] >> 8);
        d[2] = (uint8_t) (pixels[i] >> 16);
    }
}

static void _twin_linux_fb_span32(twin_coord_t left,
                                  twin_coord_t top,
                                  twin_coord_t right,
                                  twin_argb32_t *pixels,
                                  void *closure)
{
    memcpy(_twin_linux_fb_row(closure, left, top, 4), pixels,
           (size_t) (right - left) * sizeof(*pixels));
}

/* Span writer for @bpp (16, 24 or 32), NULL for any other depth */
static inline twin_put_span_t twin_linux_fb_span(int bpp)
{
    switch (bpp) {
    case 16:
        return _twin_linux_fb_span16;
    case 24:
        return _twin_linux_fb_span24;
    case 32:
        return _twin_linux_fb_span32;
    default:
        return NULL;
    }
}

/* Drop the mapping, if any */
static inline void twin_linux_fb_unmap(twin_linux_fb_t *fb)
{
    if (fb->fb_base != MAP_FAILED)
        munmap(fb->fb_base, fb->fb_len);
    fb->fb_base = MAP_FAILED;
}

static bool _twin_linux_fb_work(void *closure)
{
    twin_linux_fb_t *fb = LINUX_FB(closure);
    twin_screen_t *screen = ((twin_context_t *) closure)->screen;

    if (fb->vt_away)
        twin_linux_fb_unmap(fb);

    if (!fb->vt_away && fb->fb_base == MAP_FAILED) {
        if (!fb->remap(fb))
            log_error("Failed to map the framebuffer again");
        /* Whoever had the VT drew over everything */
        twin_screen_damage(screen, 0, 0, screen->width, screen->height);
    }

    /* No mapping (restoring it failed): keep the damage for the next try */
    if (!fb->vt_away && fb->fb_base != MAP_FAILED &&
        twin_screen_damaged(screen)) {
        twin_screen_update(screen);
        if (fb->flush)
            fb->flush(fb);
    }
    return true;
}

/* Take the VT for graphics; on failure nothing is left to undo */
static inline bool twin_linux_fb_take_vt(twin_linux_fb_t *fb)
{
    if (!twin_vt_setup(&fb->vt_fd, &fb->old_vtm, &fb->vt_away))
        return false;
    twin_vt_setup_signal_handler();
    return true;
}

static inline void twin_linux_fb_release_vt(twin_linux_fb_t *fb)
{
    twin_vt_restore(fb->vt_fd, &fb->old_vtm);
    close(fb->vt_fd);
}

/* Create the screen over the mapped framebuffer, input and redisplay
 * work; on failure nothing created here is left behind */
static inline bool twin_linux_fb_start(twin_context_t *ctx,
                                       int width,
                                       int height,
                                       int bpp)
{
    twin_linux_fb_t *fb = LINUX_FB(ctx);
    twin_put_span_t span = twin_linux_fb_span(bpp);
    if (!span) {
        log_error("Unsupported bits per pixel: %d", bpp);
        return false;
    }
    ctx->screen = twin_screen_create(width, height, NULL, span, ctx);
    if (!ctx->screen) {
        log_error("Failed to create screen");
        return false;
    }
    fb->input = twin_linux_input_create(ctx->screen);
    if (!fb->input) {
        log_error("Failed to create Linux input system object");
        twin_screen_destroy(ctx->screen);
        ctx->screen = NULL;
        return false;
    }
    twin_set_work(_twin_linux_fb_work, TWIN_WORK_REDISPLAY, ctx);
    return true;
}

/* Undo twin_linux_fb_start() and give the VT back */
static inline void twin_linux_fb_stop(twin_linux_fb_t *fb)
{
    twin_linux_input_destroy(fb->input);
    twin_linux_fb_release_vt(fb);
}

/* Retry period for a framebuffer that could not be mapped again */
#define TWIN_LINUX_FB_RETRY_MS 250

/* Sleep in the input poll until input arrives or there is something to
 * show; input is dispatched on this thread.  While the framebuffer is
 * gone, damage cannot be shown and does not end the wait: only timers,
 * the VT signal, or a retry of a failed mapping do. */
static bool twin_linux_fb_poll(twin_context_t *ctx)
{
    twin_linux_fb_t *fb = LINUX_FB(ctx);
    twin_time_t wait;
    if (fb->vt_away)
        wait = _twin_idle_timeout(NULL, -1);
    else if (fb->fb_base == MAP_FAILED)
        wait = _twin_idle_timeout(NULL, TWIN_LINUX_FB_RETRY_MS);
    else
        wait = _twin_idle_timeout(ctx->screen, -1);
    twin_linux_input_poll(fb->input, (int) wait);
    return true;
}

#endif /* _LINUX_FB_H_ */
