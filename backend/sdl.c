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
    /* Union of the rectangles scanned out since the last present */
    twin_coord_t dirty_left, dirty_top, dirty_right, dirty_bottom;
    bool dirty;
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
    if (!tx->dirty) {
        tx->dirty_left = left;
        tx->dirty_top = top;
        tx->dirty_right = right;
        tx->dirty_bottom = bottom;
        tx->dirty = true;
        return;
    }
    if (left < tx->dirty_left)
        tx->dirty_left = left;
    if (top < tx->dirty_top)
        tx->dirty_top = top;
    if (right > tx->dirty_right)
        tx->dirty_right = right;
    if (bottom > tx->dirty_bottom)
        tx->dirty_bottom = bottom;
}

static void _twin_sdl_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_sdl_t *tx = PRIV(closure);

    memcpy(&tx->pixels[top * screen->width + left], pixels,
           (size_t) (right - left) * sizeof(*pixels));
}

/* Upload only what changed, then present once per screen update */
static void _twin_sdl_present(twin_screen_t *screen, twin_sdl_t *tx)
{
    if (!tx->dirty)
        return;
    SDL_Rect r = {tx->dirty_left, tx->dirty_top,
                  tx->dirty_right - tx->dirty_left,
                  tx->dirty_bottom - tx->dirty_top};
    SDL_UpdateTexture(tx->texture, &r, &tx->pixels[r.y * screen->width + r.x],
                      screen->width * sizeof(*tx->pixels));
    SDL_RenderCopy(tx->render, tx->texture, NULL, NULL);
    SDL_RenderPresent(tx->render);
    tx->dirty = false;
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

static bool twin_sdl_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);

    if (twin_screen_damaged(screen)) {
        twin_screen_update(screen);
        _twin_sdl_present(screen, PRIV(closure));
    }
    return true;
}

/*
 * Window scale: MADO_SCALE=n, or 2x for phone-sized screens so a 240x320
 * display is readable on a desktop monitor.  The renderer's logical size
 * maps mouse coordinates back to screen pixels.
 */
static int _twin_sdl_scale(int width, int height)
{
    const char *env = getenv("MADO_SCALE");
    if (env) {
        int s = atoi(env);
        if (s >= 1 && s <= 8)
            return s;
    }
    return (width <= 320 && height <= 480) ? 2 : 1;
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
    int scale = _twin_sdl_scale(width, height);
    tx->win = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, width * scale,
                               height * scale, SDL_WINDOW_SHOWN);
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

    /* Mado composites in software anyway: any renderer that can present a
     * texture will do, so fall back when there is no GPU. */
    tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_ACCELERATED);
    if (!tx->render)
        tx->render = SDL_CreateRenderer(tx->win, -1, SDL_RENDERER_SOFTWARE);
    if (!tx->render) {
        log_error("%s", SDL_GetError());
        goto bail_pixels;
    }
    SDL_RenderSetLogicalSize(tx->render, width, height);
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
    /* Screen pixels are the renderer's logical size, not the (scaled)
     * window size; the pixel buffer was allocated for the former. */
    int width, height;
    SDL_RenderGetLogicalSize(PRIV(ctx)->render, &width, &height);
    if (width > 0 && height > 0)
        twin_screen_resize(ctx->screen, width, height);
}

/* Deliver a character to the focused window */
static void _twin_sdl_ucs4(twin_screen_t *screen, twin_ucs4_t ucs4)
{
    twin_event_t tev = {.kind = TwinEventUcs4};
    tev.u.ucs4.ucs4 = ucs4;
    twin_screen_dispatch(screen, &tev);
}

static bool twin_sdl_poll(twin_context_t *ctx)
{
    twin_screen_t *screen = SCREEN(ctx);
    twin_sdl_t *tx = PRIV(ctx);

    /* Sleep until input arrives or the next timeout is due instead of
     * spinning, waking at least every 100 ms */
    if (!twin_screen_damaged(screen)) {
        twin_time_t wait = _twin_timeout_delay();
        if (wait < 0 || wait > 100)
            wait = 100;
        if (wait > 0)
            SDL_WaitEventTimeout(NULL, wait);
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        twin_event_t tev;
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
                ev.window.event == SDL_WINDOWEVENT_SHOWN) {
                twin_screen_damage(screen, 0, 0, screen->width, screen->height);
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
            /* Alt+Tab cycles the windows instead of reaching the app */
            if (ev.key.keysym.sym == SDLK_TAB &&
                (ev.key.keysym.mod & KMOD_ALT)) {
                if (ev.type == SDL_KEYDOWN)
                    twin_screen_focus_next(screen);
                break;
            }
            tev.u.key.key = ev.key.keysym.sym;
            tev.kind = ((ev.key.type == SDL_KEYDOWN) ? TwinEventKeyDown
                                                     : TwinEventKeyUp);
            twin_screen_dispatch(screen, &tev);
            /* Editing keys produce no SDL text input; send them as UCS-4 */
            if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    _twin_sdl_ucs4(screen, '\r');
                    break;
                case SDLK_BACKSPACE:
                    _twin_sdl_ucs4(screen, 8);
                    break;
                case SDLK_ESCAPE:
                    _twin_sdl_ucs4(screen, 27);
                    break;
                default:
                    break;
                }
            }
            break;
        case SDL_TEXTINPUT: {
            twin_ucs4_t ucs4;
            const char *p = ev.text.text;
            int len;
            while ((len = _twin_utf8_to_ucs4(p, &ucs4)) > 0) {
                _twin_sdl_ucs4(screen, ucs4);
                p += len;
            }
            break;
        }
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
