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

#include <pthread.h>
#include <memory.h>
#include <malloc.h>
#include <errno.h>

#include "api_error.h"
#include "api_misc.h"
#include "api_loop.h"
#include "api_async.h"
#include "api_wait.h"
#include "api_stream.h"

#define API_MAX_EVENTS 60

typedef struct os_linux_t {
    void(*processor)(struct os_linux_t* e, int events);
    struct epoll_event e;
} os_linux_t;

int api_loop_init(api_loop_t* loop)
{
    api_pool_init(&loop->base.pool);
    api_mpscq_create(&loop->asyncs.queue);
    loop->base.sleeps.pool = &loop->base.pool;
    loop->base.idles.pool = &loop->base.pool;
    loop->base.timeouts.pool = &loop->base.pool;
    api_wait_init(loop);
    return api_async_init(loop);
}

int api_loop_cleanup(api_loop_t* loop)
{
    api_timer_terminate(&loop->base.idles);
    api_timer_terminate(&loop->base.sleeps);
    api_timer_terminate(&loop->base.timeouts);
    api_wait_notify(loop);
    api_scheduler_destroy(&loop->base.scheduler);
    api_pool_cleanup(&loop->base.pool);
    return api_async_terminate(loop);
}

int api_loop_run_internal(api_loop_t* loop)
{
    struct epoll_event events[API_MAX_EVENTS];
    os_linux_t* os_linux;
    int n, i;

    api_scheduler_init(&loop->base.scheduler);
    loop->base.scheduler.pool = &loop->base.pool;

    memset(events, 0, sizeof(struct epoll_event) * API_MAX_EVENTS);
    loop->base.now = api_time_current();
    loop->base.last_activity = loop->base.now;

    api_loop_ref(loop);

    do
    {
        if (0 < api_timer_process(&loop->base.sleeps, TIMER_Sleep, loop->base.now))
        {
            loop->base.now = api_time_current();
            loop->base.last_activity = loop->base.now;
        }

        n = epoll_wait(loop->epoll, events, API_MAX_EVENTS,
            (int)api_loop_calculate_wait_timeout(&loop->base));

        loop->base.now = api_time_current();

        if (n == -1)
        {
            if (errno == EBADF || errno == EINVAL)
                break;

            // EINTR
            continue;
        }

        if (n > 0)
        {
            for (i = 0; i < n; ++i)
            {
                os_linux = (os_linux_t*)events[i].data.ptr;
                os_linux->processor(os_linux, events[i].events);
                loop->base.now = api_time_current();
                loop->base.last_activity = loop->base.now;
            }
        }
        else
        {
            if (0 < api_timer_process(&loop->base.idles, TIMER_Idle,
                            loop->base.now - loop->base.last_activity))
            {
                loop->base.now = api_time_current();
                loop->base.last_activity = loop->base.now;
            }
        }

        api_timer_process(&loop->base.timeouts, TIMER_Timeout,
                loop->base.now - loop->base.last_activity);
    }
    while (1);

    if (api_loop_cleanup(loop) != API__OK)
    {
        /* handle error */
    }

    if (api_close(loop->epoll) != API__OK)
    {
        /* handle error when eopll not closed already */
    }

    return API__OK;
}

static void* loop_thread_start(void* arg)
{
    api_loop_t* loop = (api_loop_t*)arg;

    api_loop_run_internal(loop);

    free(loop);

    return 0;
}

int api_loop_start(api_loop_t** loop)
{
    pthread_t thread;

    *loop = (api_loop_t*)malloc(sizeof(api_loop_t));
    int error = API__OK;
    int sys_error = 0;

    if (*loop == 0)
    {
        errno = ENOMEM;
        return API__NO_MEMORY;
    }
    
    memset(*loop, 0, sizeof(api_loop_t));

    (*loop)->epoll = epoll_create1(0);
    if ((*loop)->epoll == -1)
    {
        sys_error = errno;
        free(*loop);
        errno = sys_error;
        *loop = 0;

        return api_error_translate(sys_error);
    }

    error = api_loop_init(*loop);
    if (error != API__OK)
    {
        sys_error = errno;

        if (api_close((*loop)->epoll) != API__OK)
        {
            /* handle error */
        }

        free(*loop);
        errno = sys_error;
        *loop = 0;

        return error;
    }

    if (pthread_create(&thread, 0, loop_thread_start, *loop))
    {
        sys_error = errno;
        if (api_loop_cleanup(*loop) != API__OK)
        {
            /* handle error */
        }

        if (api_close((*loop)->epoll) != API__OK)
        {
            /* handle error */
        }

        free(*loop);
        errno = sys_error;
        *loop = 0;

        error = api_error_translate(sys_error);
    }

    return error;
}

int api_loop_stop(api_loop_t* loop)
{
    return api_close(loop->epoll);
}

int api_loop_stop_and_wait(api_loop_t* current, api_loop_t* loop)
{
    int error = 0;

    api_wait_exec(current, loop, 0);

    error = api_close(loop->epoll);

    api_task_sleep(current->base.scheduler.current);

    return error;
}

int api_loop_wait(api_loop_t* current, api_loop_t* loop)
{
    api_wait_exec(current, loop, 1);

    return API__OK;
}

int api_loop_post(api_loop_t* loop, api_loop_fn callback, void* arg,
                  size_t stack_size)
{
    return api_async_post(loop, callback, arg, stack_size);
}

int api_loop_exec(api_loop_t* current, api_loop_t* loop,
                  api_loop_fn callback, void* arg, size_t stack_size)
{
    return api_async_exec(current, loop, callback, arg, stack_size);
}

int api_loop_run(api_loop_fn callback, void* arg, size_t stack_size)
{
    api_loop_t loop;
    int error;
    int sys_error;

    memset(&loop, 0, sizeof(loop));

    loop.epoll = epoll_create1(0);
    if (loop.epoll == -1)
    {
        return api_error_translate(errno);
    }

    error = api_loop_init(&loop);
    if (error < 0)
    {
        sys_error = errno;
    
        if (api_close(loop.epoll) != API__OK)
        {
            /* handle error */
        }

        errno = sys_error;
        return error;
    }

    if (callback != 0)
    {
        error = api_loop_post(&loop, callback, arg, 0);

        if (API__OK != error)
        {
            sys_error = errno;
            if (api_close(loop.epoll) != API__OK)
            {
                /* handle error */
            }

            errno = sys_error;
            return error;
        }
    }

    return api_loop_run_internal(&loop);
}
