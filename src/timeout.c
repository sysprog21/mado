/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "twin_private.h"

#define twin_time_compare(a, op, b) (((a) - (b)) op 0)

static twin_time_t twin_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static twin_queue_t *head;
static twin_time_t start;

static twin_order_t _twin_timeout_order(twin_queue_t *a, twin_queue_t *b)
{
    const twin_timeout_t *at = (twin_timeout_t *) a;
    const twin_timeout_t *bt = (twin_timeout_t *) b;

    if (twin_time_compare(at->time, <, bt->time))
        return TWIN_AFTER;
    if (twin_time_compare(at->time, >, bt->time))
        return TWIN_BEFORE;
    return TWIN_AT;
}

static void _twin_queue_timeout(twin_timeout_t *timeout, twin_time_t time)
{
    timeout->time = time;
    _twin_queue_remove(&head, &timeout->queue);
    _twin_queue_insert(&head, _twin_timeout_order, &timeout->queue);
}

void _twin_run_timeout(void)
{
    twin_time_t now = twin_now();
    twin_timeout_t *timeout;
    twin_time_t delay;

    twin_timeout_t *first = (twin_timeout_t *) _twin_queue_set_order(&head);
    for (timeout = first; timeout && twin_time_compare(now, >=, timeout->time);
         timeout = (twin_timeout_t *) timeout->queue.order) {
        /* Validate closure pointer using closure lifetime tracking */
        if (!_twin_closure_is_valid(timeout->closure)) {
            /* Invalid or freed closure, skip this timeout */
            continue;
        }

        delay = (*timeout->proc)(now, timeout->closure);
        if (delay >= 0) {
            timeout->time = twin_now() + delay;
            _twin_queue_reorder(&head, _twin_timeout_order, &timeout->queue);
        } else
            _twin_queue_delete(&head, &timeout->queue);
    }
    _twin_queue_review_order(&first->queue);
}

twin_timeout_t *twin_set_timeout(twin_timeout_proc_t timeout_proc,
                                 twin_time_t delay,
                                 void *closure)
{
    twin_timeout_t *timeout = malloc(sizeof(twin_timeout_t));
    if (!timeout)
        return NULL;

    /* Register closure with lifetime tracking if provided */
    if (closure) {
        if (!_twin_closure_register(closure)) {
            /* Failed to register closure */
            free(timeout);
            return NULL;
        }
        /* Add reference for this timeout */
        _twin_closure_ref(closure);
    }

    if (!start)
        start = twin_now();
    timeout->delay = delay;
    timeout->proc = timeout_proc;
    timeout->closure = closure;
    _twin_queue_timeout(timeout, twin_now() + delay);
    return timeout;
}

void twin_clear_timeout(twin_timeout_t *timeout)
{
    if (!timeout)
        return;

    _twin_queue_delete(&head, &timeout->queue);

    /* Unref the closure */
    if (timeout->closure)
        _twin_closure_unref(timeout->closure);

    free(timeout);
}

twin_time_t _twin_timeout_delay(void)
{
    if (head) {
        const twin_timeout_t *thead = (twin_timeout_t *) head;
        twin_time_t now = twin_now();
        if (twin_time_compare(now, >=, thead->time))
            return 0;
        return thead->time - now;
    }
    return -1;
}
