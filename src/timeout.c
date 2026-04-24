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
static twin_timeout_t *deferred_free;
static unsigned running_depth;

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

static void _twin_defer_timeout_free(twin_timeout_t *timeout)
{
    timeout->queue.next = deferred_free ? &deferred_free->queue : NULL;
    deferred_free = timeout;
}

static void _twin_flush_timeout_free(void)
{
    while (deferred_free) {
        twin_timeout_t *timeout = deferred_free;

        deferred_free = (twin_timeout_t *) deferred_free->queue.next;
        twin_free(timeout);
    }
}

static void _twin_release_timeout(twin_timeout_t *timeout)
{
    if (timeout->closure)
        _twin_closure_unref(timeout->closure);
    timeout->closure = NULL;

    if (running_depth)
        _twin_defer_timeout_free(timeout);
    else
        twin_free(timeout);
}

static void _twin_delete_timeout(twin_timeout_t *timeout)
{
    if (!timeout || timeout->queue.deleted)
        return;

    _twin_queue_delete(&head, &timeout->queue);
    _twin_release_timeout(timeout);
}

void _twin_run_timeout(void)
{
    twin_time_t now = twin_now();
    twin_timeout_t *timeout;
    twin_time_t delay;
    twin_timeout_t *first = NULL;

    if (running_depth++ == 0) {
        first = (twin_timeout_t *) _twin_queue_set_order(&head);
        for (timeout = first;
             timeout && twin_time_compare(now, >=, timeout->time);
             timeout = (twin_timeout_t *) timeout->queue.order) {
            /* Validate closure pointer using closure lifetime tracking */
            if (!_twin_closure_is_valid(timeout->closure))
                continue;

            delay = (*timeout->proc)(now, timeout->closure);
            if (timeout->queue.deleted) {
                continue;
            } else if (delay >= 0) {
                timeout->time = twin_now() + delay;
                _twin_queue_reorder(&head, _twin_timeout_order,
                                    &timeout->queue);
            } else {
                _twin_delete_timeout(timeout);
            }
        }
    } else {
        size_t count = 0;
        twin_timeout_t **snapshot;

        for (twin_queue_t *q = head; q; q = q->next) {
            timeout = (twin_timeout_t *) q;
            if (!twin_time_compare(now, >=, timeout->time))
                break;
            count++;
        }

        snapshot = count ? twin_malloc(sizeof(*snapshot) * count) : NULL;
        if (!count || snapshot) {
            size_t i = 0;

            for (twin_queue_t *q = head; q && i < count; q = q->next)
                snapshot[i++] = (twin_timeout_t *) q;

            for (i = 0; i < count; i++) {
                timeout = snapshot[i];
                if (timeout->queue.deleted)
                    continue;
                if (!_twin_closure_is_valid(timeout->closure))
                    continue;
                if (!twin_time_compare(now, >=, timeout->time))
                    break;

                delay = (*timeout->proc)(now, timeout->closure);
                if (timeout->queue.deleted) {
                    continue;
                } else if (delay >= 0) {
                    timeout->time = twin_now() + delay;
                    _twin_queue_reorder(&head, _twin_timeout_order,
                                        &timeout->queue);
                } else {
                    _twin_delete_timeout(timeout);
                }
            }

            twin_free(snapshot);
        }
    }

    if (--running_depth == 0) {
        if (first)
            _twin_queue_review_order(&first->queue);
        _twin_flush_timeout_free();
    } else if (first) {
        _twin_queue_review_order(&first->queue);
    }
}

twin_timeout_t *twin_set_timeout(twin_timeout_proc_t timeout_proc,
                                 twin_time_t delay,
                                 void *closure)
{
    twin_timeout_t *timeout = twin_malloc(sizeof(twin_timeout_t));
    if (!timeout)
        return NULL;

    /* Register closure with lifetime tracking if provided */
    if (closure) {
        if (!_twin_closure_register(closure)) {
            /* Failed to register closure */
            twin_free(timeout);
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

    _twin_delete_timeout(timeout);
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
