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

#include <inttypes.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <memory.h>

#include "../../include/api.h"
#include "api_error.h"
#include "api_loop.h"

int api_event_init(api_event_t* ev, api_loop_t* loop)
{
    memset(ev, 0, sizeof(*ev));

    ev->loop = loop;

    return API__OK;
}

int api_event_signal(api_event_t* ev, api_loop_t* loop)
{
    api_task_t* task;

    ++ev->value;

    if (ev->reserved != 0)
    {
        task = (api_task_t*)ev->reserved;
        ev->reserved = 0;

        api_task_wakeup(task);
    }

    return API__OK;
}

int api_event_wait(api_event_t* ev, uint64_t timeout)
{
    api_timer_t timer;

    if (ev->value == 0)
    {
        if (timeout > 0)
        {
            memset(&timer, 0, sizeof(timer));
            timer.task = ev->loop->base.scheduler.current;

            api_timeout_exec(&ev->loop->base.timeouts, &timer, timeout);
        }

        ev->reserved = ev->loop->base.scheduler.current;
        api_task_sleep(ev->loop->base.scheduler.current);
        ev->loop->base.scheduler.current = 0;

        if (timeout > 0)
            api_timeout_exec(&ev->loop->base.timeouts, &timer, 0);

        if (timeout > 0 && timer.elapsed)
            return api_error_translate(ETIMEDOUT);
    }

    return API__OK;
}