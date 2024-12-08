/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include <stdlib.h>

#include "twin_private.h"

void _twin_queue_insert(twin_queue_t **head,
                        twin_queue_proc_t proc,
                        twin_queue_t *new)
{
    twin_queue_t **prev, *q;

    for (prev = head; (q = *prev); prev = &q->next)
        if ((*proc)(new, q) == TWIN_AFTER)
            break;
    new->next = *prev;
    new->order = 0;
    new->deleted = false;
    *prev = new;
}

void _twin_queue_remove(twin_queue_t **head, twin_queue_t *old)
{
    twin_queue_t **prev, *q;

    for (prev = head; (q = *prev); prev = &q->next)
        if (q == old) {
            *prev = q->next;
            break;
        }
}

void _twin_queue_reorder(twin_queue_t **head,
                         twin_queue_proc_t proc,
                         twin_queue_t *elem)
{
    twin_queue_t **prev, *q;

    _twin_queue_remove(head, elem);

    for (prev = head; (q = *prev); prev = &q->next)
        if ((*proc)(elem, q) == TWIN_AFTER)
            break;
    elem->next = *prev;
    *prev = elem;
}

void _twin_queue_delete(twin_queue_t **head, twin_queue_t *old)
{
    _twin_queue_remove(head, old);
    old->deleted = true;
}

twin_queue_t *_twin_queue_set_order(twin_queue_t **head)
{
    twin_queue_t *first = *head;

    for (twin_queue_t *q = first; q; q = q->next) {
        q->order = q->next;
    }
    return first;
}

void _twin_queue_review_order(twin_queue_t *first)
{
    twin_queue_t *q, *o;

    for (q = first; q; q = o) {
        o = q->order;
        q->order = 0;
        if (q->deleted)
            free(q);
    }
}
