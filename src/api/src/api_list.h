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

#ifndef API_LIST_H_INCLUDED
#define API_LIST_H_INCLUDED

typedef struct api_node_t {
    struct api_node_t* next;
    struct api_node_t* prev;
} api_node_t;

typedef struct api_list_t {
    struct api_node_t* head;
    struct api_node_t* tail;
} api_list_t;

static void api_list_push_head(api_list_t* list, api_node_t* node)
{
    node->prev = 0;
    node->next = 0;

    if (list->head == 0)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
}

static void api_list_push_tail(api_list_t* list, api_node_t* node)
{
    node->prev = 0;
    node->next = 0;

    if (list->tail == 0)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
}

static api_node_t* api_list_pop_head(api_list_t* list)
{
    api_node_t* node = 0;

    if (list->head == 0)
        return 0;

    node = list->head;

    list->head = list->head->next;

    if (list->tail == node)
        list->tail = 0;
    else
        list->head->prev = 0;

    return node;
}

static api_node_t* api_list_pop_tail(api_list_t* list)
{
    api_node_t* node = 0;

    if (list->tail == 0)
        return 0;

    node = list->tail;

    list->tail = list->tail->prev;

    if (list->head == node)
        list->head = 0;
    else
        list->tail->next = 0;

    return node;
}

static api_node_t* api_list_remove(api_list_t* list, api_node_t* node)
{
    if (node == list->head)
        return api_list_pop_head(list);

    if (node == list->tail)
        return api_list_pop_tail(list);

    node->prev->next = node->next;
    node->next->prev = node->prev;

    return node;
}

#endif // API_LIST_H_INCLUDED