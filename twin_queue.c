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

void
_twin_queue_insert (twin_queue_t	**head,
		    twin_queue_proc_t	proc,
		    twin_queue_t	*new)
{
    twin_queue_t **prev, *q;

    for (prev = head; (q = *prev); prev = &q->next)
	if ((*proc) (new, q) == TWIN_AFTER)
	    break;
    new->next = *prev;
    new->order = 0;
    new->walking = TWIN_FALSE;
    new->deleted = TWIN_FALSE;
    *prev = new;
}

void
_twin_queue_remove (twin_queue_t	**head,
		    twin_queue_t	*old)
{
    twin_queue_t **prev, *q;

    for (prev = head; (q = *prev); prev = &q->next)
	if (q == old)
	{
	    *prev = q->next;
	    break;
	}
}

void
_twin_queue_delete (twin_queue_t	**head,
		    twin_queue_t	*old)
{
    _twin_queue_remove (head, old);
    old->deleted = TWIN_TRUE;
    if (!old->walking)
	free (old);
}

twin_queue_t *
_twin_queue_set_order (twin_queue_t	**head)
{
    twin_queue_t *first = *head;
    twin_queue_t *q;

    for (q = first; q; q = q->next)
    {
	q->order = q->next;
	q->walking = TWIN_TRUE;
    }
    return first;
}

void
_twin_queue_review_order (twin_queue_t	*first)
{
    twin_queue_t *q, *o;

    for (q = first; q; q = o)
    {
	o = q->order;
	q->order = 0;
	q->walking = TWIN_FALSE;
	if (q->deleted)
	    free (q);
    }
}
