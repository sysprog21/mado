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
twin_mutex_init (twin_mutex_t *mutex)
{
#if HAVE_PTHREAD_H
    pthread_mutex_init (mutex, NULL);
#endif
}

void
twin_mutex_lock (twin_mutex_t *mutex)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (mutex);
#endif
}

void
twin_mutex_unlock (twin_mutex_t *mutex)
{
#if HAVE_PTHREAD_H
    pthread_mutex_unlock (mutex);
#endif
}

void
twin_cond_init (twin_cond_t *cond)
{
#if HAVE_PTHREAD_H
    pthread_cond_init (cond, NULL);
#endif
}

void
twin_cond_broadcast (twin_cond_t *cond)
{
#if HAVE_PTHREAD_H
    pthread_cond_broadcast (cond);
#else
    OOPS - need some synchronization mechanism
#endif
}

void
twin_cond_wait (twin_cond_t *cond, twin_mutex_t *mutex)
{
#if HAVE_PTHREAD_H
    pthread_cond_wait (cond, mutex);
#else
    OOPS - need some synchronization mechanism
#endif
}

int
twin_thread_create (twin_thread_t *thread, twin_thread_func_t func, void *arg)
{
    return pthread_create (thread, NULL, func, arg);
}
