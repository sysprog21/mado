/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#ifndef _TWIN_SDL_H_
#define _TWIN_SDL_H_

#include <SDL.h>
#include <SDL_render.h>

#include <twin.h>

typedef struct _twin_sdl {
    twin_screen_t *screen;
    SDL_Window *win;
    int dpy;
    int depth;
    int *pixels;
    SDL_Renderer *render;
    SDL_Texture *texture;
    twin_coord_t width;
    twin_coord_t height;
    int image_y;
} twin_sdl_t;

twin_sdl_t *twin_sdl_create_ext(int width, int height, int handle_events);

static inline twin_sdl_t *twin_sdl_create(int width, int height)
{
    return twin_sdl_create_ext(width, height, 1);
}

void twin_sdl_destroy(twin_sdl_t *tx);

void twin_sdl_damage(twin_sdl_t *tx, SDL_Event *ev);

void twin_sdl_configure(twin_sdl_t *tx, SDL_Event *ev);

void twin_sdl_update(twin_sdl_t *tx);

bool twin_sdl_process_events(twin_sdl_t *tx);

#endif /* _TWIN_SDL_H_ */
