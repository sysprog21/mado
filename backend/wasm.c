/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* WebAssembly backend for Mado
 *
 * This backend provides direct Canvas API integration without SDL dependency,
 * significantly reducing binary size. Key features:
 * - Direct Canvas 2D API rendering via EM_ASM
 * - Browser-native image decoding (JPEG/PNG)
 * - Single-threaded design
 * - Event handling through exported C functions
 */

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>

#include "twin_private.h"

typedef struct {
    twin_argb32_t *framebuffer; /* ARGB32 pixel buffer */
    twin_coord_t width, height;
    bool running;
} twin_wasm_t;

#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_wasm_t *) ((twin_context_t *) x)->priv)

/* Global context pointer for event callbacks
 * WebAssembly has single-threaded execution, so this is safe.
 */
static twin_context_t *g_wasm_ctx = NULL;

/* Rendering callbacks */

static void _twin_wasm_put_begin(twin_coord_t left,
                                 twin_coord_t top,
                                 twin_coord_t right,
                                 twin_coord_t bottom,
                                 void *closure)
{
    (void) left;
    (void) top;
    (void) right;
    (void) bottom;
    (void) closure;
}

static void _twin_wasm_put_span(twin_coord_t left,
                                twin_coord_t top,
                                twin_coord_t right,
                                twin_argb32_t *pixels,
                                void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_wasm_t *tw = PRIV(closure);

    if (!tw->framebuffer)
        return;

    /* Copy pixel data to framebuffer */
    twin_coord_t width = screen->width;
    for (twin_coord_t x = left; x < right; x++)
        tw->framebuffer[top * width + x] = *pixels++;
}

/* Called after rendering is complete - flush to Canvas */
static bool _twin_wasm_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);

    if (twin_screen_damaged(screen)) {
        twin_screen_update(screen);

        /* Notify JavaScript that framebuffer has been updated.
         * JavaScript should use mado_wasm_get_*() functions to access
         * framebuffer data directly instead of receiving parameters.
         * This eliminates parameter passing overhead on every frame.
         */
        /* clang-format off */
        EM_ASM({
            if (typeof MadoCanvas !== 'undefined')
                MadoCanvas.updateCanvas();
        });
        /* clang-format on */
    }

    return true;
}

/* Backend initialization */

static twin_context_t *twin_wasm_init(int width, int height)
{
    /* Allocate context */
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;

    /* Allocate private data */
    twin_wasm_t *tw = calloc(1, sizeof(twin_wasm_t));
    if (!tw) {
        free(ctx);
        return NULL;
    }

    tw->width = width;
    tw->height = height;
    tw->running = true;

    /* Allocate framebuffer (ARGB32 format) */
    tw->framebuffer = calloc(width * height, sizeof(twin_argb32_t));
    if (!tw->framebuffer) {
        free(tw);
        free(ctx);
        return NULL;
    }

    ctx->priv = tw;

    /* Create screen with our rendering callbacks */
    ctx->screen = twin_screen_create(width, height, _twin_wasm_put_begin,
                                     _twin_wasm_put_span, ctx);
    if (!ctx->screen) {
        free(tw->framebuffer);
        free(tw);
        free(ctx);
        return NULL;
    }

    /* Register work callback for rendering */
    twin_set_work(_twin_wasm_work, 0, ctx);

    /* Store global context before JavaScript initialization
     * (required by accessor functions)
     */
    g_wasm_ctx = ctx;

    /* Initialize Canvas via JavaScript */
    EM_ASM_({ MadoCanvas.init($0, $1); }, width, height);

    /* Note: Framebuffer caching is done in mado-wasm.js onRuntimeInitialized
     * to ensure Module.HEAPU32 is fully initialized
     */

    return ctx;
}

static void twin_wasm_configure(twin_context_t *ctx)
{
    /* WebAssembly configuration
     * For now, we use fixed canvas size from initialization.
     * TODO: Support dynamic canvas resizing via JS callback.
     */
    (void) ctx;
}

/* Event polling - called by main loop
 * Events are injected via exported C functions from JavaScript
 */
static bool twin_wasm_poll(twin_context_t *ctx)
{
    twin_wasm_t *tw = PRIV(ctx);
    return tw->running;
}

/* Emscripten main loop state */
static void (*g_wasm_init_callback)(twin_context_t *) = NULL;
static bool g_wasm_initialized = false;

