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

#include <memory.h>

#include "api_timer.h"

void api_timer_add(api_timers_t* timers, api_timer_list_t* list,
                    api_timer_t* timer, api_timer_type_t type)
{
    timer->list = list;

    if (type == TIMER_Sleep || type == TIMER_Timeout)
        timer->issued = api_time_current();

    ++timers->version;
    timer->version = timers->version;

    api_list_push_tail((api_list_t*)&list->head, (api_node_t*)timer);
}

void api_timer_set(api_timers_t* timers, api_timer_t* timer,
                    api_timer_type_t type, uint64_t value)
{
    api_timer_list_t* list;
    api_timer_list_t* temp;

    /* handle is registered */
    if (timer->list != 0)
    {
        /* remove from its list */
        api_list_remove((api_list_t*)&timer->list->head, (api_node_t*)timer);

        /* if reset then move handle to list tail */
        if (timer->list->value == value)
        {
            api_timer_add(timers, timer->list, timer, type);
            return;
        }

        /* remove empty list */
        if (timer->list->head == 0)
        {
            api_list_remove((api_list_t*)&timers->head,
                            (api_node_t*)timer->list);

            api_free(timers->pool, sizeof(*timer->list), timer->list);
        }

        timer->list = 0;
    }

    /* 0 value indicates remove */
    if (value == 0)
        return;

    /* find or create list with matching value */
    list = timers->head;
    if (list == 0 || value < list->value)
    {
        /* create new list and push to timers head */

        list = (api_timer_list_t*)api_alloc(timers->pool, sizeof(*list));
        memset(list, 0, sizeof(*list));
        list->value = value;

        api_list_push_head((api_list_t*)&timers->head, (api_node_t*)list);
    }
    else
    {
        while (list->next)
        {
            if (list->next->value < value)
                list = list->next;
            else
                break;
        }

        if (list->value != value)
        {
            if (list->next)
            {
                if (list->next->value == value)
                {
                    /* found */
                    list = list->next;
                }
                else
                {
                    /* create new list add after list */
                    temp = 
                        (api_timer_list_t*)api_alloc(timers->pool, sizeof(*temp));
                    memset(temp, 0, sizeof(*temp));
                    temp->value = value;

                    temp->prev = list;
                    temp->next = list->next;
                    list->next->prev = temp;
                    list->next = temp;
                    list = temp;
                }
            }
            else
            {
                /* create new list push to timers tail  */

                list =
                    (api_timer_list_t*)api_alloc(timers->pool, sizeof(*list));
                memset(list, 0, sizeof(*list));
                list->value = value;

                api_list_push_tail((api_list_t*)&timers->head, (api_node_t*)list);
            }
        }
    }

    api_timer_add(timers, list, timer, type);
}

int api_sleep_exec(api_timers_t* timers, api_task_t* task, uint64_t value)
{
    api_timer_t timer;

    if (value == 0)
        return API__OK;

    memset(&timer, 0, sizeof(timer));

    timer.task = task;

    api_timer_set(timers, &timer, TIMER_Sleep, value);
    api_task_sleep(task);

    if (timer.elapsed)
        return API__OK;

    return API__TERMINATE;
}

int api_idle_exec(api_timers_t* timers, api_task_t* task, uint64_t value)
{
    api_timer_t timer;

    if (value == 0)
        return API__OK;

    memset(&timer, 0, sizeof(timer));

    timer.task = task;

    api_timer_set(timers, &timer, TIMER_Idle, value);
    api_task_sleep(timer.task);

    if (timer.elapsed)
        return API__OK;

    return API__TERMINATE;
}

int api_timeout_exec(api_timers_t* timers, api_timer_t* timer, uint64_t value)
{
    api_timer_set(timers, timer, TIMER_Timeout, value);

    return API__OK;
}

/*
 * TIMER_sleep   - value = now
 * TIMER_idle    - value = period
 * TIMER_timeout - value = period
 */
int api_timer_process(api_timers_t* timers, api_timer_type_t type, uint64_t value)
{
    api_timer_list_t* list = timers->head;
    api_timer_list_t* next;
    api_timer_t* timer;
    api_timer_t* temp;
    uint64_t version = timers->version;
    int elapsed = 0;
    int count = 0;

    while (list != 0)
    {
        next = list->next;

        timer = list->head;
        while (timer != 0)
        {
            if (timer->version > version)
            {
                /* skip timers set during this call */
                timer = timer->next;
            }
            else
            {
                switch (type) {
                case TIMER_Sleep:
                    elapsed = (value - timer->issued >= list->value);
                    break;
                default:
                    elapsed = (value >= list->value);
                    break;
                }

                if (elapsed)
                {
                    temp = timer->next;

                    /* remove timer from timers and signal */
                    api_list_remove((api_list_t*)&list->head, (api_node_t*)timer);

                    timer->list = 0;

                    timer->elapsed = 1;
                    api_task_wakeup(timer->task);

                    timer = temp;
                    ++count;
                }
                else
                {
                    /* the rest in current list are still pending */
                    break;
                }
            }
        }

        if (list->head == 0)
        {
            /* remove empty list */
            api_list_remove((api_list_t*)&timers->head, (api_node_t*)list);
            api_free(timers->pool, sizeof(*list), list);
        }

        list = next;
    }

    return count;
}

void api_timer_terminate(api_timers_t* timers)
{
    api_timer_list_t* list = timers->head;
    api_timer_list_t* next;
    api_timer_t* timer;
    api_timer_t* temp;

    while (list != 0)
    {
        next = list->next;

        timer = list->head;
        while (timer != 0)
        {
            temp = timer->next;

            /* remove timer from timers and signal */
            api_list_remove((api_list_t*)&list->head, (api_node_t*)timer);

            timer->list = 0;
            api_task_wakeup(timer->task);

            timer = temp;
        }

        if (list->head == 0)
        {
            /* remove empty list */
            api_list_remove((api_list_t*)&timers->head, (api_node_t*)list);
            api_free(timers->pool, sizeof(*list), list);
        }

        list = next;
    }
}