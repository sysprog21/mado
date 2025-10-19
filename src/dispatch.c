/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <unistd.h>

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
 * Platform behavior:
 *   - Native: Returns control immediately after one iteration
 *   - Emscripten/WASM: Must be called via emscripten_set_main_loop_arg()
 *     to integrate with browser's event loop (requestAnimationFrame)
 *
 * @ctx : The Twin context to dispatch events for (must not be NULL)
 * Return false if backend requested termination (e.g., window closed),
 *        true otherwise to continue dispatching
 *
 * Example usage with Emscripten (REQUIRED for WASM backends):
 *   static void main_loop(void *arg)
 *   {
 *       if (!twin_dispatch_once((twin_context_t *) arg))
 *           emscripten_cancel_main_loop();
 *   }
 *   emscripten_set_main_loop_arg(main_loop, ctx, 0, 1);
 *
 * Example usage in native backends (alternative to twin_dispatch):
 *   while (twin_dispatch_once(ctx))
 *       ; // Continue until termination requested
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

        /* CPU yielding strategy (platform-dependent):
         *
         * Emscripten/WASM builds:
         *   No explicit yielding needed. When this function returns, control
         *   goes back to the browser's event loop (set up via
         *   emscripten_set_main_loop_arg). The browser automatically handles
         *   scheduling via requestAnimationFrame(), ensuring smooth rendering
         *   without consuming 100% CPU.
         *
         * Native POSIX builds:
         *   Use usleep(1000) to yield CPU for 1ms, preventing busy-waiting
         *   when there's no event source. This is a fallback for backends
         *   that don't implement the poll() function properly.
         */
#if !defined(__EMSCRIPTEN__) && defined(_POSIX_VERSION)
        usleep(1000); /* 1ms sleep to prevent busy-waiting */
#endif
        /* WASM: No action needed - browser handles scheduling */
    }

    return true;
}
