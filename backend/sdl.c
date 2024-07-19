/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <stdio.h>

#include "twin_private.h"
#include "twin_sdl.h"

static void _twin_sdl_put_begin(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_coord_t bottom,
                                void *closure)
{
    twin_sdl_t *tx = closure;
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
    twin_sdl_t *tx = closure;
    twin_coord_t ix;
    twin_coord_t iy = top;

    for (ix = left; ix < right; ix++) {
        twin_argb32_t pixel = *pixels++;

        if (tx->depth == 16)
            pixel = twin_argb32_to_rgb16(pixel);

        tx->pixels[iy * tx->screen->width + ix] = pixel;
    }
    if ((top + 1 - tx->image_y) == tx->height) {
        SDL_UpdateTexture(tx->texture, NULL, tx->pixels,
                          tx->screen->width * sizeof(uint32_t));
        SDL_RenderCopy(tx->render, tx->texture, NULL, NULL);
        SDL_RenderPresent(tx->render);
    }
}

static bool twin_sdl_read_events(int file maybe_unused,
                                 twin_file_op_t ops maybe_unused,
                                 void *closure)
{
    twin_sdl_t *tx = closure;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        twin_event_t tev;
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
                ev.window.event == SDL_WINDOWEVENT_SHOWN) {
                twin_sdl_damage(tx, &ev);
            }
            break;
        case SDL_QUIT:
            twin_sdl_destroy(tx);
            return false;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            tev.u.pointer.screen_x = ev.button.x;
            tev.u.pointer.screen_y = ev.button.y;
            tev.u.pointer.button =
                ((ev.button.state >> 8) | (1 << (ev.button.button - 1)));
            tev.kind = ((ev.type == SDL_MOUSEBUTTONDOWN) ? TwinEventButtonDown
                                                         : TwinEventButtonUp);
            twin_screen_dispatch(tx->screen, &tev);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            tev.u.key.key = ev.key.keysym.sym;
            tev.kind = ((ev.key.type == SDL_KEYDOWN) ? TwinEventKeyDown
                                                     : TwinEventKeyUp);
            twin_screen_dispatch(tx->screen, &tev);
            break;
        case SDL_MOUSEMOTION:
            tev.u.pointer.screen_x = ev.motion.x;
            tev.u.pointer.screen_y = ev.motion.y;
            tev.kind = TwinEventMotion;
            tev.u.pointer.button = ev.motion.state;
            twin_screen_dispatch(tx->screen, &tev);
            break;
        }
    }
    return true;
}

static bool twin_sdl_work(void *closure)
{
    twin_sdl_t *tx = closure;

    if (twin_screen_damaged(tx->screen))
        twin_sdl_update(tx);
    return true;
}

twin_sdl_t *twin_sdl_create(int width, int height)
{
    static char *argv[] = {"twin-sdl", 0};

    twin_sdl_t *tx = malloc(sizeof(twin_sdl_t));
    if (!tx)
        return NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        printf("error : %s\n", SDL_GetError());
    tx->win = SDL_CreateWindow(argv[0], SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, width, height,
                               SDL_WINDOW_SHOWN);
    if (!tx->win)
        printf("error : %s\n", SDL_GetError());
    tx->pixels = malloc(width * height * sizeof(uint32_t));
    memset(tx->pixels, 255, width * height * sizeof(uint32_t));

    tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_ACCELERATED);
    if (!tx->render)
        printf("error : %s\n", SDL_GetError());
    SDL_SetRenderDrawColor(tx->render, 255, 255, 255, 255);
    SDL_RenderClear(tx->render);

    tx->texture = SDL_CreateTexture(tx->render, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);

    tx->screen = twin_screen_create(width, height, _twin_sdl_put_begin,
                                    _twin_sdl_put_span, tx);

    twin_set_file(twin_sdl_read_events, 0, TWIN_READ, tx);

    twin_set_work(twin_sdl_work, TWIN_WORK_REDISPLAY, tx);

    return tx;
}

void twin_sdl_destroy(twin_sdl_t *tx)
{
    SDL_DestroyRenderer(tx->render);
    SDL_DestroyWindow(tx->win);
    tx->win = 0;
    twin_screen_destroy(tx->screen);
    SDL_Quit();
}

void twin_sdl_damage(twin_sdl_t *tx, SDL_Event *ev maybe_unused)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_damage(tx->screen, 0, 0, width, height);
}

void twin_sdl_configure(twin_sdl_t *tx, SDL_Event *ev maybe_unused)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_resize(tx->screen, width, height);
}

void twin_sdl_update(twin_sdl_t *tx)
{
    twin_screen_update(tx->screen);
}

bool twin_sdl_process_events(twin_sdl_t *tx)
{
    bool result;

    _twin_run_work();
    result = twin_sdl_read_events(0, 0, tx);
    _twin_run_work();

    return result;
}
