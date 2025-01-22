/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <twin.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "linux_input.h"
#include "linux_vt.h"
#include "twin_backend.h"

#define DRM_DRI_NAME "DRMDEVICE"
#define DRM_DRI_DEFAULT "/dev/dri/card0"
#define SCREEN(x) ((twin_context_t *) x)->screen
#define RES(x) ((twin_drm_t *) x)->res
#define CONN(x) ((twin_drm_t *) x)->conn
#define CRTC(x) ((twin_drm_t *) x)->crtc
#define PRIV(x) ((twin_drm_t *) ((twin_context_t *) x)->priv)

typedef struct {
    twin_screen_t *screen;

    /* Linux input system */
    void *input;

    /* Linux virtual terminal (VT) */
    int vt_fd;
    bool vt_active;
    struct vt_mode old_vtm;

    /* DRM driver */
    int width, height;
    int drm_dri_fd;
    drmModeResPtr res;
    drmModeConnectorPtr conn;
    drmModeCrtcPtr crtc;
    uint32_t fb_id;
    uint8_t *fb_base;
    size_t fb_len;
} twin_drm_t;

static void _twin_drm_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    twin_drm_t *tx = PRIV(closure);

    if (tx->fb_base == MAP_FAILED)
        return;

    twin_coord_t width = right - left;
    off_t off = tx->width * top + left;
    uint32_t *dest =
        (uint32_t *) ((uintptr_t) tx->fb_base + (off * sizeof(*dest)));
    memcpy(dest, pixels, width * sizeof(*dest));
}

static void twin_drm_get_screen_size(twin_drm_t *tx, int *width, int *height)
{
    *width = CONN(tx)->modes[0].hdisplay;
    *height = CONN(tx)->modes[0].vdisplay;
}

static bool twin_drm_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);

    if (twin_screen_damaged(screen))
        twin_screen_update(screen);

    return true;
}

static bool get_resources(int fd, drmModeResPtr *resources)
{
    *resources = drmModeGetResources(fd);
    if (*resources == NULL) {
        log_error("Failed to get resources");
        return false;
    }
    return true;
}

static drmModeConnectorPtr find_drm_connector(int fd, drmModeResPtr resources)
{
    drmModeConnectorPtr connector_ptr = NULL;

    for (int i = 0; i < resources->count_connectors; i++) {
        connector_ptr = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector_ptr && connector_ptr->connection == DRM_MODE_CONNECTED) {
            /* it's connected, let's use this! */
            break;
        }
        drmModeFreeConnector(connector_ptr);
        connector_ptr = NULL;
    }

    return connector_ptr;
}

