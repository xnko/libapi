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

#include "api_task.h"

#if defined(__linux__)

#define api_task_swapcontext_native(current, other) swapcontext(current, other)

#else

#if defined(_WIN64)
const int offset_eip = (int)(&((CONTEXT*)0)->Rip);
const int offset_esp = (int)(&((CONTEXT*)0)->Rsp);
#else
const int offset_eip = (int)(&((CONTEXT*)0)->Eip);
const int offset_esp = (int)(&((CONTEXT*)0)->Esp);
#endif

__declspec(noinline) void __stdcall api_task_getcontext(api_task_t* task)
{
    RtlCaptureContext(&task->platform);
}

__declspec(noinline) void __stdcall api_task_setcontext(api_task_t* task)
{
    SetThreadContext(GetCurrentThread(), &task->platform);
}

#if defined(_WIN64)

extern void __stdcall api_task_swapcontext_native(CONTEXT* oucp, CONTEXT* ucp);

#else

void __stdcall api_task_swapcontext_native(CONTEXT* oucp, CONTEXT* ucp)
{
    __asm {

        push [oucp]
        call api_task_getcontext

        // correct eip
        mov eax, [oucp]
        add eax, offset_eip
        mov edx, offset done
        mov [eax], edx

        // correct esp
        mov eax, [oucp]
        add eax, offset_esp
        mov [eax], esp

        push [ucp]
        call api_task_setcontext
done:
    }
}

#endif

#endif

#if !defined(__linux__)
#pragma optimize( "", off)
#endif
void api_task_swapcontext(api_task_t* current, api_task_t* other)
{
    api_scheduler_t* scheduler = current->scheduler;
	
    /* save/restore system error codes across task switches */

    int error = errno;
#if !defined(__linux__)
    DWORD win_error = GetLastError();
#endif

    scheduler->prev = current;
    scheduler->current = other;
    api_task_swapcontext_native(&current->platform, &other->platform);
    scheduler->current = current;

    if (scheduler->prev != 0 &&
        scheduler->prev->is_post &&
        scheduler->prev->is_done)
    {
        // if prev was forked and done then delete it

        api_task_delete(scheduler->prev);
        scheduler->prev = 0;
    }

    errno = error;
#if !defined(__linux__)
    SetLastError(win_error);
#endif
}
#if !defined(__linux__)
#pragma optimize( "", on)
#endif

#if defined(__linux__)

static void api_task_entry_point(api_task_t* task, api_task_fn callback)
{
    callback(task);
    task->is_done = 1;

    api_task_swapcontext(task, task->parent);
}

#define api_task_makecontext(ctx, fun, args, ...) \
        makecontext(ctx, fun, args, __VA_ARGS__)

#else

void api_task_defer(api_task_t* task)
{
    task->is_done = 1;
    task->scheduler->prev = task;
    api_task_setcontext(task->parent);
}

void api_task_makecontext(api_task_t* task, api_task_fn callback)
{
    size_t* sp = (size_t*)((char*)(task + 1) + task->stack_size);

    *(sp - 1) = (size_t)task;
    *(sp - 2) = (size_t)task;
    *(sp - 3) = (size_t)api_task_defer;

#if defined(_WIN64)
    task->platform.Rip = (size_t)callback;
    task->platform.Rsp = (size_t)(sp - 3);
#else
    task->platform.Eip = (size_t)callback;
    task->platform.Esp = (size_t)(sp - 3);
#endif
}

#endif

void api_scheduler_init(api_scheduler_t* scheduler)
{
    scheduler->current = &scheduler->main;
    scheduler->main.scheduler = scheduler;
    scheduler->prev = 0;
}

void api_scheduler_destroy(api_scheduler_t* scheduler)
{
}

api_task_t* api_task_create(api_scheduler_t* scheduler, 
                            api_task_fn callback, size_t stack_size)
{
    api_task_t* task;

    if (stack_size == 0)
        stack_size = 8 * 1024;

    stack_size -= sizeof(*task);

    task = (api_task_t*)api_alloc(scheduler->pool, sizeof(*task) + stack_size);

    task->data = 0;
    task->is_done = 0;
    task->is_post = 0;
    task->parent = 0;
    task->stack_size = stack_size;
    task->scheduler = scheduler;

#if defined(__linux__)

    getcontext(&task->platform);

    task->platform.uc_stack.ss_sp = task + 1;
    task->platform.uc_stack.ss_size = stack_size;
    task->platform.uc_stack.ss_flags = 0;
    task->platform.uc_link = 0;

    api_task_makecontext(&task->platform, (void (*)())api_task_entry_point,
                        2, task, callback);

#else

    task->platform.ContextFlags = CONTEXT_ALL;

    api_task_getcontext(task);
    api_task_makecontext(task, callback);

#endif

    return task;
}

void api_task_delete(api_task_t* task)
{
    /* dont delete yourself */
    if (task->scheduler->current != task)
        api_free(task->scheduler->pool,
                sizeof(*task) + task->stack_size, task);
}

void api_task_yield(api_task_t* current, void* value)
{
    current->scheduler->value = value;

    /* main task cannot yield */
    if (current != &current->scheduler->main)
        api_task_swapcontext(current, current->parent);
}

api_task_t* api_task_current(api_scheduler_t* scheduler)
{
    return scheduler->current;
}

void* api_task_exec(api_task_t* task)
{
    if (task->is_done)
        return 0;

    /* main task not executable */
    if (task == &task->scheduler->main)
        return 0;

    task->parent = task->scheduler->current;
    task->is_post = 0;
	
    api_task_swapcontext(task->scheduler->current, task);

    return task->scheduler->value;
}

void api_task_post(api_task_t* task)
{
    // reset all ???
    if (task->is_done)
        return;

    /* main task not postable */
    if (task == &task->scheduler->main)
        return;

    task->parent = &task->scheduler->main;
    task->is_post = 1;
	
    api_task_swapcontext(task->scheduler->current, task);
}

void api_task_sleep(api_task_t* current)
{
    api_task_swapcontext(current, &current->scheduler->main);
}

void api_task_wakeup(api_task_t* task)
{
    api_task_swapcontext(task->scheduler->current, task);
}