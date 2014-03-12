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

#include "api_loop_base.h"
#include "api_list.h"
#include "api_task.h"
#include "api_stream_common.h"

/* used in api_stream_transfer */
typedef struct api_buffer_t {
    struct api_buffer_t* next;
    struct api_buffer_t* prev;
    char*   buffer;
    size_t  used;
} api_buffer_t;

/* data for api_stream_transfer */
typedef struct api_transfer_t {
    api_stream_t* src;
    api_list_t buffers;
    api_task_t* writer;
    size_t chunk_size;
    int read_done;
    int write_done;
    int num_wakeup_req;
    int num_wakeup_done;
} api_transfer_t;

extern int api_async_wakeup(api_loop_t* loop, api_task_t* task);

size_t api_filter_on_read(api_filter_t* filter, char* buffer, size_t length)
{
    return filter->next->on_read(filter->next, buffer, length);
}

size_t api_filter_on_write(api_filter_t* filter, const char* buffer, size_t length)
{
    return filter->next->on_write(filter->next, buffer, length);
}

void api_filter_on_read_timeout(api_filter_t* filter)
{
    filter->next->on_read_timeout(filter->next);
}

void api_filter_on_write_timeout(api_filter_t* filter)
{
    filter->next->on_write_timeout(filter->next);
}

void api_filter_on_error(api_filter_t* filter, int code)
{
    filter->next->on_error(filter->next, code);
}

void api_filter_on_peerclosed(api_filter_t* filter)
{
    filter->next->on_peerclosed(filter->next);
}

void api_filter_on_closed(api_filter_t* filter)
{
    filter->next->on_closed(filter->next);
}

void api_filter_on_terminate(api_filter_t* filter)
{
    filter->next->on_terminate(filter->next);
}

size_t api_stream_read_exact(api_stream_t* stream, char* buffer, size_t length)
{
    size_t offset = 0;
    size_t done = 0;

    while (offset < length)
    {
        done = api_stream_read(stream, buffer + offset, length - offset);
        offset += done;

        if (done == 0)
            break;
    }

    return offset;
}

size_t api_stream_unread(api_stream_t* stream, const char* buffer, size_t length)
{
    if (length == 0)
        return length;

    if (stream->unread.length > 0)
        api_free(api_pool_default(stream->loop), stream->unread.length, stream->unread.buffer);

    stream->unread.buffer = 
        (char*)api_alloc(api_pool_default(stream->loop), length);
    stream->unread.length = length;
    stream->unread.offset = 0;

    memcpy(stream->unread.buffer, buffer, length);

    return length;
}

void api_transfer_reader(api_loop_t* loop, void* arg)
{
    api_transfer_t* transfer = (api_transfer_t*)arg;
    api_pool_t* pool = api_pool_default(transfer->src->loop);
    api_buffer_t* buffer;
    size_t nread;

    while (1)
    {
        buffer = (api_buffer_t*)api_alloc(pool, sizeof(*buffer));
        buffer->buffer = (char*)api_alloc(pool, transfer->chunk_size);

        nread = api_stream_read(transfer->src, buffer->buffer,
                                transfer->chunk_size);

        if (nread == 0)
        {
            api_free(pool, transfer->chunk_size, buffer->buffer);
            api_free(pool, sizeof(*buffer), buffer);

            transfer->read_done = 1;
            break;
        }
        else
        {
            buffer->used = nread;
            api_list_push_tail(&transfer->buffers, (api_node_t*)buffer);
        }

        if (transfer->write_done)
            break;

        if (transfer->num_wakeup_done == transfer->num_wakeup_req)
        {
            api_async_wakeup(transfer->src->loop, transfer->writer);
            ++transfer->num_wakeup_req;
        }
    }

    if (transfer->num_wakeup_done == transfer->num_wakeup_req)
    {
        api_async_wakeup(transfer->src->loop, transfer->writer);
        ++transfer->num_wakeup_req;
    }
}

int api_stream_transfer(api_stream_t* dst,
                           api_stream_t* src,
                           size_t chunk_size,
                           size_t* transferred)
{
    api_loop_base_t* base = (api_loop_base_t*)src->loop;
    api_transfer_t transfer;
    size_t total = 0;
    size_t wrote = 0;
    size_t used;
    int error;
    int failed = 0;
    api_buffer_t* buffer;
    api_pool_t* pool = api_pool_default(src->loop);

    memset(&transfer, 0, sizeof(transfer));

    transfer.src = src;
    transfer.writer = base->scheduler.current;
    transfer.chunk_size = chunk_size;

    error = api_loop_post(src->loop, api_transfer_reader, &transfer, 0);
    if (API__OK != error)
        return error;

    while (1)
    {
        api_task_sleep(transfer.writer);

        ++transfer.num_wakeup_done;

        buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
        while (buffer != 0)
        {
            used = buffer->used;

            wrote = api_stream_write(dst, buffer->buffer, used);

            api_free(pool, chunk_size, buffer->buffer);
            api_free(pool, sizeof(*buffer), buffer);

            if (used == wrote)
            {
                buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
            }
            else
            {
                failed = 1;
                transfer.write_done = 1;
                break;
            }
        }

        if (failed)
            break;

        total += wrote;
        if (transfer.read_done == 1 && transfer.buffers.head == 0)
            break;
    }

    buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
    while (buffer != 0)
    {
        api_free(pool, chunk_size, buffer->buffer);
        api_free(pool, sizeof(*buffer), buffer);

        buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
    }

    if (transferred != 0)
        *transferred = total;

    return API__OK;
}

void api_filter_attach(api_filter_t* filter, api_stream_t* stream)
{
    filter->stream = stream;
    filter->on_closed = api_filter_on_closed;
    filter->on_error = api_filter_on_error;
    filter->on_peerclosed = api_filter_on_peerclosed;
    filter->on_read = api_filter_on_read;
    filter->on_terminate = api_filter_on_terminate;
    filter->on_write = api_filter_on_write;
    filter->on_read_timeout = api_filter_on_read_timeout;
    filter->on_write_timeout = api_filter_on_write_timeout;

    api_list_push_head((api_list_t*)&stream->filter_head, (api_node_t*)filter);
}

void api_filter_detach(api_filter_t* filter, api_stream_t* stream)
{
    api_list_remove((api_list_t*)&stream->filter_head, (api_node_t*)filter);
    filter->stream = 0;
}