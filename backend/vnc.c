/*
Twin - A Tiny Window System
Copyright (c) 2024 National Cheng Kung University, Taiwan
All rights reserved.
*/
#define AML_UNSTABLE_API 1
#include <aml.h>
#include <assert.h>
#include <neatvnc.h>
#include <pixman.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>

#include "twin_backend.h"
#include "twin_private.h"

#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_vnc_t *) ((twin_context_t *) x)->priv)
#define MADO_VNC_HOST "MADO_VNC_HOST"
#define MADO_VNC_PORT "MADO_VNC_PORT"
#define MADO_VNC_HOST_DEFAULT "127.0.0.1"
#define MADO_VNC_PORT_DEFAULT "5900"

#ifndef DRM_FORMAT_ARGB8888
#define fourcc_code(a, b, c, d)                                        \
    ((uint32_t) (a) | ((uint32_t) (b) << 8) | ((uint32_t) (c) << 16) | \
     ((uint32_t) (d) << 24))
#define DRM_FORMAT_ARGB8888 fourcc_code('A', 'R', '2', '4')
#endif

typedef struct {
    twin_screen_t *screen;
    struct aml *aml;
    struct aml_handler *aml_handler;
    struct nvnc *server;
    struct nvnc_display *display;
    struct nvnc_fb *current_fb;
    struct pixman_region16 damage_region;
    uint32_t *framebuffer;
    int width;
    int height;
} twin_vnc_t;

typedef struct {
    uint16_t px, py;
    enum nvnc_button_mask prev_button;
} twin_peer_t;

#define CURSOR_WIDTH 14
#define CURSOR_HEIGHT 20

static void _twin_vnc_put_begin(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_coord_t bottom,
                                void *closure)
{
    (void) left;
    (void) top;
    (void) right;
    (void) bottom;
    twin_vnc_t *tx = PRIV(closure);
    pixman_region_init_rect(&tx->damage_region, 0, 0, tx->width, tx->height);
}

static void _twin_vnc_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    twin_vnc_t *tx = PRIV(closure);
    uint32_t *fb_pixels = tx->framebuffer + top * tx->width + left;
    size_t span_width = right - left;

    memcpy(fb_pixels, pixels, span_width * sizeof(*fb_pixels));
    pixman_region_union_rect(&tx->damage_region, &tx->damage_region, left, top,
                             span_width, 1);
}

static void twin_vnc_get_screen_size(twin_vnc_t *tx, int *width, int *height)
{
    *width = nvnc_fb_get_width(tx->current_fb);
    *height = nvnc_fb_get_height(tx->current_fb);
}

static bool _twin_vnc_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_vnc_t *tx = PRIV(closure);
    if (twin_screen_damaged(screen)) {
        pixman_region_clear(&tx->damage_region);
        twin_screen_update(screen);
        nvnc_display_feed_buffer(tx->display, tx->current_fb,
                                 &tx->damage_region);
    }
    return true;
}

static void _twin_vnc_client_cleanup(struct nvnc_client *client)
{
    twin_peer_t *peer = nvnc_get_userdata(client);
    free(peer);
    nvnc_set_userdata(client, NULL, NULL);
}

static void _twin_vnc_new_client(struct nvnc_client *client)
{
    twin_peer_t *peer = malloc(sizeof(twin_peer_t));
    nvnc_set_userdata(client, peer, NULL);
    nvnc_set_client_cleanup_fn(client, _twin_vnc_client_cleanup);
}

static void _twin_vnc_pointer_event(struct nvnc_client *client,
                                    uint16_t x,
                                    uint16_t y,
                                    enum nvnc_button_mask button)
{
    twin_peer_t *peer = nvnc_get_userdata(client);
    twin_event_t tev;
    if ((button & NVNC_BUTTON_LEFT) &&
        !(peer->prev_button & NVNC_BUTTON_LEFT)) {
        tev.u.pointer.screen_x = x;
        tev.u.pointer.screen_y = y;
        tev.kind = TwinEventButtonDown;
        tev.u.pointer.button = 1;
    } else if (!(button & NVNC_BUTTON_LEFT) &&
               (peer->prev_button & NVNC_BUTTON_LEFT)) {
        tev.u.pointer.screen_x = x;
        tev.u.pointer.screen_y = y;
        tev.kind = TwinEventButtonUp;
        tev.u.pointer.button = 1;
    }
    if ((peer->px != x || peer->py != y)) {
        peer->px = x;
        peer->py = y;
        tev.u.pointer.screen_x = x;
        tev.u.pointer.screen_y = y;
        tev.u.pointer.button = 0;
        tev.kind = TwinEventMotion;
    }
    peer->prev_button = button;
    struct nvnc *server = nvnc_client_get_server(client);
    twin_vnc_t *tx = nvnc_get_userdata(server);
    twin_screen_dispatch(tx->screen, &tev);
}

