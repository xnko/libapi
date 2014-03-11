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

#include "../api_loop_base.h"
#include "api_async.h"
#include "api_wait.h"

typedef struct os_win_t {
    void(*processor)(struct os_win_t* e, DWORD transferred,
                    OVERLAPPED* overlapped, struct api_loop_t* loop, DWORD error);
} os_win_t;

typedef struct api_loop_t {
    api_loop_base_t base;
    HANDLE iocp;
    struct api_wait_t* waiters;
    LARGE_INTEGER frequency;
} api_loop_t;

static uint64_t api_loop_ref(api_loop_t* loop)
{
    return InterlockedIncrement64((volatile LONGLONG*)&loop->base.refs);
}

static uint64_t api_loop_unref(api_loop_t* loop)
{
    uint64_t refs = InterlockedDecrement64((volatile LONGLONG*)&loop->base.refs);

    if (refs == 0)
    {
        free(loop);
    }

    return refs;
}

#endif // API_LOOP_H_INCLUDED