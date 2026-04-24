/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

static twin_queue_t *head;
static twin_work_t *deferred_free;
static unsigned running_depth;

static twin_order_t _twin_work_order(twin_queue_t *a, twin_queue_t *b)
{
    const twin_work_t *aw = (twin_work_t *) a, *bw = (twin_work_t *) b;

    if (aw->priority < bw->priority)
        return TWIN_BEFORE;
    if (aw->priority > bw->priority)
        return TWIN_AFTER;
    return TWIN_AT;
}

static void _twin_queue_work(twin_work_t *work)
{
    _twin_queue_insert(&head, _twin_work_order, &work->queue);
}

static void _twin_defer_work_free(twin_work_t *work)
{
    work->queue.next = deferred_free ? &deferred_free->queue : NULL;
    deferred_free = work;
}

static void _twin_flush_work_free(void)
{
    while (deferred_free) {
        twin_work_t *work = deferred_free;

        deferred_free = (twin_work_t *) deferred_free->queue.next;
        twin_free(work);
    }
}

static void _twin_release_work(twin_work_t *work)
{
    if (work->closure)
        _twin_closure_unref(work->closure);
    work->closure = NULL;

    if (running_depth)
        _twin_defer_work_free(work);
    else
        twin_free(work);
}

static void _twin_delete_work(twin_work_t *work)
{
    if (!work || work->queue.deleted)
        return;

    _twin_queue_delete(&head, &work->queue);
    _twin_release_work(work);
}

void _twin_run_work(void)
{
    twin_work_t *work;
    twin_work_t *first = NULL;

    if (running_depth++ == 0) {
        first = (twin_work_t *) _twin_queue_set_order(&head);
        for (work = first; work; work = (twin_work_t *) work->queue.order) {
            /* Validate closure pointer using closure lifetime tracking */
            if (!_twin_closure_is_valid(work->closure))
                continue;

            if (!(*work->proc)(work->closure))
                _twin_delete_work(work);
            else if (!work->queue.deleted)
                _twin_queue_reorder(&head, _twin_work_order, &work->queue);
        }
    } else {
        size_t count = 0;
        twin_work_t **snapshot;

        for (twin_queue_t *q = head; q; q = q->next)
            count++;

        snapshot = count ? twin_malloc(sizeof(*snapshot) * count) : NULL;
        if (!count || snapshot) {
            size_t i = 0;

            for (twin_queue_t *q = head; q; q = q->next)
                snapshot[i++] = (twin_work_t *) q;

            for (i = 0; i < count; i++) {
                work = snapshot[i];
                if (work->queue.deleted)
                    continue;
                if (!_twin_closure_is_valid(work->closure))
                    continue;

                if (!(*work->proc)(work->closure))
                    _twin_delete_work(work);
                else if (!work->queue.deleted)
                    _twin_queue_reorder(&head, _twin_work_order, &work->queue);
            }

            twin_free(snapshot);
        }
    }

    if (--running_depth == 0) {
        if (first)
            _twin_queue_review_order(&first->queue);
        _twin_flush_work_free();
    } else if (first) {
        _twin_queue_review_order(&first->queue);
    }
}

twin_work_t *twin_set_work(twin_work_proc_t work_proc,
                           int priority,
                           void *closure)
{
    twin_work_t *work = twin_malloc(sizeof(twin_work_t));
    if (!work)
        return NULL;

    /* Register closure with lifetime tracking if provided */
    if (closure) {
        if (!_twin_closure_register(closure)) {
            /* Failed to register closure */
            twin_free(work);
            return NULL;
        }
        /* Add reference for this work item */
        _twin_closure_ref(closure);
    }

    work->proc = work_proc;
    work->priority = priority;
    work->closure = closure;
    _twin_queue_work(work);
    return work;
}

void twin_clear_work(twin_work_t *work)
{
    if (!work)
        return;

    _twin_delete_work(work);
}