static bool create_fb(twin_drm_t *tx)
{
    /* Create dumb buffer */
    struct drm_mode_create_dumb create_req = {
        .width = tx->width,
        .height = tx->height,
        .bpp = 32,
    };
    if (ioctl(tx->drm_dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        log_error("Cannot create dumb buffer");
        return false;
    }
    tx->fb_len = create_req.size;

    /* Create framebuffer object for the dumb-buffer */
    if (drmModeAddFB(tx->drm_dri_fd, tx->width, tx->height, 24, 32,
                     create_req.pitch, create_req.handle, &tx->fb_id) != 0) {
        log_error("Cannot create framebubber");
        return false;
    }

    /* Prepare buffer for memory mapping */
    struct drm_mode_map_dumb map_req = {
        .handle = create_req.handle,
    };
    if (ioctl(tx->drm_dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        log_error("Cannot map dumb buffer");
        goto bail_fb;
    }

    /* Perform actual memory mapping */
    tx->fb_base = mmap(NULL, tx->fb_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                       tx->drm_dri_fd, map_req.offset);
    if (tx->fb_base == MAP_FAILED) {
        log_error("Failed to mmap framebuffer");
        goto bail_fb;
    }

    return true;

bail_fb:
    drmModeRmFB(tx->drm_dri_fd, tx->fb_id);
    return false;
}

static bool twin_drm_apply_config(twin_drm_t *tx)
{
    /* Retrieve resources */
    if (!get_resources(tx->drm_dri_fd, &RES(tx)))
        return false;

    /* Find a connected connector */
    CONN(tx) = find_drm_connector(tx->drm_dri_fd, RES(tx));
    if (!CONN(tx))
        goto bail_res;

    /* Get the mode information */
    drmModeModeInfo mode = CONN(tx)->modes[0];
    tx->width = mode.hdisplay;
    tx->height = mode.vdisplay;

    /* Create the framebuffer */
    if (!create_fb(tx))
        goto bail_conn;

    /* Set the mode on a CRTC */
    CRTC(tx) = drmModeGetCrtc(tx->drm_dri_fd, RES(tx)->crtcs[0]);
    if (!CRTC(tx))
        goto bail_mmap;

    if (drmModeSetCrtc(tx->drm_dri_fd, CRTC(tx)->crtc_id, tx->fb_id, 0, 0,
                       &CONN(tx)->connector_id, 1, &mode) != 0) {
        log_error("Failed to set CRTC");
        goto bail_crtc;
    }

    return true;

bail_crtc:
    drmModeFreeCrtc(CRTC(tx));
bail_mmap:
    munmap(tx->fb_base, tx->fb_len);
    drmModeRmFB(tx->drm_dri_fd, tx->fb_id);
bail_conn:
    drmModeFreeConnector(CONN(tx));
bail_res:
    drmModeFreeResources(RES(tx));
    return false;
}

twin_context_t *twin_drm_init(int width, int height)
{
    /* Get environment variable to execute */
    char *drm_dri_path = getenv(DRM_DRI_NAME);
    if (!drm_dri_path) {
        log_info("Environment variable $DRMDEVICE not set, use %s by default",
                 DRM_DRI_DEFAULT);
        drm_dri_path = DRM_DRI_DEFAULT;
    }

    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;

    ctx->priv = calloc(1, sizeof(twin_drm_t));
    if (!ctx->priv)
        return NULL;

    twin_drm_t *tx = ctx->priv;

    /* Open the DRM driver */
    tx->drm_dri_fd = open(drm_dri_path, O_RDWR);
    if (tx->drm_dri_fd < 0) {
        log_error("Failed to open %s", drm_dri_path);
        goto bail;
    }

    /* Set up virtual terminal environment */
    if (!twin_vt_setup(&tx->vt_fd, &tx->old_vtm, &tx->vt_active)) {
        goto bail_dri_dev_fd;
    }

    /* Apply configurations to the DRM driver*/
    if (!twin_drm_apply_config(tx)) {
        log_error("Failed to apply configurations to the DRM driver");
        goto bail_vt_fd;
    }

    /* Create TWIN screen */
    ctx->screen =
        twin_screen_create(width, height, NULL, _twin_drm_put_span, ctx);

    /* Create Linux input system object */
    tx->input = twin_linux_input_create(ctx->screen);
    if (!tx->input) {
        log_error("Failed to create Linux input system object");
        goto bail_screen;
    }

    /* Setup file handler and work functions */
    twin_set_work(twin_drm_work, TWIN_WORK_REDISPLAY, ctx);

    return ctx;

bail_screen:
    twin_screen_destroy(ctx->screen);
bail_vt_fd:
    close(tx->vt_fd);
bail_dri_dev_fd:
    close(tx->drm_dri_fd);
bail:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static void twin_drm_configure(twin_context_t *ctx)
{
    int width, height;
    twin_drm_t *tx = PRIV(ctx);
    twin_drm_get_screen_size(tx, &width, &height);
    twin_screen_resize(SCREEN(ctx), width, height);
}

static void twin_drm_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_drm_t *tx = PRIV(ctx);
    twin_vt_mode(tx->vt_fd, KD_TEXT);
    drmModeFreeCrtc(CRTC(tx));
    munmap(tx->fb_base, tx->fb_len);
    drmModeRmFB(tx->drm_dri_fd, tx->fb_id);
    drmModeFreeConnector(CONN(tx));
    drmModeFreeResources(RES(tx));
    twin_linux_input_destroy(tx->input);
    close(tx->vt_fd);
    close(tx->drm_dri_fd);
    free(ctx->priv);
    free(ctx);
}

/* Register the Linux DRM backend */

const twin_backend_t g_twin_backend = {
    .init = twin_drm_init,
    .configure = twin_drm_configure,
    .exit = twin_drm_exit,
};
