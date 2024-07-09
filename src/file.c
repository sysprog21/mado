/*
 * Twin - A Tiny Window System
 * Copyright © 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "twinint.h"

#include <assert.h>
#include <sys/poll.h>
#include <unistd.h>

static twin_queue_t *head;

static twin_order_t _twin_file_order(twin_queue_t *a, twin_queue_t *b)
{
    twin_file_t *af = (twin_file_t *) a;
    twin_file_t *bf = (twin_file_t *) b;

    if (af->file < bf->file)
        return TWIN_BEFORE;
    if (af->file > bf->file)
        return TWIN_AFTER;
    return TWIN_AT;
}

int _twin_run_file(twin_time_t delay)
{
    twin_file_t *first;
    twin_file_t *file;
    int n;
    int i;
    int r;
    short events;
    twin_file_op_t op;
    struct pollfd *polls;

    first = (twin_file_t *) _twin_queue_set_order(&head);
    if (first) {
        for (file = first, n = 0; file;
             file = (twin_file_t *) file->queue.order, n++)
            ;
        polls = malloc(n * sizeof(struct pollfd));
        if (!polls)
            return 1;
        for (file = first, i = 0; file;
             file = (twin_file_t *) file->queue.order, i++) {
            short events = 0;

            if (file->ops & TWIN_READ)
                events |= POLLIN;
            if (file->ops & TWIN_WRITE)
                events |= POLLOUT;
            polls[i].fd = file->file;
            polls[i].events = events;
        }
        r = poll(polls, n, delay);
        if (r > 0)
            for (file = first, i = 0; file;
                 file = (twin_file_t *) file->queue.order, i++) {
                events = polls[i].revents;
                assert(polls[i].fd == file->file);
                op = 0;
                if (events & POLLIN)
                    op |= TWIN_READ;
                if (events & POLLOUT)
                    op |= TWIN_WRITE;
                if (op && !(*file->proc)(file->file, op, file->closure))
                    _twin_queue_delete(&head, &file->queue);
            }
        _twin_queue_review_order(&first->queue);
        free(polls);
        return 1;
    } else {
        if (delay > 0)
            usleep(delay * 1000);
        return 0;
    }
}

twin_file_t *twin_set_file(twin_file_proc_t file_proc,
                           int fd,
                           twin_file_op_t ops,
                           void *closure)
{
    twin_file_t *file = malloc(sizeof(twin_file_t));

    if (!file)
        return 0;

    file->file = fd;
    file->proc = file_proc;
    file->ops = ops;
    file->closure = closure;

    _twin_queue_insert(&head, _twin_file_order, &file->queue);
    return file;
}

void twin_clear_file(twin_file_t *file)
{
    _twin_queue_delete(&head, &file->queue);
}
