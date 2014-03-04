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

#include "api_loop_base.h"
#include "api_task.h"

typedef struct api_call_t {
    api_loop_t* loop;
    api_loop_fn callback;
    void* arg;
} api_call_t;

void* api_call_task_fn(api_task_t* task)
{
    api_call_t* call = (api_call_t*)task->data;
    call->callback(call->loop, call->arg);

    return 0;
}

uint64_t api_loop_calculate_wait_timeout(api_loop_base_t* loop)
{
    uint64_t timeout = (uint64_t)-1;
    uint64_t timeout_sleep = api_timers_nearest_event(&loop->sleeps, loop->now);
    uint64_t timeout_idle = api_timers_nearest_event(&loop->idles, loop->now);
    uint64_t timeout_timeout = api_timers_nearest_event(&loop->timeouts, loop->now);

    if (timeout_sleep < timeout)
        timeout = timeout_sleep;

    if (timeout_idle < timeout)
        timeout = timeout_idle;

    if (timeout_timeout < timeout)
        timeout = timeout_timeout;

    return timeout;
}

api_pool_t* api_pool_default(api_loop_t* loop)
{
    api_loop_base_t* base = (api_loop_base_t*)loop;

    return &base->pool;
}

int api_loop_sleep(api_loop_t* loop, uint64_t period)
{
    api_loop_base_t* base = (api_loop_base_t*)loop;

    return api_sleep_exec(&base->sleeps, base->scheduler.current, period);
}

int api_loop_idle(api_loop_t* loop, uint64_t period)
{
    api_loop_base_t* base = (api_loop_base_t*)loop;

    return api_idle_exec(&base->idles, base->scheduler.current, period);
}

int api_loop_call(api_loop_t* loop, api_loop_fn callback, void* arg, size_t stack_size)
{
    api_loop_base_t* base = (api_loop_base_t*)loop;
    api_call_t call;
    api_task_t* task;

    call.loop = loop;
    call.callback = callback;
    call.arg = arg;

    task = api_task_create(&base->scheduler, api_call_task_fn, stack_size);
    task->data = &call;
    api_task_exec(task);
    api_task_delete(task);

    return API__OK;
}