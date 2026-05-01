/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
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

#include "linux_input.h"
#include "linux_vt.h"
#include "twin_private.h"

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
    int bpp, depth;
    int fix_length;
    uint32_t dumb_id;
    size_t fb_len;
    uint32_t fb_id;
    uint8_t *fb_base;

    int drm_dri_fd;

    drmModeResPtr res;
    drmModeConnectorPtr conn;
    drmModeCrtcPtr crtc;

    /* Initialization state */
    bool first_run;
} twin_drm_t;

static void twin_drm_get_screen_size(twin_drm_t *tx, int *width, int *height)
{
    *width = CONN(tx)->modes[0].hdisplay;
    *height = CONN(tx)->modes[0].vdisplay;
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
            /* it's connected*/
            if (connector_ptr->count_modes > 0) {
                /* there is at least one valid mode */
                break;
            }
        }
        drmModeFreeConnector(connector_ptr);
        connector_ptr = NULL;
    }

    return connector_ptr;
}

static bool create_fb(twin_drm_t *tx, int32_t crtc_id)
{
    /* Try to get framebuffer information */
    drmModeCrtcPtr crtc = drmModeGetCrtc(tx->drm_dri_fd, crtc_id);
    if (crtc && crtc->buffer_id) {
        drmModeFBPtr fb = drmModeGetFB(tx->drm_dri_fd, crtc->buffer_id);
        if (fb) {
            tx->bpp = fb->bpp;
            tx->depth = fb->depth;
            drmModeFreeFB(fb);
        }
        drmModeFreeCrtc(crtc);
    } else
        return false;

    /* Validate bits per pixel */
    if (tx->bpp != 16 && tx->bpp != 24 && tx->bpp != 32) {
        log_error("Unsupported bits per pixel: %d", tx->bpp);
        return false;
    }

    /* Create dumb buffer */
    struct drm_mode_create_dumb create_req = {
        .width = tx->width,
        .height = tx->height,
        .bpp = tx->bpp,
    };
    if (drmIoctl(tx->drm_dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        log_error("Cannot create dumb buffer");
        return false;
    }
    tx->dumb_id = create_req.handle;
    tx->fb_len = create_req.size;
    tx->fix_length = create_req.pitch;

    /* Create framebuffer object for the dumb-buffer */
    if (drmModeAddFB(tx->drm_dri_fd, tx->width, tx->height, tx->depth, tx->bpp,
                     create_req.pitch, create_req.handle, &tx->fb_id) != 0) {
        log_error("Cannot create framebuffer");
        goto bail_dumb;
    }

    /* Prepare buffer for memory mapping */
    struct drm_mode_map_dumb map_req = {
        .handle = create_req.handle,
    };
    if (drmIoctl(tx->drm_dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
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
bail_dumb:
    struct drm_mode_destroy_dumb destroy = {.handle = tx->dumb_id};
    drmIoctl(tx->drm_dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    return false;
}

static void destroy_fb(twin_drm_t *tx)
{
    /* Unmap framebuffer */
    munmap(tx->fb_base, tx->fb_len);

    /* Delete framebuffer */
    drmModeRmFB(tx->drm_dri_fd, tx->fb_id);

    /* Delete dumb buffer */
    struct drm_mode_destroy_dumb destroy = {.handle = tx->dumb_id};
    drmIoctl(tx->drm_dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static int32_t find_drm_crtc(twin_drm_t *tx)
{
    drmModeEncoderPtr enc = NULL;
    int32_t crtc_id = -1;

    /* First try the currently conected encoder+crtc */
    enc = drmModeGetEncoder(tx->drm_dri_fd, CONN(tx)->encoder_id);
    if (enc) {
        drmModeCrtcPtr crtc = drmModeGetCrtc(tx->drm_dri_fd, enc->crtc_id);
        if (crtc) {
            crtc_id = enc->crtc_id;
            drmModeFreeCrtc(crtc);
            drmModeFreeEncoder(enc);
            return crtc_id;
        }

        drmModeFreeEncoder(enc);
    }

    /* Iterate all encoders，find supported CRTC */
    for (int i = 0; i < CONN(tx)->count_encoders; ++i) {
        enc = drmModeGetEncoder(tx->drm_dri_fd, CONN(tx)->encoders[i]);
        if (!enc)
            continue;
        /* Iterate all CRTCs */
        for (int j = 0; j < RES(tx)->count_crtcs; ++j) {
            /* Check whether this CRTC works with the encoder */
            if (enc->possible_crtcs & (1 << j)) {
                crtc_id = RES(tx)->crtcs[j];
                drmModeFreeEncoder(enc);
                return crtc_id;
            }
        }
    }

    log_error("No valid CRTC found");
    return crtc_id;
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

    int32_t crtc_id = find_drm_crtc(tx);
    if (crtc_id == -1)
        goto bail_conn;

    /* Get the mode information */
    drmModeModeInfo mode = CONN(tx)->modes[0];
    tx->width = mode.hdisplay;
    tx->height = mode.vdisplay;

    /* Create the framebuffer */
    if (!create_fb(tx, crtc_id))
        goto bail_conn;

    /* Set the mode on a CRTC */
    CRTC(tx) = drmModeGetCrtc(tx->drm_dri_fd, crtc_id);
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
    destroy_fb(tx);
bail_conn:
    drmModeFreeConnector(CONN(tx));
bail_res:
    drmModeFreeResources(RES(tx));
    return false;
}

static bool twin_drm_open(twin_drm_t *tx, const char *path)
{
    int fd;
    uint64_t has_dumb;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        log_error("Failed to open %s", path);
        return false;
    }

    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
        log_error("Drm device '%s' does not support dumb buffers", path);
        close(fd);
        return false;
    }

    tx->drm_dri_fd = fd;
    return true;
}

static void twin_drm_damage(twin_drm_t *tx)
{
    int width, height;
    twin_drm_get_screen_size(tx, &width, &height);
    twin_screen_damage(tx->screen, 0, 0, width, height);
}

static void twin_drm_flush(twin_drm_t *tx)
{
    drmModeDirtyFB(tx->drm_dri_fd, tx->fb_id, NULL, 0);
}

static bool twin_drm_update_damage(void *closure)
{
    twin_drm_t *tx = PRIV(closure);
    twin_screen_t *screen = SCREEN(closure);

    if (!tx->vt_active && twin_screen_damaged(screen)) {
        twin_screen_update(screen);
        twin_drm_flush(tx);
    }

    return true;
}

static bool twin_drm_work(void *closure)
{
    twin_drm_t *tx = PRIV(closure);
    twin_screen_t *screen = SCREEN(closure);

    /* Mark entire screen as damaged on first run to ensure initial rendering.
     * This is necessary because the screen content is not automatically
     * rendered when the application starts. Using per-context state allows
     * proper reinitialization if the backend is torn down and recreated.
     */
    if (tx->first_run) {
        tx->first_run = false;
        twin_screen_damage(screen, 0, 0, screen->width, screen->height);
    }

    if (tx->vt_active && (tx->fb_base != MAP_FAILED)) {
        /* Unmap the drm device */
        munmap(tx->fb_base, tx->fb_len);
        tx->fb_base = MAP_FAILED;
    }

    if (!tx->vt_active && (tx->fb_base == MAP_FAILED)) {
        /* Restore the drm device settings */
        if (!twin_drm_apply_config(tx))
            log_error("Failed to apply configurations to the drm device");

        /* Mark entire screen for refresh */
        twin_screen_damage(screen, 0, 0, screen->width, screen->height);
    }

    if (!tx->vt_active && twin_screen_damaged(screen)) {
        twin_screen_update(screen);
        twin_drm_flush(tx);
    }

    return true;
}

#define ARGB32_TO_RGB565_PERLINE(dest, pixels, width)    \
    do {                                                 \
        uint16_t *_dest = (uint16_t *) dest;             \
        for (int i = 0; i < width; i++)                  \
            _dest[i] = ((pixels[i] & 0x00f80000) >> 8) | \
                       ((pixels[i] & 0x0000fc00) >> 5) | \
                       ((pixels[i] & 0x000000f8) >> 3);  \
    } while (0)

/* Requires validation in 24-bit per pixel environments */
#define ARGB32_TO_RGB888_PERLINE(dest, pixels, width)            \
    do {                                                         \
        uint8_t *_dest = (uint8_t *) dest;                       \
        for (int i = 0; i < width; i++) {                        \
            _dest[i * 3] = pixels[i] & 0xFF;                     \
            _dest[i * 3 + 1] = ((pixels[i] & 0x0000FF00) >> 8);  \
            _dest[i * 3 + 2] = ((pixels[i] & 0x00FF0000) >> 16); \
        }                                                        \
    } while (0)

#define ARGB32_TO_ARGB32_PERLINE(dest, pixels, width) \
    memcpy((uint32_t *) dest, pixels, width * 4)

#define DRM_PUT_SPAN_IMPL(bpp, op)                                            \
    static void _twin_drm_put_span##bpp(twin_coord_t left, twin_coord_t top,  \
                                        twin_coord_t right,                   \
                                        twin_argb32_t *pixels, void *closure) \
    {                                                                         \
        twin_drm_t *tx = PRIV(closure);                                       \
        off_t off = (bpp / 8) * left + top * tx->fix_length;                  \
        uintptr_t dest = (uintptr_t) tx->fb_base + off;                       \
        twin_coord_t width = right - left;                                    \
        op(dest, pixels, width);                                              \
    }

DRM_PUT_SPAN_IMPL(16, ARGB32_TO_RGB565_PERLINE)
DRM_PUT_SPAN_IMPL(24, ARGB32_TO_RGB888_PERLINE)
DRM_PUT_SPAN_IMPL(32, ARGB32_TO_ARGB32_PERLINE)
twin_context_t *twin_drm_init(int width maybe_unused, int height maybe_unused)
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
    if (!ctx->priv) {
        free(ctx);
        return NULL;
    }

    twin_drm_t *tx = ctx->priv;

    /* Initialize first_run flag for initial screen damage */
    tx->first_run = true;

    /* Open the DRM driver */
    if (!twin_drm_open(tx, drm_dri_path)) {
        goto bail;
    }

    /* Set up virtual terminal environment */
    if (!twin_vt_setup(&tx->vt_fd, &tx->old_vtm, &tx->vt_active)) {
        goto bail_dri_dev_fd;
    }

    /* Set up signal handlers for switching TTYs */
    twin_vt_setup_signal_handler();

    /* Apply configurations to the DRM driver*/
    if (!twin_drm_apply_config(tx)) {
        log_error("Failed to apply configurations to the DRM driver");
        goto bail_vt_fd;
    }

    const twin_put_span_t drm_put_spans[] = {
        _twin_drm_put_span16,
        _twin_drm_put_span24,
        _twin_drm_put_span32,
    };

    /* Create TWIN screen */
    ctx->screen = twin_screen_create(tx->width, tx->height, NULL,
                                     drm_put_spans[tx->bpp / 8 - 2], ctx);
    if (!ctx->screen) {
        log_error("Failed to create screen");
        goto bail_vt_fd;
    }

    /* Create Linux input system object */
    tx->input = twin_linux_input_create(ctx->screen);
    if (!tx->input) {
        log_error("Failed to create Linux input system object");
        goto bail_screen;
    }

    /* Setup file handler and work functions */
    twin_set_work(twin_drm_work, TWIN_WORK_REDISPLAY, ctx);

    /* Register a callback function to handle damaged rendering */
    twin_screen_register_damaged(ctx->screen, twin_drm_update_damage, ctx);

    return ctx;

bail_screen:
    twin_screen_destroy(ctx->screen);
bail_vt_fd:
    /* Restore VT mode before closing */
    ioctl(tx->vt_fd, VT_SETMODE, &tx->old_vtm);
    twin_vt_mode(tx->vt_fd, KD_TEXT);
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

    /* Restore VT mode before cleanup */
    if (tx->vt_fd >= 0) {
        ioctl(tx->vt_fd, VT_SETMODE, &tx->old_vtm);
        twin_vt_mode(tx->vt_fd, KD_TEXT);
    }

    drmModeFreeCrtc(CRTC(tx));
    destroy_fb(tx);
    drmModeFreeConnector(CONN(tx));
    drmModeFreeResources(RES(tx));
    twin_linux_input_destroy(tx->input);
    close(tx->vt_fd);
    close(tx->drm_dri_fd);
    free(ctx->priv);
    free(ctx);
}

/* Poll function for DRM backend
 * The DRM backend uses linux_input background thread for event handling,
 * so this poll function just returns true to continue the main loop.
 */
static bool twin_drm_poll(twin_context_t *ctx maybe_unused)
{
    return true;
}

static void twin_drm_start(twin_context_t *ctx,
                           void (*init_callback)(twin_context_t *))
{
    if (init_callback)
        init_callback(ctx);

    while (twin_dispatch_once(ctx))
        ;
}

/* Register the Linux DRM backend */

const twin_backend_t g_twin_backend = {
    .init = twin_drm_init,
    .configure = twin_drm_configure,
    .poll = twin_drm_poll,
    .start = twin_drm_start,
    .exit = twin_drm_exit,
};
