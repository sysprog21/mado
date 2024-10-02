/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <SDL.h>
#include <SDL_render.h>
#include <stdio.h>
#include <twin.h>

#include "twin_backend.h"

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
                          screen->width * sizeof(uint32_t));
        SDL_RenderCopy(tx->render, tx->texture, NULL, NULL);
        SDL_RenderPresent(tx->render);
    }
}

static void _twin_sdl_destroy(twin_screen_t *screen maybe_unused,
                              twin_sdl_t *tx)
{
    SDL_DestroyTexture(tx->texture);
    SDL_DestroyRenderer(tx->render);
    SDL_DestroyWindow(tx->win);
    SDL_Quit();
}

static void twin_sdl_damage(twin_screen_t *screen, twin_sdl_t *tx)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_damage(screen, 0, 0, width, height);
}

static bool twin_sdl_read_events(int file maybe_unused,
                                 twin_file_op_t ops maybe_unused,
                                 void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_sdl_t *tx = PRIV(closure);

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
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
    return true;
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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
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
        goto bail;
    }

    tx->pixels = malloc(width * height * sizeof(uint32_t));
    memset(tx->pixels, 255, width * height * sizeof(uint32_t));

    tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_ACCELERATED);
    if (!tx->render) {
        log_error("%s", SDL_GetError());
        goto bail_pixels;
    }
    SDL_SetRenderDrawColor(tx->render, 255, 255, 255, 255);
    SDL_RenderClear(tx->render);

    tx->texture = SDL_CreateTexture(tx->render, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);

    ctx->screen = twin_screen_create(width, height, _twin_sdl_put_begin,
                                     _twin_sdl_put_span, ctx);

    twin_set_file(twin_sdl_read_events, 0, TWIN_READ, ctx);

    twin_set_work(twin_sdl_work, TWIN_WORK_REDISPLAY, ctx);

    return ctx;

bail_pixels:
    free(tx->pixels);
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

static void twin_sdl_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;
    free(PRIV(ctx)->pixels);
    free(ctx->priv);
    free(ctx);
}

/* Register the SDL backend */

const twin_backend_t g_twin_backend = {
    .init = twin_sdl_init,
    .configure = twin_sdl_configure,
    .exit = twin_sdl_exit,
};
