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

#include "api_error.h"
#include "api_misc.h"
#include "api_async.h"

void* api_async_task_fn(api_task_t* task)
{
    api_async_t* async = (api_async_t*)task->data;
    async->callback(async->loop, async->arg);
    api_free(&async->loop->base.pool, sizeof(*async), async);

    return 0;
}

void* api_exec_task_fn(api_task_t* task)
{
    api_exec_t* exec = (api_exec_t*)task->data;
    exec->async.callback(exec->async.loop, exec->async.arg);

    return 0;
}

void api_async_post_handler(api_loop_t* loop, struct api_async_t* async,
                            int events)
{
    api_task_t* task;

    /* handle terminate */
    if (events == -1)
    {
        api_free(&async->loop->base.pool, sizeof(*async), async);
    }
    else
    {
        task = api_task_create(&loop->base.scheduler, api_async_task_fn,
                            async->stack_size);
        task->data = async;
        api_task_post(task);
    }
}

void api_async_wakeup_handler(api_loop_t* loop, struct api_async_t* async,
                            int events)
{
    api_task_wakeup((api_task_t*)async->arg);
}

void api_async_exec_completed_handler(api_loop_t* loop,
                                struct api_async_t* async, int events)
{
    api_exec_t* exec = (api_exec_t*)async;

    api_task_wakeup(exec->task);
}

void api_async_exec_handler(api_loop_t* loop, struct api_async_t* async,
                            int events)
{
    api_task_t* task;
    api_exec_t* exec = (api_exec_t*)async;

    /* handle terminate */
    if (events == -1)
    {
        exec->result = API__TERMINATE;
    }
    else
    {
        task = api_task_create(&loop->base.scheduler, api_exec_task_fn,
                                async->stack_size);
        task->data = async;
        api_task_exec(task);
        api_task_delete(task);

        exec->result = API__OK;
    }

    exec->async.handler = api_async_exec_completed_handler;
    api_async_post(exec->loop, 0, 0, 0);
}

void api_async_processor(void* a, int events)
{
    api_loop_t* loop = (api_loop_t*)((char*)a - offsetof(api_loop_t, asyncs));
    eventfd_t value;
    api_async_t* async = 0;
    int error = 0;

    error = eventfd_read(loop->asyncs.fd, &value);
    if (error)
    {
        /* handle error */
    }

    async = (api_async_t*)api_mpscq_pop(&loop->asyncs.queue);
    while (async != 0)
    {
        async->handler(loop, async, events);
        async = (api_async_t*)api_mpscq_pop(&loop->asyncs.queue);
    }
}

int api_async_init(api_loop_t* loop)
{
    int error = 0;

    api_mpscq_create(&loop->asyncs.queue);

    loop->asyncs.fd = eventfd(0, EFD_NONBLOCK);
    if (loop->asyncs.fd == -1)
        return api_error_translate(errno);

    loop->asyncs.e.events = EPOLLIN | EPOLLPRI;
    loop->asyncs.e.data.ptr = &loop->asyncs;
    loop->asyncs.processor = api_async_processor;

    if (-1 == epoll_ctl(loop->epoll, EPOLL_CTL_ADD, loop->asyncs.fd,
                        &loop->asyncs.e))
    {
        error = errno;

        if (api_close(loop->asyncs.fd) != API__OK)
        {
            /* todo: handle error */
        }

        errno = error;
        return api_error_translate(error);
    }

    return API__OK;
}

int api_async_terminate(api_loop_t* loop)
{
    int error = 0;

    if (-1 == epoll_ctl(loop->epoll, EPOLL_CTL_DEL, loop->asyncs.fd,
                &loop->asyncs.e))
    {
        error = errno;
    }
    
    if (api_close(loop->asyncs.fd) != API__OK)
    {
        /* todo: handle error */
    }

    /* notify terminate event */
    loop->asyncs.processor(&loop->asyncs, -1);

    errno = error;
    return api_error_translate(error);
}

int api_async_post(api_loop_t* loop, 
                   api_loop_fn callback, void* arg, size_t stack_size)
{
    api_async_t* async =
        (api_async_t*)api_alloc(&loop->base.pool, sizeof(api_async_t));

    if (async == 0)
    {
        errno = ENOMEM;
        return API__NO_MEMORY;
    }

    async->loop = loop;
    async->handler = api_async_post_handler;
    async->callback = callback;
    async->arg = arg;
    async->stack_size = stack_size;

    api_mpscq_push(&loop->asyncs.queue, &async->node);

    if (-1 == eventfd_write(loop->asyncs.fd, 1))
    {
        return api_error_translate(errno);
    }

    return API__OK;
}

int api_async_wakeup(api_loop_t* loop, api_task_t* task)
{
    api_async_t* async =
        (api_async_t*)api_alloc(&loop->base.pool, sizeof(api_async_t));

    if (async == 0)
    {
        errno = ENOMEM;
        return API__NO_MEMORY;
    }

    async->loop = loop;
    async->handler = api_async_wakeup_handler;
    async->arg = task;

    api_mpscq_push(&loop->asyncs.queue, &async->node);

    if (-1 == eventfd_write(loop->asyncs.fd, 1))
    {
        return api_error_translate(errno);
    }

    return API__OK;
}

int api_async_exec(api_loop_t* current, api_loop_t* loop,
                   api_loop_fn callback, void* arg, size_t stack_size)
{
    api_exec_t exec;

    exec.async.loop = loop;
    exec.async.handler = api_async_exec_handler;
    exec.async.callback = callback;
    exec.async.arg = arg;
    exec.async.stack_size = stack_size;
    exec.loop = current;
    exec.task = current->base.scheduler.current;
    exec.result = 0;

    api_mpscq_push(&loop->asyncs.queue, &exec.async.node);

    if (-1 == eventfd_write(loop->asyncs.fd, 1))
    {
        /* handle error */
    }

    api_task_sleep(current->base.scheduler.current);

    return exec.result;
}