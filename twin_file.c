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

#include <sys/poll.h>
#include <assert.h>
#include <unistd.h>

static twin_queue_t	*head;

static twin_order_t
_twin_file_order (twin_queue_t *a, twin_queue_t *b)
{
    twin_file_t	*af = (twin_file_t *) a;
    twin_file_t	*bf = (twin_file_t *) b;
    
    if (af->file < bf->file)
	return TWIN_BEFORE;
    if (af->file > bf->file)
	return TWIN_AFTER;
    return TWIN_AT;
}

void
_twin_run_file (twin_time_t delay)
{
    twin_file_t	*first;
    twin_file_t	*file;
    int			n;
    int			i;
    int			r;
    short		events;
    twin_file_op_t	op;
    struct pollfd	*polls;
    
    first = (twin_file_t *) _twin_queue_set_order (&head);
    if (first)
    {
	for (file = first, n = 0; file; file = (twin_file_t *) file->queue.order, n++);
	polls = malloc (n * sizeof (struct pollfd));
	if (!polls)
	    return;
	for (file = first, i = 0; file; file = (twin_file_t *) file->queue.order, i++)
	{
	    short   events = 0;
    
	    if (file->ops & TWIN_READ)
		events |= POLLIN;
	    if (file->ops & TWIN_WRITE)
		events |= POLLOUT;
	    polls[i].fd = file->file;
	    polls[i].events = events;
	}
	r = poll (polls, n, delay);
	if (r > 0) 
	    for (file = first, i = 0; file; file = (twin_file_t *) file->queue.order, i++)
	    {
		events = polls[i].revents;
		assert (polls[i].fd == file->file);
		op = 0;
		if (events & POLLIN)
		    op |= TWIN_READ;
		if (events & POLLOUT)
		    op |= TWIN_WRITE;
		if (op && !(*file->proc) (file->file, op, file->closure))
		    _twin_queue_delete (&head, &file->queue);
	    }
	_twin_queue_review_order (&first->queue);
	free (polls);
    }
    else
    {
	if (delay > 0)
	    usleep (delay * 1000);
    }
}

twin_file_t *
twin_set_file (twin_file_proc_t	file_proc,
		  int			fd,
		  twin_file_op_t	ops,
		  void			*closure)
{
    twin_file_t	*file = malloc (sizeof (twin_file_t));

    if (!file)
	return 0;

    file->file = fd;
    file->proc = file_proc;
    file->ops = ops;
    file->closure = closure;
    
    _twin_queue_insert (&head, _twin_file_order, &file->queue);
    return file;
}

void
twin_clear_file (twin_file_t *file)
{
    _twin_queue_delete (&head, &file->queue);
}
