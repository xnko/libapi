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

#ifndef API_MPSCQ_H_INCLUDED
#define API_MPSCQ_H_INCLUDED

typedef struct api_mpscq_node_t {
    struct api_mpscq_node_t* volatile next;
} api_mpscq_node_t;

typedef struct api_mpscq_t {
    api_mpscq_node_t* volatile  head;
    api_mpscq_node_t*           tail;
    api_mpscq_node_t            stub;
} api_mpscq_t;

static void api_mpscq_create(api_mpscq_t* self)
{
    self->head = &self->stub;
    self->tail = &self->stub;
    self->stub.next = 0;
}

static void api_mpscq_push(api_mpscq_t* self, api_mpscq_node_t* n)
{
    api_mpscq_node_t* prev;

    n->next = 0;
    prev = __sync_lock_test_and_set(&self->head, n);
    prev->next = n;
}

static api_mpscq_node_t* api_mpscq_pop(api_mpscq_t* self)
{
    api_mpscq_node_t* tail = self->tail;
    api_mpscq_node_t* next = tail->next;

    if (tail == &self->stub)
    {
        if (0 == next)
            return 0;
        self->tail = next;
        tail = next;
        next = next->next;
    }

    if (next)
    {
        self->tail = next;
        return tail;
    }

    api_mpscq_node_t* head = self->head;
    if (tail != head)
        return 0;

    api_mpscq_push(self, &self->stub);
    next = tail->next;
    if (next)
    {
        self->tail = next;
        return tail;
    }

    return 0;
}

#endif // API_MPSCQ_H_INCLUDED