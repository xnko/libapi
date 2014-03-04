/* Copyright (c) 2014, Artak Khnkoyan <artak.khnkoyan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef API_LOOP_H_INCLUDED
#define API_LOOP_H_INCLUDED

#include <malloc.h>

#include "../api_loop_base.h"
#include "../api_task.h"
#include "api_mpscq.h"

typedef struct api_loop_t {
    api_loop_base_t base;
    int epoll;
    api_mpscq_t waiters;
    struct {
        void(*processor)(void* asyncs, int events);
        struct epoll_event e;
        int fd;
        api_mpscq_t queue;
    } asyncs;
} api_loop_t;

static int api_loop_update(api_loop_t* loop, int fd, struct epoll_event* e, int events)
{
    int error;
    e->events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;

    if ((events & API_READ) == API_READ)
        e->events |= (EPOLLIN | EPOLLPRI);

    if ((events & API_WRITE) == API_WRITE)
        e->events |= EPOLLOUT;

    error = epoll_ctl(loop->epoll, EPOLL_CTL_MOD, fd, e);
    if (error == 0)
        return API__OK;

    return api_error_translate(errno);
}

static int api_loop_read_add(api_loop_t* loop, int fd, struct epoll_event* e)
{
    if (e->events & EPOLLOUT)
        return api_loop_update(loop, fd, e, API_READ | API_WRITE);
    else
        return api_loop_update(loop, fd, e, API_READ);
}

static int api_loop_read_del(api_loop_t* loop, int fd, struct epoll_event* e)
{
    if (e->events & EPOLLOUT)
        return api_loop_update(loop, fd, e, API_WRITE);
    else
        return api_loop_update(loop, fd, e, 0);
}

static int api_loop_write_add(api_loop_t* loop, int fd, struct epoll_event* e)
{
    if (e->events & EPOLLIN)
        return api_loop_update(loop, fd, e, API_READ | API_WRITE);
    else
        return api_loop_update(loop, fd, e, API_WRITE);
}

static int api_loop_write_del(api_loop_t* loop, int fd, struct epoll_event* e)
{
    if (e->events & EPOLLIN)
        return api_loop_update(loop, fd, e, API_READ);
    else
        return api_loop_update(loop, fd, e, 0);
}

static uint64_t api_loop_ref(api_loop_t* loop)
{
    return __sync_add_and_fetch(&loop->base.refs, 1);
}

static uint64_t api_loop_unref(api_loop_t* loop)
{
    uint64_t refs = __sync_sub_and_fetch(&loop->base.refs, 1);

    if (refs == 0)
    {
        free(loop);
    }

    return refs;
}

#endif // API_LOOP_H_INCLUDED