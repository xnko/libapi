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

#ifndef API_ASYNC_H_INCLUDED
#define API_ASYNC_H_INCLUDED

#include "api_loop.h"

typedef struct api_async_t {
	api_loop_t* loop;
	api_loop_fn callback;
	void* arg;
	size_t stack_size;
	void (*handler)(struct api_async_t* async);
} api_async_t;

typedef struct api_exec_t {
	api_async_t async;
	api_loop_t* loop;
	api_task_t* task;
	int result;
} api_exec_t;

void api_async_init();
int api_async_post(api_loop_t* loop, 
				   api_loop_fn callback, void* arg, size_t stack_size);
int api_async_wakeup(api_loop_t* loop, api_task_t* task);
int api_async_exec(api_loop_t* current, api_loop_t* loop,
				   api_loop_fn callback, void* arg, size_t stack_size);

#endif // API_ASYNC_H_INCLUDED