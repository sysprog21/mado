/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "twin_private.h"

extern twin_backend_t g_twin_backend;

/* Execute a single iteration of the event dispatch loop.
 *
 * This function processes one iteration of the event loop and is designed
 * for integration with external event loops (e.g., browser, GUI toolkits).
 *
 * Processing order:
 *   1. Run pending timeout callbacks
 *   2. Execute queued work items
 *   3. Poll backend for new events
 *
 * @ctx : The Twin context to dispatch events for (must not be NULL)
 * Return false if backend requested termination (e.g., window closed),
 *        true otherwise
 *
 * Example usage with Emscripten:
 *   static void main_loop(void *arg)
 *   {
 *       if (!twin_dispatch_once((twin_context_t *) arg))
 *           emscripten_cancel_main_loop();
 *   }
 *   emscripten_set_main_loop_arg(main_loop, ctx, 0, 1);
 */
bool twin_dispatch_once(twin_context_t *ctx)
{
    /* Validate context to prevent null pointer dereference in callbacks */
    if (!ctx) {
        log_error("twin_dispatch_once: NULL context");
        return false;
    }

    _twin_run_timeout();
    _twin_run_work();

    /* Poll backend for events. Returns false when user requests exit.
     * If no poll function is registered, assume no event source and yield CPU.
     */
    if (g_twin_backend.poll) {
        if (!g_twin_backend.poll(ctx))
            return false;
    } else {
        log_warn("twin_dispatch_once: No backend poll function registered");
        /* Yield CPU to avoid busy-waiting when no event source available */
#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#elif defined(_POSIX_VERSION)
        usleep(1000); /* 1ms sleep */
#endif
    }

    return true;
}

/* Run the main event dispatch loop (native platforms only).
 *
 * This function runs an infinite event loop, processing timeouts, work items,
 * and backend events. The loop exits when the backend's poll function returns
 * false, typically when the user closes the window or requests termination.
 *
 * @ctx : The Twin context to dispatch events for (must not be NULL)
 *
 * Platform notes:
 *   - Native builds: Blocks until backend terminates
 *   - Emscripten: This function is not available. Use twin_dispatch_once()
 *                 with emscripten_set_main_loop_arg() instead.
 *
 * See also: twin_dispatch_once() for single-iteration event processing
 */
void twin_dispatch(twin_context_t *ctx)
{
#ifdef __EMSCRIPTEN__
    /* Emscripten builds must use emscripten_set_main_loop_arg() with
     * twin_dispatch_once() to integrate with the browser's event loop.
     * Calling twin_dispatch() directly will not work in WebAssembly.
     */
    (void) ctx; /* Unused in Emscripten builds */
    log_error(
        "twin_dispatch() called in Emscripten build - use "
        "twin_dispatch_once() with emscripten_set_main_loop_arg()");
    return;
#else
    /* Validate context before entering event loop */
    if (!ctx) {
        log_error("twin_dispatch: NULL context");
        return;
    }

    /* Main event loop - runs until backend requests termination */
    for (;;) {
        if (!twin_dispatch_once(ctx))
            break;
    }
#endif
}
