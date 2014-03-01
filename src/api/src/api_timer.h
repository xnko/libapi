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

/*
 * Timer requests stored as follows
 *
 * api_timers_t structure stores list of api_timer_list_t by each
 * value in ascending order of value.
 * Each api_timer_list_t stores list of api_timer_t in order of
 * first issued time
 *
 *
 * Algorithm:
 *
 * assume	TS = api_timers_t
 *			TL = api_timer_list_t
 *			TR = api_timer_t
 *
 * set(TS, value) complexity = O(TS.length)
 *
 *		TL = null
 *		if (value < TS.head.value) {
 *			TL = create new
 *			insert TL into TS.head
 *		} else {
 *			TL = TS.head;
 *			while (TL != null && value < TL.value)
 *				TL = TL.next;
 *		}
 *
 *		if (TL == null) {
 *			TL = create new
 *			insert TL into TS.tail
 *		}
 *	
 *		insert TR into TL.tail
 *
 *
 * reset(TR) complexity = O(1)
 *
 *		remove TR from TR.list and
 *		insert into TR.list.tail
 *
 *
 * remove(TR) complexity = O(1)
 *
 *		remove TR from TR.list
 *
 *
 * process(TS) complexity = O(count of fired TR)
 *
 *		iterate over TL
 *			iterate over TR
 *				if TR elapsed then fire
 *				else exit process
 *
 *
 * For little amount of various timer values (not amount of timer requests)
 * this is better case for performance. For example an application doing any
 * number of timer requests but in values of 10 sec, 20 sec 1 min and 1 hour.
 */

#ifndef API_TIMER_H_INCLUDED
#define API_TIMER_H_INCLUDED

#include "../include/api.h"
#include "api_task.h"

typedef enum api_timer_type_t {
	TIMER_Sleep,
	TIMER_Idle,
	TIMER_Timeout
} api_timer_type_t;

typedef struct api_timer_t {
	struct api_timer_t* next;
	struct api_timer_t* prev;
	struct api_timer_list_t* list;
	api_task_t* task;
	uint64_t issued;
	uint64_t version;
	int elapsed;
} api_timer_t;

typedef struct api_timer_list_t {
	struct api_timer_list_t* next;
	struct api_timer_list_t* prev;
	api_timer_t* head;
	api_timer_t* tail;
	uint64_t value;
} api_timer_list_t;

typedef struct api_timers_t {
	api_timer_list_t* head;
	api_timer_list_t* tail;
	api_pool_t* pool;
	uint64_t version;
} api_timers_t;

void api_timer_set(api_timers_t* timers, api_timer_t* timer,
				   api_timer_type_t type, uint64_t value);
int api_sleep_exec(api_timers_t* timers, api_task_t* task, uint64_t value);
int api_idle_exec(api_timers_t* timers, api_task_t* task, uint64_t value);
int api_timeout_exec(api_timers_t* timers, api_timer_t* timer, uint64_t value);

/*
 * TIMER_sleep   - value = now
 * TIMER_idle    - value = period
 * TIMER_timeout - value = period
 */
int api_timer_process(api_timers_t* timers, api_timer_type_t type,
					   uint64_t value);
void api_timer_terminate(api_timers_t* timers);

#endif // API_TIMER_H_INCLUDED