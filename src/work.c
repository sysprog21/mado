/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

static twin_queue_t    *head;

static twin_order_t
_twin_work_order (twin_queue_t *a, twin_queue_t *b)
{
    twin_work_t	*aw = (twin_work_t *) a;
    twin_work_t	*bw = (twin_work_t *) b;
    
    if (aw->priority < bw->priority)
	return TWIN_BEFORE;
    if (aw->priority > bw->priority)
	return TWIN_AFTER;
    return TWIN_AT;
}

static void
_twin_queue_work (twin_work_t   *work)
{
    _twin_queue_insert (&head, _twin_work_order, &work->queue);
}

void
_twin_run_work (void)
{
    twin_work_t	*work;
    twin_work_t	*first;
    
    first = (twin_work_t *) _twin_queue_set_order (&head);
    for (work = first; work; work = (twin_work_t *) work->queue.order)
	if (!(*work->proc) (work->closure))
	    _twin_queue_delete (&head, &work->queue);
    _twin_queue_review_order (&first->queue);
}

twin_work_t *
twin_set_work (twin_work_proc_t	work_proc,
		  int			priority,
		  void			*closure)
{
    twin_work_t	*work = malloc (sizeof (twin_work_t));

    if (!work)
	return 0;

    work->proc = work_proc;
    work->priority = priority;
    work->closure = closure;
    _twin_queue_work (work);
    return work;
}

void
twin_clear_work (twin_work_t *work)
{
    _twin_queue_delete (&head, &work->queue);
}
