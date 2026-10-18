/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <twin.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "linux_fb.h"

#define DRM_DRI_NAME "DRMDEVICE"
#define DRM_DRI_DEFAULT "/dev/dri/card0"

typedef struct {
    twin_linux_fb_t fb; /* first: the shared core reads it from priv */
    int fd;
    drmModeConnectorPtr conn;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    drmModeCrtcPtr saved_crtc; /* the console's setup, restored on exit */
    int bpp, depth;
    uint32_t dumb, fb_id;
    uint64_t map_offset;
} twin_drm_t;

static drmModeConnectorPtr find_connector(int fd, drmModeResPtr res)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr c = drmModeGetConnector(fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0)
            return c;
        drmModeFreeConnector(c);
    }
    log_error("No connected display");
    return NULL;
}

/* The CRTC driving the connector now, else any its encoders can use */
static int64_t find_crtc(int fd, drmModeResPtr res, drmModeConnectorPtr conn)
{
    drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (enc) {
        uint32_t id = enc->crtc_id;
        drmModeFreeEncoder(enc);
        if (id)
            return id;
    }
    for (int i = 0; i < conn->count_encoders; i++) {
        enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc)
            continue;
        uint32_t possible = enc->possible_crtcs;
        drmModeFreeEncoder(enc);
        for (int j = 0; j < res->count_crtcs; j++)
            if (possible & (1u << j))
                return res->crtcs[j];
    }
    log_error("No usable CRTC");
    return -1;
}

/* Pixel depth of what the CRTC shows now; XRGB8888 when it shows nothing */
static void probe_depth(twin_drm_t *tx)
{
    tx->bpp = 32;
    tx->depth = 24;
    drmModeCrtcPtr crtc = tx->saved_crtc;
    if (!crtc || !crtc->buffer_id)
        return;
    drmModeFBPtr fb = drmModeGetFB(tx->fd, crtc->buffer_id);
    if (!fb)
        return;
    if (fb->bpp == 16 || fb->bpp == 24 || fb->bpp == 32) {
        tx->bpp = fb->bpp;
        tx->depth = fb->depth;
    }
    drmModeFreeFB(fb);
}

/* Map the dumb buffer and show it: at start and on every return to the
 * VT, whose owner may have changed the mode meanwhile */
static bool twin_drm_map(twin_linux_fb_t *fb)
{
    twin_drm_t *tx = (twin_drm_t *) fb;
    fb->fb_base = mmap(NULL, fb->fb_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                       tx->fd, tx->map_offset);
    if (fb->fb_base == MAP_FAILED) {
        log_error("Failed to mmap framebuffer");
        return false;
    }
    if (drmModeSetCrtc(tx->fd, tx->crtc_id, tx->fb_id, 0, 0,
                       &tx->conn->connector_id, 1, &tx->mode)) {
        log_error("Failed to set CRTC");
        twin_linux_fb_unmap(fb);
        return false;
    }
    return true;
}

static void twin_drm_flush(twin_linux_fb_t *fb)
{
    twin_drm_t *tx = (twin_drm_t *) fb;
    drmModeDirtyFB(tx->fd, tx->fb_id, NULL, 0);
}

/* Pick the display and create the buffer the screen draws into */
static bool twin_drm_setup(twin_drm_t *tx)
{
    drmModeResPtr res = drmModeGetResources(tx->fd);
    if (!res) {
        log_error("Failed to get DRM resources");
        return false;
    }
    tx->conn = find_connector(tx->fd, res);
    int64_t crtc = tx->conn ? find_crtc(tx->fd, res, tx->conn) : -1;
    drmModeFreeResources(res);
    if (crtc < 0)
        return false;
    tx->crtc_id = (uint32_t) crtc;
    tx->mode = tx->conn->modes[0];
    tx->saved_crtc = drmModeGetCrtc(tx->fd, tx->crtc_id);
    probe_depth(tx);

    struct drm_mode_create_dumb create = {
        .width = tx->mode.hdisplay,
        .height = tx->mode.vdisplay,
        .bpp = (uint32_t) tx->bpp,
    };
    if (drmIoctl(tx->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        log_error("Cannot create dumb buffer");
        return false;
    }
    tx->dumb = create.handle;
    tx->fb.fb_len = create.size;
    tx->fb.stride = create.pitch;
    if (drmModeAddFB(tx->fd, create.width, create.height, (uint8_t) tx->depth,
                     (uint8_t) tx->bpp, create.pitch, create.handle,
                     &tx->fb_id)) {
        log_error("Cannot create framebuffer");
        return false;
    }
    struct drm_mode_map_dumb map = {.handle = create.handle};
    if (drmIoctl(tx->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        log_error("Cannot map dumb buffer");
        return false;
    }
    tx->map_offset = map.offset;
    return twin_drm_map(&tx->fb);
}

/* Undo twin_drm_setup(), whatever part of it ran, and close the device */
static void twin_drm_teardown(twin_drm_t *tx)
{
    twin_linux_fb_unmap(&tx->fb);
    drmModeCrtcPtr c = tx->saved_crtc;
    if (c && c->mode_valid)
        drmModeSetCrtc(tx->fd, c->crtc_id, c->buffer_id, c->x, c->y,
                       &tx->conn->connector_id, 1, &c->mode);
    if (tx->fb_id)
        drmModeRmFB(tx->fd, tx->fb_id);
    if (tx->dumb) {
        struct drm_mode_destroy_dumb destroy = {.handle = tx->dumb};
        drmIoctl(tx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    drmModeFreeCrtc(tx->saved_crtc);
    drmModeFreeConnector(tx->conn);
    close(tx->fd);
}

twin_context_t *twin_drm_init(int width maybe_unused, int height maybe_unused)
{
    char *path = getenv(DRM_DRI_NAME);
    if (!path) {
        log_info("Environment variable $DRMDEVICE not set, use %s by default",
                 DRM_DRI_DEFAULT);
        path = DRM_DRI_DEFAULT;
    }

    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;
    twin_drm_t *tx = ctx->priv = calloc(1, sizeof(twin_drm_t));
    if (!tx)
        goto bail;
    tx->fb.fb_base = MAP_FAILED;
    tx->fb.remap = twin_drm_map;
    tx->fb.flush = twin_drm_flush;

    tx->fd = open(path, O_RDWR);
    if (tx->fd < 0) {
        log_error("Failed to open %s", path);
        goto bail;
    }
    uint64_t has_dumb;
    if (drmGetCap(tx->fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
        log_error("DRM device '%s' does not support dumb buffers", path);
        goto bail_fd;
    }
    if (!twin_linux_fb_take_vt(&tx->fb))
        goto bail_fd;
    if (!twin_drm_setup(tx) || !twin_linux_fb_start(ctx, tx->mode.hdisplay,
                                                    tx->mode.vdisplay, tx->bpp))
        goto bail_drm;
    return ctx;

bail_drm:
    twin_drm_teardown(tx); /* closes the device */
    twin_linux_fb_release_vt(&tx->fb);
    goto bail;
bail_fd:
    close(tx->fd);
bail:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static void twin_drm_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;
    twin_drm_t *tx = ctx->priv;
    twin_drm_teardown(tx);
    twin_linux_fb_stop(&tx->fb);
    free(ctx->priv);
    free(ctx);
}

const twin_backend_t g_twin_backend = {
    .init = twin_drm_init,
    .poll = twin_linux_fb_poll,
    .exit = twin_drm_exit,
};