/* Main loop callback for Emscripten */
static void twin_wasm_loop(void *arg)
{
    twin_context_t *ctx = (twin_context_t *) arg;

    /* Perform one-time initialization on first iteration */
    if (!g_wasm_initialized && g_wasm_init_callback) {
        g_wasm_init_callback(ctx);
        g_wasm_initialized = true;
    }

    /* Process work queue, timeouts, and events */
    twin_dispatch_once(ctx);
}

/* Start the main loop */
static void twin_wasm_start(twin_context_t *ctx,
                            void (*init_callback)(twin_context_t *))
{
    g_wasm_init_callback = init_callback;
    g_wasm_initialized = false;

    /* Set up Emscripten main loop
     * Parameters:
     * - callback function
     * - callback argument (ctx)
     * - FPS (0 = browser's requestAnimationFrame)
     * - simulate_infinite_loop (1 = yes)
     */
    emscripten_set_main_loop_arg(twin_wasm_loop, ctx, 0, 1);
}

/* Cleanup */
static void twin_wasm_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_wasm_t *tw = PRIV(ctx);

    /* Cancel main loop */
    emscripten_cancel_main_loop();

    /* Cleanup Canvas via JavaScript */
    EM_ASM({ MadoCanvas.destroy(); });

    /* Free memory */
    if (tw) {
        free(tw->framebuffer);
        free(tw);
    }
    free(ctx);

    g_wasm_ctx = NULL;
}

/* Exported functions for JavaScript event injection */

/* Mouse events */
EMSCRIPTEN_KEEPALIVE
void mado_wasm_mouse_motion(int x, int y, int button_state)
{
    if (!g_wasm_ctx)
        return;

    twin_event_t tev = {
        .kind = TwinEventMotion,
        .u.pointer.screen_x = x,
        .u.pointer.screen_y = y,
        .u.pointer.button = button_state,
    };
    twin_screen_dispatch(SCREEN(g_wasm_ctx), &tev);
}

EMSCRIPTEN_KEEPALIVE
void mado_wasm_mouse_button(int x, int y, int button, bool down)
{
    if (!g_wasm_ctx)
        return;

    twin_event_t tev;
    tev.kind = down ? TwinEventButtonDown : TwinEventButtonUp;
    tev.u.pointer.screen_x = x;
    tev.u.pointer.screen_y = y;
    tev.u.pointer.button = (1 << button);

    twin_screen_dispatch(SCREEN(g_wasm_ctx), &tev);
}

/* Keyboard events */
EMSCRIPTEN_KEEPALIVE
void mado_wasm_key(int keycode, bool down)
{
    if (!g_wasm_ctx)
        return;

    twin_event_t tev;
    tev.kind = down ? TwinEventKeyDown : TwinEventKeyUp;
    tev.u.key.key = keycode;

    twin_screen_dispatch(SCREEN(g_wasm_ctx), &tev);
}

/* Framebuffer accessor functions for optimized Canvas updates
 *
 * These allow JavaScript to access framebuffer data directly without
 * passing pointers through EM_ASM on every frame. JavaScript should:
 * 1. Call these once at initialization to get buffer pointer and dimensions
 * 2. Access WASM memory directly via Module.HEAPU32
 * 3. Convert ARGB32 → RGBA in JavaScript
 *
 * This eliminates repeated parameter passing and improves performance.
 */

EMSCRIPTEN_KEEPALIVE
twin_argb32_t *mado_wasm_get_framebuffer(void)
{
    if (!g_wasm_ctx)
        return NULL;

    twin_wasm_t *tw = PRIV(g_wasm_ctx);
    return tw->framebuffer;
}

EMSCRIPTEN_KEEPALIVE
int mado_wasm_get_width(void)
{
    if (!g_wasm_ctx)
        return 0;

    twin_wasm_t *tw = PRIV(g_wasm_ctx);
    return tw->width;
}

EMSCRIPTEN_KEEPALIVE
int mado_wasm_get_height(void)
{
    if (!g_wasm_ctx)
        return 0;

    twin_wasm_t *tw = PRIV(g_wasm_ctx);
    return tw->height;
}

/* Shutdown */
EMSCRIPTEN_KEEPALIVE
void mado_wasm_shutdown(void)
{
    if (!g_wasm_ctx)
        return;

    twin_wasm_t *tw = PRIV(g_wasm_ctx);
    tw->running = false;
}

/* Register the WebAssembly backend */
const twin_backend_t g_twin_backend = {
    .init = twin_wasm_init,
    .configure = twin_wasm_configure,
    .poll = twin_wasm_poll,
    .start = twin_wasm_start,
    .exit = twin_wasm_exit,
};
