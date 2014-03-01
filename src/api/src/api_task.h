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

#ifndef API_TASK_H_INCLUDED
#define API_TASK_H_INCLUDED

#include "../include/api.h"
#include "api_pool.h"


#if defined(__linux__)

#include <ucontext.h>
typedef ucontext_t api_context_t;

#else

typedef CONTEXT api_context_t;

#endif

typedef struct api_task_t {
	api_context_t	platform;
	struct api_scheduler_t* scheduler;
	struct api_task_t* parent;
	size_t	stack_size;	// stack size
	int		is_done;	// finished
	int		is_post;	// task is posted
	void*	data;		// user data
} api_task_t;

typedef struct api_scheduler_t {
	struct api_task_t*	current;
	struct api_task_t*	prev;
	struct api_task_t	main;
	void*  value;
	api_pool_t* pool;
} api_scheduler_t;

typedef void* (*api_task_fn)(api_task_t* task);

API_EXTERN void api_scheduler_init(api_scheduler_t* scheduler);
API_EXTERN void api_scheduler_destroy(api_scheduler_t* scheduler);

API_EXTERN api_task_t* api_task_create(api_scheduler_t* scheduler,
									  api_task_fn callback, size_t stack_size);
API_EXTERN void api_task_delete(api_task_t* task);
API_EXTERN void api_task_yield(api_task_t* task, void* value);
API_EXTERN api_task_t* api_task_current(api_scheduler_t* scheduler);

API_EXTERN void* api_task_exec(api_task_t* task);
API_EXTERN void  api_task_post(api_task_t* task);

API_EXTERN void api_task_sleep(api_task_t* current);
API_EXTERN void api_task_wakeup(api_task_t* task);

#endif // API_TASK_H_INCLUDED