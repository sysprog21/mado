/*
 * $Id$
 *
 * Copyright © 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
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
