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
#include "api_async.h"

static struct os_win_t g_api_async_processor;

void* api_async_task_fn(api_task_t* task)
{
	api_async_t* async = (api_async_t*)task->data;
	async->callback(async->loop, async->arg);
	api_free(&async->loop->pool, sizeof(*async), async);

	return 0;
}

void* api_exec_task_fn(api_task_t* task)
{
	api_exec_t* exec = (api_exec_t*)task->data;
	exec->async.callback(exec->async.loop, exec->async.arg);

	return 0;
}

void api_async_processor(struct os_win_t* e, DWORD transferred,
						 OVERLAPPED* overlapped, api_loop_t* loop)
{
	api_async_t* async = (api_async_t*)overlapped;

	async->handler(async);
}

void api_async_post_handler(struct api_async_t* async)
{
	api_task_t* task;

	task = api_task_create(&async->loop->scheduler, api_async_task_fn,
							async->stack_size);
	task->data = async;
	api_task_post(task);
}

void api_async_wakeup_handler(struct api_async_t* async)
{
	api_task_wakeup((api_task_t*)async->arg);
}

void api_async_exec_completed_handler(struct api_async_t* async)
{
	api_exec_t* exec = (api_exec_t*)async;

	api_task_wakeup(exec->task);
}

void api_async_exec_handler(struct api_async_t* async)
{
	api_task_t* task;
	api_exec_t* exec = (api_exec_t*)async;

	task = api_task_create(&async->loop->scheduler, api_exec_task_fn,
							async->stack_size);
	task->data = async;
	api_task_exec(task);
	api_task_delete(task);

	exec->result = API__OK;

	exec->async.handler = api_async_exec_completed_handler;
	api_async_post(exec->loop, 0, 0, 0);
}

void api_async_init()
{
	g_api_async_processor.processor = api_async_processor;
}

int api_async_post(api_loop_t* loop, 
				   api_loop_fn callback, void* arg, size_t stack_size)
{
	api_async_t* async = 
		(api_async_t*)api_alloc(&loop->pool, sizeof(api_async_t));
	int error = 0;

	if (async == 0)
		return API__NO_MEMORY;

	async->loop = loop;
	async->callback = callback;
	async->arg = arg;
	async->stack_size = stack_size;
	async->handler = api_async_post_handler;

	if (!PostQueuedCompletionStatus(loop->iocp, sizeof(*async),
					(ULONG_PTR)&g_api_async_processor, (LPOVERLAPPED)async))
	{
		error = api_error_translate(GetLastError());
		api_free(&loop->pool, sizeof(api_async_t), async);
		return error;
	}

	return API__OK;
}

int api_async_wakeup(api_loop_t* loop, api_task_t* task)
{
	api_async_t* async =
		(api_async_t*)api_alloc(&loop->pool, sizeof(api_async_t));
	int error = 0;

	if (async == 0)
		return API__NO_MEMORY;

	async->loop = loop;
	async->arg = task;
	async->stack_size = 0;
	async->handler = api_async_wakeup_handler;

	if (!PostQueuedCompletionStatus(loop->iocp, sizeof(*async),
				(ULONG_PTR)&g_api_async_processor, (LPOVERLAPPED)async))
	{
		error = api_error_translate(GetLastError());
		api_free(&loop->pool, sizeof(api_async_t), async);
		return error;
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
	exec.task = current->scheduler.current;
	exec.result = 0;

	if (!PostQueuedCompletionStatus(loop->iocp, sizeof(exec),
			(ULONG_PTR)&g_api_async_processor, (LPOVERLAPPED)&exec))
	{
		exec.result = api_error_translate(GetLastError());
		return exec.result;
	}

	api_task_sleep(current->scheduler.current);

	return exec.result;
}