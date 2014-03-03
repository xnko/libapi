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
#include "api_wait.h"

static struct os_win_t g_api_wait_processor;
static struct os_win_t g_api_wait_notifier;

void api_wait_processor(struct os_win_t* e, DWORD transferred,
                        OVERLAPPED* overlapped, struct api_loop_t* loop)
{
    api_wait_t* wait = (api_wait_t*)overlapped;

    wait->next = loop->waiters;
    loop->waiters = wait;
}

void api_wait_notifier(struct os_win_t* e, DWORD transferred,
                       OVERLAPPED* overlapped, struct api_loop_t* loop)
{
    api_wait_t* wait = (api_wait_t*)overlapped;
    api_task_wakeup(wait->task);
}


void api_wait_init()
{
    g_api_wait_processor.processor = api_wait_processor;
    g_api_wait_notifier.processor = api_wait_notifier;
}

int api_wait_exec(struct api_loop_t* current,
                struct api_loop_t* loop, int sleep)
{
    api_wait_t wait;

    wait.from = current;
    wait.task = current->scheduler.current;

    if (!PostQueuedCompletionStatus(loop->iocp, 0, 
        (ULONG_PTR)&g_api_wait_processor, (LPOVERLAPPED)&wait))
    {
        return api_error_translate(GetLastError());
    }

    if (sleep)
        api_task_sleep(current->scheduler.current);

    return API__OK;
}

void api_wait_notify(struct api_loop_t* loop)
{
    api_wait_t* wait = 0;
    api_wait_t* next = 0;
    int error = 0;

    wait = loop->waiters;
    while (wait != 0)
    {
        next = wait->next;

        if (!PostQueuedCompletionStatus(wait->from->iocp, 0,
            (ULONG_PTR)&g_api_wait_notifier, (LPOVERLAPPED)wait))
        {
            //return api_error_translate(GetLastError());
        }

        wait = next;
    }
}