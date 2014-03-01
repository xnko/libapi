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

#ifndef API_LOOP_H_INCLUDED
#define API_LOOP_H_INCLUDED

#include "../api_list.h"
#include "../api_pool.h"
#include "../api_timer.h"
#include "api_async.h"
#include "api_wait.h"

#define API_READ	1
#define API_WRITE	2

typedef struct os_win_t {
	void(*processor)(struct os_win_t* e, DWORD transferred,
					OVERLAPPED* overlapped, struct api_loop_t* loop);
} os_win_t;

typedef struct api_loop_t {
	HANDLE iocp;
	int terminated;
	uint64_t refs;
	struct api_pool_t pool;
	uint64_t now;
	uint64_t last_activity;
	struct api_scheduler_t scheduler;
	struct api_timers_t sleeps;
	struct api_timers_t idles;
	struct api_timers_t timeouts;
	struct api_wait_t* waiters;
} api_loop_t;

static uint64_t api_loop_ref(api_loop_t* loop)
{
	return InterlockedIncrement64((volatile LONGLONG*)&loop->refs);
}

static uint64_t api_loop_unref(api_loop_t* loop)
{
	uint64_t refs = InterlockedDecrement64((volatile LONGLONG*)&loop->refs);

	if (refs == 0)
	{
		free(loop);
	}

	return refs;
}

#endif // API_LOOP_H_INCLUDED