/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Tweens: time-based animation on top of timeouts
 *
 * Progress comes from elapsed time, not from counting frames, so a slow
 * frame shortens the animation rather than stretching it.  All arithmetic
 * is 16.16 fixed point.
 */

#include "twin_private.h"

#define TWIN_TWEEN_FRAME_MS 16

struct _twin_tween {
    twin_timeout_t *timeout;
    twin_time_t start, duration;
    twin_ease_t ease;
    twin_tween_proc_t proc;
    void *closure;
    bool running;  /* inside proc: defer frees */
    bool dead;     /* cancelled or finished from inside proc */
    bool finished; /* finished from inside proc: deliver the last frame */
};

twin_fixed_t twin_ease(twin_ease_t ease, twin_fixed_t t)
{
    if (t <= 0)
        return 0;
    if (t >= TWIN_FIXED_ONE)
        return TWIN_FIXED_ONE;

    switch (ease) {
    case TwinEaseOutCubic: {
        twin_fixed_t u = TWIN_FIXED_ONE - t;
        return TWIN_FIXED_ONE - twin_fixed_mul(twin_fixed_mul(u, u), u);
    }
    case TwinEaseInOutCubic:
        if (t < TWIN_FIXED_HALF)
            return 4 * twin_fixed_mul(twin_fixed_mul(t, t), t);
        else {
            twin_fixed_t u = 2 * (TWIN_FIXED_ONE - t);
            return TWIN_FIXED_ONE - twin_fixed_mul(twin_fixed_mul(u, u), u) / 2;
        }
    case TwinEaseOutBack: {
        /* 1 + c3 (t - 1)^3 + c1 (t - 1)^2, c1 = 1.70158, c3 = c1 + 1 */
        const twin_fixed_t c1 = 111514; /* 1.70158 */
        const twin_fixed_t c3 = c1 + TWIN_FIXED_ONE;
        twin_fixed_t u = t - TWIN_FIXED_ONE;
        twin_fixed_t u2 = twin_fixed_mul(u, u);
        return TWIN_FIXED_ONE + twin_fixed_mul(c3, twin_fixed_mul(u2, u)) +
               twin_fixed_mul(c1, u2);
    }
    case TwinEaseLinear:
    default:
        return t;
    }
}

static twin_time_t _twin_tween_tick(twin_time_t now, void *closure)
{
    twin_tween_t *tween = closure;
    twin_time_t elapsed =
        (twin_time_t) ((uint32_t) now - (uint32_t) tween->start);
    /* A tween of no duration ends at once, even on a tick stamped early */
    bool last = elapsed >= tween->duration || tween->duration <= 0;
    twin_fixed_t value;

    if (last)
        value = TWIN_FIXED_ONE;
    else
        /* elapsed / duration in 16.16; under 32 s it needs no 64-bit
         * division */
        value =
            twin_ease(tween->ease, twin_fixed_div(elapsed, tween->duration));

    tween->running = true;
    bool keep = (*tween->proc)(value, tween->closure);
    /* twin_tween_finish() from inside proc still promises the end value,
     * even when proc also returns false to stop */
    if (tween->finished && !last)
        (*tween->proc)(TWIN_FIXED_ONE, tween->closure);
    tween->running = false;

    if (last || !keep || tween->dead) {
        twin_free(tween);
        return -1; /* drop the timeout */
    }
    return TWIN_TWEEN_FRAME_MS;
}

twin_tween_t *twin_tween_start(twin_time_t duration,
                               twin_ease_t ease,
                               twin_tween_proc_t proc,
                               void *closure)
{
    twin_tween_t *tween = twin_calloc(1, sizeof(*tween));
    if (!tween)
        return NULL;
    tween->start = twin_now();
    tween->duration = duration > 0 ? duration : 0;
    tween->ease = ease;
    tween->proc = proc;
    tween->closure = closure;
    tween->timeout = twin_set_timeout(_twin_tween_tick, 0, tween);
    if (!tween->timeout) {
        twin_free(tween);
        return NULL;
    }
    return tween;
}

void twin_tween_cancel(twin_tween_t *tween)
{
    if (!tween)
        return;
    if (tween->running) {
        tween->dead = true;
        return;
    }
    twin_clear_timeout(tween->timeout);
    twin_free(tween);
}

void twin_tween_finish(twin_tween_t *tween)
{
    if (!tween)
        return;
    if (tween->running) {
        tween->dead = true;
        tween->finished = true;
        return;
    }
    twin_clear_timeout(tween->timeout);
    tween->running = true;
    (*tween->proc)(TWIN_FIXED_ONE, tween->closure);
    twin_free(tween);
}
