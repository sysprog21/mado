/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <twin.h>
#include <unistd.h>

#include "linux_fb.h"

#define FBDEV_NAME "FRAMEBUFFER"
#define FBDEV_DEFAULT "/dev/fb0"

typedef struct {
    twin_linux_fb_t fb; /* first: the shared core reads it from priv */
    int fb_fd;
    struct fb_var_screeninfo fb_var;
} twin_fbdev_t;

static bool _channels(const struct fb_var_screeninfo *v,
                      int r,
                      int rl,
                      int g,
                      int gl,
                      int b,
                      int bl)
{
    return v->red.offset == (unsigned) r && v->red.length == (unsigned) rl &&
           v->green.offset == (unsigned) g &&
           v->green.length == (unsigned) gl && v->blue.offset == (unsigned) b &&
           v->blue.length == (unsigned) bl;
}

/* Set the mode, check the pixel layout and map the framebuffer */
static bool twin_fbdev_map(twin_linux_fb_t *fb)
{
    twin_fbdev_t *tx = (twin_fbdev_t *) fb;

    if (ioctl(tx->fb_fd, FBIOGET_VSCREENINFO, &tx->fb_var) < 0) {
        log_error("Failed to get framebuffer information");
        return false;
    }
    /* The virtual screen is the physical one: no panning */
    tx->fb_var.xres_virtual = tx->fb_var.xres;
    tx->fb_var.yres_virtual = tx->fb_var.yres;
    if (ioctl(tx->fb_fd, FBIOPUT_VSCREENINFO, &tx->fb_var) < 0 ||
        ioctl(tx->fb_fd, FBIOGET_VSCREENINFO, &tx->fb_var) < 0) {
        log_error("Failed to set framebuffer mode");
        return false;
    }

    bool ok;
    switch (tx->fb_var.bits_per_pixel) {
    case 16:
        ok = _channels(&tx->fb_var, 11, 5, 5, 6, 0, 5);
        break;
    case 24:
    case 32:
        ok = _channels(&tx->fb_var, 16, 8, 8, 8, 0, 8);
        break;
    default:
        ok = false;
        break;
    }
    if (!ok) {
        log_error("Unsupported framebuffer format: %u bpp",
                  tx->fb_var.bits_per_pixel);
        return false;
    }

    struct fb_fix_screeninfo fix;
    if (ioctl(tx->fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        log_error("Failed to get framebuffer layout");
        return false;
    }
    /* The mapping starts on a page boundary and covers whole pages */
    size_t pgsize = (size_t) getpagesize();
    size_t start = (size_t) fix.smem_start & (pgsize - 1);
    fb->fb_len = (start + fix.smem_len + pgsize - 1) & ~(pgsize - 1);
    fb->stride = fix.line_length;
    fb->fb_base = mmap(NULL, fb->fb_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                       tx->fb_fd, 0);
    if (fb->fb_base == MAP_FAILED) {
        log_error("Failed to mmap framebuffer");
        return false;
    }
    return true;
}

twin_context_t *twin_fbdev_init(int width maybe_unused, int height maybe_unused)
{
    char *fbdev_path = getenv(FBDEV_NAME);
    if (!fbdev_path) {
        log_info("Environment variable $FRAMEBUFFER not set, use %s by default",
                 FBDEV_DEFAULT);
        fbdev_path = FBDEV_DEFAULT;
    }

    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;
    twin_fbdev_t *tx = ctx->priv = calloc(1, sizeof(twin_fbdev_t));
    if (!tx)
        goto bail;
    tx->fb.fb_base = MAP_FAILED;
    tx->fb.remap = twin_fbdev_map;

    tx->fb_fd = open(fbdev_path, O_RDWR);
    if (tx->fb_fd < 0) {
        log_error("Failed to open %s", fbdev_path);
        goto bail;
    }
    if (!twin_linux_fb_take_vt(&tx->fb))
        goto bail_fb_fd;
    if (!twin_fbdev_map(&tx->fb))
        goto bail_vt;

    /* The display's own resolution wins over the caller's hint, which
     * only resizable backends such as SDL honor */
    if (!twin_linux_fb_start(ctx, tx->fb_var.xres, tx->fb_var.yres,
                             tx->fb_var.bits_per_pixel))
        goto bail_unmap;
    return ctx;

bail_unmap:
    twin_linux_fb_unmap(&tx->fb);
bail_vt:
    twin_linux_fb_release_vt(&tx->fb);
bail_fb_fd:
    close(tx->fb_fd);
bail:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static void twin_fbdev_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;
    twin_fbdev_t *tx = ctx->priv;
    twin_linux_fb_stop(&tx->fb);
    twin_linux_fb_unmap(&tx->fb);
    close(tx->fb_fd);
    free(ctx->priv);
    free(ctx);
}

const twin_backend_t g_twin_backend = {
    .init = twin_fbdev_init,
    .poll = twin_linux_fb_poll,
    .exit = twin_fbdev_exit,
};
