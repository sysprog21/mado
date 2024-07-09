/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdio.h>

#include "twin_sdl.h"
#include "twinint.h"

static void _twin_x11_put_begin(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_coord_t bottom,
                                void *closure)
{
    twin_x11_t *tx = closure;
    tx->width = right - left;
    tx->height = bottom - top;
    tx->image_y = top;
}

static void _twin_x11_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    twin_x11_t *tx = closure;
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

static twin_bool_t twin_x11_read_events(int maybe_unused file,
                                        twin_file_op_t maybe_unused ops,
                                        void *closure)
{
    twin_x11_t *tx = closure;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        twin_event_t tev;
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
                ev.window.event == SDL_WINDOWEVENT_SHOWN) {
                twin_x11_damage(tx, &ev);
            }
            break;
        case SDL_QUIT:
            twin_x11_destroy(tx);
            return 0;
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
    return TWIN_TRUE;
}

static twin_bool_t twin_x11_work(void *closure)
{
    twin_x11_t *tx = closure;

    if (twin_screen_damaged(tx->screen)) {
        twin_x11_update(tx);
    }
    return TWIN_TRUE;
}

twin_x11_t *twin_x11_create_ext(int width, int height, int handle_events)
{
    twin_x11_t *tx;

    static char *argv[] = {"xtwin", 0};
    static int argc = 1;

    tx = malloc(sizeof(twin_x11_t));
    if (!tx)
        return 0;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        printf("error : %s\n", SDL_GetError());
    }
    tx->win = SDL_CreateWindow(argv[0], SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, width, height,
                               SDL_WINDOW_SHOWN);
    if (!tx->win) {
        printf("error : %s\n", SDL_GetError());
    }
    tx->pixels = malloc(width * height * sizeof(uint32_t));
    memset(tx->pixels, 255, width * height * sizeof(uint32_t));

    tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_ACCELERATED);
    if (!tx->render) {
        printf("error : %s\n", SDL_GetError());
    }
    SDL_SetRenderDrawColor(tx->render, 255, 255, 255, 255);
    SDL_RenderClear(tx->render);

    tx->texture = SDL_CreateTexture(tx->render, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STATIC, width, height);

    tx->screen = twin_screen_create(width, height, _twin_x11_put_begin,
                                    _twin_x11_put_span, tx);

    tx->dpy = SDL_GetWindowDisplayIndex(tx->win);

    if (handle_events)
        twin_set_file(twin_x11_read_events, tx->dpy, TWIN_READ, tx);

    twin_set_work(twin_x11_work, TWIN_WORK_REDISPLAY, tx);


    return tx;
}

void twin_x11_destroy(twin_x11_t *tx)
{
    SDL_DestroyRenderer(tx->render);
    SDL_DestroyWindow(tx->win);
    tx->win = 0;
    twin_screen_destroy(tx->screen);
    SDL_Quit();
}

void twin_x11_damage(twin_x11_t *tx, SDL_Event *ev)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_damage(tx->screen, 0, 0, width, height);
}

void twin_x11_configure(twin_x11_t *tx, SDL_Event *ev)
{
    int width, height;
    SDL_GetWindowSize(tx->win, &width, &height);
    twin_screen_resize(tx->screen, width, height);
}

void twin_x11_update(twin_x11_t *tx)
{
    twin_screen_update(tx->screen);
}

twin_bool_t twin_x11_process_events(twin_x11_t *tx)
{
    twin_bool_t result;

    _twin_run_work();
    result = twin_x11_read_events(tx->dpy, 0, tx);
    _twin_run_work();

    return result;
}