static struct nvnc_fb *_twin_vnc_create_cursor()
{
    struct nvnc_fb *fb = nvnc_fb_new(CURSOR_WIDTH, CURSOR_HEIGHT,
                                     DRM_FORMAT_ARGB8888, CURSOR_WIDTH);
    uint32_t *pixels = nvnc_fb_get_addr(fb);
    for (int i = 0; i < CURSOR_WIDTH * CURSOR_HEIGHT; i++) {
        uint32_t a = _twin_cursor_default[i * 4];
        uint32_t r = _twin_cursor_default[i * 4 + 1];
        uint32_t g = _twin_cursor_default[i * 4 + 2];
        uint32_t b = _twin_cursor_default[i * 4 + 3];
        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    return fb;
}

twin_context_t *twin_vnc_init(int width, int height)
{
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;
    ctx->priv = calloc(1, sizeof(twin_vnc_t));
    if (!ctx->priv) {
        free(ctx);
        return NULL;
    }

    twin_vnc_t *tx = ctx->priv;
    tx->width = width;
    tx->height = height;

    tx->aml = aml_new();
    if (!tx->aml) {
        log_error("Failed to create aml");
        goto bail_priv;
    }
    aml_set_default(tx->aml);
    char *vnc_host = getenv(MADO_VNC_HOST);
    if (!vnc_host) {
        log_info(
            "Environment variable $MADO_VNC_HOST not set, use %s by default",
            MADO_VNC_HOST_DEFAULT);
        vnc_host = MADO_VNC_HOST_DEFAULT;
    }
    char *vnc_port = getenv(MADO_VNC_PORT);
    if (!vnc_port) {
        log_info(
            "Environment variable $MADO_VNC_PORT not set, use %s by default",
            MADO_VNC_PORT_DEFAULT);
        vnc_port = MADO_VNC_PORT_DEFAULT;
    }
    log_info("NeatVNC server IP %s PORT %s", vnc_host, vnc_port);
    tx->server = nvnc_open(vnc_host, atoi(vnc_port));
    if (!tx->server) {
        log_error("Failed to open neatvnc server");
        goto bail_aml;
    }

    tx->display = nvnc_display_new(0, 0);
    if (!tx->display) {
        log_error("Failed to create neatvnc display");
        goto bail_server;
    }

    nvnc_add_display(tx->server, tx->display);
    nvnc_set_name(tx->server, "Twin VNC Backend");
    nvnc_set_pointer_fn(tx->server, _twin_vnc_pointer_event);
    nvnc_set_new_client_fn(tx->server, _twin_vnc_new_client);
    nvnc_set_userdata(tx->server, tx, NULL);
    struct nvnc_fb *cursor = _twin_vnc_create_cursor();
    nvnc_set_cursor(tx->server, cursor, CURSOR_WIDTH, CURSOR_HEIGHT, 0, 0,
                    true);
    nvnc_fb_unref(cursor);

    ctx->screen = twin_screen_create(width, height, _twin_vnc_put_begin,
                                     _twin_vnc_put_span, ctx);
    if (!ctx->screen)
        goto bail_display;

    tx->framebuffer = calloc(width * height, sizeof(uint32_t));
    if (!tx->framebuffer) {
        log_error("Failed to allocate framebuffer");
        goto bail_screen;
    }

    tx->current_fb = nvnc_fb_from_buffer(tx->framebuffer, width, height,
                                         DRM_FORMAT_ARGB8888, width);
    if (!tx->current_fb) {
        log_error("Failed to init VNC framebuffer");
        goto bail_framebuffer;
    }

    twin_set_work(_twin_vnc_work, TWIN_WORK_REDISPLAY, ctx);
    tx->screen = ctx->screen;

    return ctx;

bail_framebuffer:
    free(tx->framebuffer);
bail_screen:
    twin_screen_destroy(ctx->screen);
bail_display:
    nvnc_display_unref(tx->display);
bail_server:
    nvnc_close(tx->server);
bail_aml:
    aml_unref(tx->aml);
bail_priv:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static bool twin_vnc_poll(twin_context_t *ctx)
{
    twin_vnc_t *tx = PRIV(ctx);
    aml_poll(tx->aml, 0);
    aml_dispatch(tx->aml);
    return true;
}

static void twin_vnc_configure(twin_context_t *ctx)
{
    int width, height;
    twin_vnc_t *tx = ctx->priv;
    twin_vnc_get_screen_size(tx, &width, &height);
    twin_screen_resize(ctx->screen, width, height);
}

static void twin_vnc_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_vnc_t *tx = PRIV(ctx);
    nvnc_fb_unref(tx->current_fb);
    nvnc_display_unref(tx->display);
    nvnc_close(tx->server);
    aml_unref(tx->aml);

    free(tx->framebuffer);
    free(ctx->priv);
    free(ctx);
}

const twin_backend_t g_twin_backend = {
    .init = twin_vnc_init,
    .poll = twin_vnc_poll,
    .configure = twin_vnc_configure,
    .exit = twin_vnc_exit,
};
