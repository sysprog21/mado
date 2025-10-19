/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <SDL.h>
#include <SDL_render.h>
#include <stdio.h>
#include <twin.h>

#include "twin_private.h"

typedef struct {
    SDL_Window *win;
    int *pixels;
    SDL_Renderer *render;
    SDL_Texture *texture;
    twin_coord_t width, height;
    int image_y;
} twin_sdl_t;

#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_sdl_t *) ((twin_context_t *) x)->priv)

static void _twin_sdl_put_begin(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_coord_t bottom,
                                void *closure)
{
    twin_sdl_t *tx = PRIV(closure);
    tx->width = right - left;
    tx->height = bottom - top;
    tx->image_y = top;
}

static void _twin_sdl_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_sdl_t *tx = PRIV(closure);

    for (twin_coord_t ix = left, iy = top; ix < right; ix++) {
        twin_argb32_t pixel = *pixels++;
        tx->pixels[iy * screen->width + ix] = pixel;
    }
    if ((top + 1 - tx->image_y) == tx->height) {
        SDL_UpdateTexture(tx->texture, NULL, tx->pixels,
                          screen->width * sizeof(*pixels));
        SDL_RenderCopy(tx->render, tx->texture, NULL, NULL);
        SDL_RenderPresent(tx->render);
    }
}

static void _twin_sdl_destroy(twin_screen_t *screen maybe_unused,
                              twin_sdl_t *tx)
{
    /* Destroy SDL resources and mark as freed to prevent double-destroy */
    if (tx->texture) {
        SDL_DestroyTexture(tx->texture);
        tx->texture = NULL;
    }
    if (tx->render) {
        SDL_DestroyRenderer(tx->render);
        tx->render = NULL;
    }
    if (tx->win) {
        SDL_DestroyWindow(tx->win);
        tx->win = NULL;
    }
    SDL_Quit();
}

static void twin_sdl_damage(twin_screen_t *screen, twin_sdl_t *tx)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_damage(screen, 0, 0, width, height);
}

static bool twin_sdl_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);

    if (twin_screen_damaged(screen))
        twin_screen_update(screen);
    return true;
}

twin_context_t *twin_sdl_init(int width, int height)
{
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;
    ctx->priv = calloc(1, sizeof(twin_sdl_t));
    if (!ctx->priv) {
        free(ctx);
        return NULL;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        log_error("%s", SDL_GetError());
        goto bail;
    }

    twin_sdl_t *tx = ctx->priv;

    static char *title = "twin-sdl";
    tx->win = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, width, height,
                               SDL_WINDOW_SHOWN);
    if (!tx->win) {
        log_error("%s", SDL_GetError());
        goto bail_sdl;
    }

    tx->pixels = malloc(width * height * sizeof(*tx->pixels));
    if (!tx->pixels) {
        log_error("Failed to allocate pixel buffer");
        goto bail_window;
    }
    memset(tx->pixels, 255, width * height * sizeof(*tx->pixels));

    tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_ACCELERATED);
    if (!tx->render) {
        log_error("%s", SDL_GetError());
        goto bail_pixels;
    }
    SDL_SetRenderDrawColor(tx->render, 255, 255, 255, 255);
    SDL_RenderClear(tx->render);

    tx->texture = SDL_CreateTexture(tx->render, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!tx->texture) {
        log_error("%s", SDL_GetError());
        goto bail_renderer;
    }

    ctx->screen = twin_screen_create(width, height, _twin_sdl_put_begin,
                                     _twin_sdl_put_span, ctx);
    if (!ctx->screen) {
        log_error("Failed to create screen");
        goto bail_texture;
    }

    twin_set_work(twin_sdl_work, TWIN_WORK_REDISPLAY, ctx);

    return ctx;

bail_texture:
    SDL_DestroyTexture(tx->texture);
bail_renderer:
    SDL_DestroyRenderer(tx->render);
bail_pixels:
    free(tx->pixels);
bail_window:
    SDL_DestroyWindow(tx->win);
bail_sdl:
    SDL_Quit();
bail:
    free(ctx->priv);
    free(ctx);
    return NULL;
}

static void twin_sdl_configure(twin_context_t *ctx)
{
    int width, height;
    SDL_GetWindowSize(PRIV(ctx)->win, &width, &height);
    twin_screen_resize(ctx->screen, width, height);
}

static bool twin_sdl_poll(twin_context_t *ctx)
{
    twin_screen_t *screen = SCREEN(ctx);
    twin_sdl_t *tx = PRIV(ctx);

    SDL_Event ev;
    bool has_event = false;
    while (SDL_PollEvent(&ev)) {
        has_event = true;
        twin_event_t tev;
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
                ev.window.event == SDL_WINDOWEVENT_SHOWN) {
                twin_sdl_damage(screen, tx);
            }
            break;
        case SDL_QUIT:
            _twin_sdl_destroy(screen, tx);
            return false;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            tev.u.pointer.screen_x = ev.button.x;
            tev.u.pointer.screen_y = ev.button.y;
            tev.u.pointer.button =
                ((ev.button.state >> 8) | (1 << (ev.button.button - 1)));
            tev.kind = ((ev.type == SDL_MOUSEBUTTONDOWN) ? TwinEventButtonDown
                                                         : TwinEventButtonUp);
            twin_screen_dispatch(screen, &tev);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            tev.u.key.key = ev.key.keysym.sym;
            tev.kind = ((ev.key.type == SDL_KEYDOWN) ? TwinEventKeyDown
                                                     : TwinEventKeyUp);
            twin_screen_dispatch(screen, &tev);
            break;
        case SDL_MOUSEMOTION:
            tev.u.pointer.screen_x = ev.motion.x;
            tev.u.pointer.screen_y = ev.motion.y;
            tev.kind = TwinEventMotion;
            tev.u.pointer.button = ev.motion.state;
            twin_screen_dispatch(screen, &tev);
            break;
        }
    }

    /* Yield CPU when idle to avoid busy-waiting.
     * Skip delay if events were processed or screen needs update.
     */
    if (!has_event && !twin_screen_damaged(screen))
        SDL_Delay(1); /* 1ms sleep reduces CPU usage when idle */

    return true;
}

static void twin_sdl_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_sdl_t *tx = PRIV(ctx);

    /* Clean up SDL resources */
    if (tx->texture)
        SDL_DestroyTexture(tx->texture);
    if (tx->render)
        SDL_DestroyRenderer(tx->render);
    if (tx->win)
        SDL_DestroyWindow(tx->win);
    SDL_Quit();

    /* Free memory */
    free(tx->pixels);
    free(ctx->priv);
    free(ctx);
}

static void twin_sdl_start(twin_context_t *ctx,
                           void (*init_callback)(twin_context_t *))
{
    /* Initialize immediately and enter standard dispatch loop */
    if (init_callback)
        init_callback(ctx);

    /* Use twin_dispatch_once() to ensure work queue and timeouts run */
    while (twin_dispatch_once(ctx))
        ;
}

/* Register the SDL backend */

const twin_backend_t g_twin_backend = {
    .init = twin_sdl_init,
    .configure = twin_sdl_configure,
    .poll = twin_sdl_poll,
    .start = twin_sdl_start,
    .exit = twin_sdl_exit,
};
