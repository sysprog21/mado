/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <twin.h>
#include <unistd.h>

#include "linux_input.h"
#include "twin_backend.h"

typedef struct {
    int fd;
    int depth;
    int image_y;
    int *pixels;
    int *fbdev_raw;
    twin_screen_t *screen;
    void *input;
} twin_fbdev_t;

#define FBDEV_NAME "FRAMEBUFFER"
#define FBDEV_DEFAULT "/dev/fb0"
#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_fbdev_t *) ((twin_context_t *) x)->priv)

static void _twin_fbdev_put_span(twin_coord_t left,
                                 twin_coord_t top,
                                 twin_coord_t right,
                                 twin_argb32_t *pixels,
                                 void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_fbdev_t *tx = PRIV(closure);

    for (twin_coord_t ix = left, iy = top; ix < right; ix++) {
        twin_argb32_t pixel = *pixels++;
        tx->pixels[iy * screen->width + ix] = pixel;
    }

    /* FIXME: Performance bottleneck */
#if 0
    /* Update the pixels to the framebuffer */
    memcpy(tx->fbdev_raw, tx->pixels,
           sizeof(uint32_t) * screen->width * screen->height);
#endif
}

static void twin_fbdev_get_screen_size(twin_fbdev_t *tx,
                                       int *width,
                                       int *height)
{
    struct fb_var_screeninfo info;
    ioctl(tx->fd, FBIOGET_VSCREENINFO, &info);
    *width = info.xres;
    *height = info.yres;
}

static void twin_fbdev_damage(twin_screen_t *screen, twin_fbdev_t *tx)
{
    int width, height;
    twin_fbdev_get_screen_size(tx, &width, &height);
    twin_screen_damage(tx->screen, 0, 0, width, height);
}

static bool twin_fbdev_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);

    if (twin_screen_damaged(screen))
        twin_screen_update(screen);
    return true;
}

twin_context_t *twin_fbdev_init(int width, int height)
{
    char *fbdev_path = getenv(FBDEV_NAME);
    if (!fbdev_path) {
        printf("Environment variable $FRAMEBUFFER not set, use %s by default\n",
               FBDEV_DEFAULT);
        fbdev_path = FBDEV_DEFAULT;
    }

    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;
    ctx->priv = calloc(1, sizeof(twin_fbdev_t));
    if (!ctx->priv)
        return NULL;

    twin_fbdev_t *tx = ctx->priv;

    /* Open the framebuffer device */
    tx->fd = open(fbdev_path, O_RDWR);
    if (tx->fd == -1) {
        printf("error : failed opening %s\n", fbdev_path);
        goto bail;
    }

    /* Read framebuffer information */
    struct fb_var_screeninfo info;
    if (ioctl(tx->fd, FBIOGET_VSCREENINFO, &info) == -1) {
        printf("error : failed getting framebuffer information\n");
        goto bail_fd;
    }
    width = info.xres;
    height = info.yres;

    /* Create memory mapping for accessing the framebuffer */
    tx->fbdev_raw = mmap(NULL, width * height * info.bits_per_pixel / 8,
                         PROT_READ | PROT_WRITE, MAP_SHARED, tx->fd, 0);
    if (tx->fbdev_raw == MAP_FAILED) {
        printf("error : failed calling mmap()\n");
        goto bail_fd;
    }

    /* Create buffer space for TWIN */
    // tx->pixels = malloc(width * height * sizeof(uint32_t));
    tx->pixels = tx->fbdev_raw;
    if (!tx->pixels) {
        printf("error : failed calling malloc()\n");
        goto bail_fd;
    }
    memset(tx->pixels, 255, width * height * sizeof(uint32_t));

    /* Create TWIN screen */
    ctx->screen =
        twin_screen_create(width, height, NULL, _twin_fbdev_put_span, ctx);

    /* Create Linux input system object */
    tx->input = twin_linux_input_create(ctx->screen);
    if (!tx->input) {
        printf("error : failed creating linux input system object.\n");
        goto bail_fd;
    }

    /* Setup file handler and work functions */
    twin_set_work(twin_fbdev_work, TWIN_WORK_REDISPLAY, ctx);

    return ctx;

bail_fd:
    close(tx->fd);
bail:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static void twin_fbdev_configure(twin_context_t *ctx)
{
    int width, height;
    twin_fbdev_t *tx = ctx->priv;
    twin_fbdev_get_screen_size(tx, &width, &height);
    twin_screen_resize(ctx->screen, width, height);
}

static void twin_fbdev_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;
    twin_linux_input_destroy(PRIV(ctx)->input);
    close(PRIV(ctx)->fd);
    free(PRIV(ctx)->pixels);
    free(ctx->priv);
    free(ctx);
}

/* Register the Linux framebuffer backend */

const twin_backend_t g_twin_backend = {
    .init = twin_fbdev_init,
    .configure = twin_fbdev_configure,
    .exit = twin_fbdev_exit,
};
