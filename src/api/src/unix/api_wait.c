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

#include "api_wait.h"

void api_wait_handler(api_loop_t* loop, api_wait_t* wait, int events)
{
    api_task_wakeup(wait->task);
}

void api_wait_init(api_loop_t* loop)
{
    api_mpscq_create(&loop->waiters);
}

void api_wait_exec(api_loop_t* current, api_loop_t* loop, int sleep)
{
    api_wait_t wait;

    wait.from = current;
    wait.to = loop;
    wait.task = current->scheduler.current;

    api_mpscq_push(&loop->waiters, &wait.node);

    if (sleep)
        api_task_sleep(current->scheduler.current);
}

void api_wait_notify(api_loop_t* loop)
{
    api_wait_t* wait = 0;
    int error = 0;

    wait = (api_wait_t*)api_mpscq_pop(&loop->waiters);
    while (wait != 0)
    {
        wait->handler = api_wait_handler;
        api_mpscq_push(&wait->from->asyncs.queue, &wait->node);
        if (-1 == eventfd_write(wait->from->asyncs.fd, 1))
        {
            /* handle error */
        }

        wait = (api_wait_t*)api_mpscq_pop(&loop->waiters);
    }
}