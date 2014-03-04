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

#include <aio.h>
#include <errno.h>
#include <unistd.h>
#include <memory.h>

#include "../../include/api.h"
#include "api_error.h"
#include "api_stream.h"
#include "api_async.h"

typedef struct api_stream_read_t {
    char* buffer;
    uint64_t length;
    uint64_t done;
    api_task_t* task;	
} api_stream_read_t;

typedef struct api_stream_write_t {
    const char* buffer;
    uint64_t length;
    uint64_t offset;
    api_task_t* task;
} api_stream_write_t;

typedef struct api_stream_file_read_t {
    struct aiocb aio;
    uint64_t done;
    api_task_t* task;
    api_loop_t* loop;
    int error;
} api_stream_file_read_t;

typedef struct api_stream_file_write_t {
    struct aiocb aio;
    uint64_t done;
    api_task_t* task;
    api_loop_t* loop;
    int error;
} api_stream_file_write_t;

/* used in api_stream_transfer */
typedef struct api_buffer_t {
    struct api_buffer_t* next;
    struct api_buffer_t* prev;
    char*	buffer;
    size_t	used;
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

void api_stream_read_try(struct api_stream_t* stream)
{
    api_stream_read_t* data = (api_stream_read_t*)stream->os_linux.reserved[0];
    ssize_t n;

    n = read(stream->fd, data->buffer, data->length);
    if (n == -1)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            stream->status.error = api_error_translate(errno);
            stream->filter_head->on_error(stream->filter_head, 
                                        stream->status.error);
        }
    }
    else if (n == 0)
    {
        stream->status.eof = 1;
    }
    else
    {
        data->done = n;
    }
}

void api_stream_write_try(struct api_stream_t* stream)
{
    api_stream_write_t* data =
        (api_stream_write_t*)stream->os_linux.reserved[1];
    ssize_t n;

    n = write(stream->fd, data->buffer + data->offset,
            data->length - data->offset);
    if (n > 0)
    {
        data->offset += n;
    }
    else
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            stream->status.error = api_error_translate(errno);
            stream->filter_head->on_error(stream->filter_head, 
                                        stream->status.error);
        }
    }
}

size_t api_stream_on_read(struct api_filter_t* filter, 
                          char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_read_t read;
    api_timer_t timeout;
    uint64_t now = stream->loop->now;
    uint64_t timeout_value = stream->read_timeout;

    if (length == 0)
        return length;

    if (stream->status.read_timeout ||
        stream->status.eof ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    read.buffer = buffer;
    read.length = length;
    read.done = 0;
    read.task = stream->loop->scheduler.current;

    stream->os_linux.reserved[0] = &read;

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = read.task;

        api_timeout_exec(&stream->loop->timeouts, &timeout, timeout_value);
    }

    api_loop_read_add(stream->loop, stream->fd, &stream->os_linux.e);

    api_task_sleep(read.task);

    api_loop_read_del(stream->loop, stream->fd, &stream->os_linux.e);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->timeouts, &timeout, 0);

    stream->os_linux.reserved[0] = 0;
    stream->read_bandwidth.read += read.done;
    stream->read_bandwidth.period += (stream->loop->now - now);

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.read_timeout = 1;
        stream->filter_head->on_read_timeout(stream->filter_head);

        return 0;
    }
    else
    {
        return read.done;
    }
}

size_t api_stream_on_write(struct api_filter_t* filter,
                           const char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_write_t write;
    api_timer_t timeout;
    uint64_t now = stream->loop->now;
    uint64_t timeout_value = stream->write_timeout;

    if (length == 0)
        return length;

    if (stream->status.write_timeout ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return -1;

    if (stream->loop->terminated)
        return -1;

    write.buffer = buffer;
    write.length = length;
    write.offset = 0;
    write.task = stream->loop->scheduler.current;

    stream->os_linux.reserved[1] = &write;

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = write.task;

        api_timeout_exec(&stream->loop->timeouts, &timeout, timeout_value);
    }

    api_loop_write_add(stream->loop, stream->fd, &stream->os_linux.e);

    do
    {
        api_task_sleep(write.task);

        if (stream->status.write_timeout ||
            stream->status.error != API__OK ||
            stream->status.closed ||
            stream->status.peer_closed ||
            stream->status.terminated)
            break;

        if (timeout_value > 0 && timeout.elapsed)
            break;
    }
    while (write.offset < write.length);

    api_loop_write_del(stream->loop, stream->fd, &stream->os_linux.e);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->timeouts, &timeout, 0);

    stream->os_linux.reserved[1] = 0;
    stream->write_bandwidth.sent += write.offset;
    stream->write_bandwidth.period += (stream->loop->now - now);

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.write_timeout = 1;
        stream->filter_head->on_write_timeout(stream->filter_head);
    }

    return write.offset;
}

void aio_read_completion_handler(sigval_t sigval)
{
    api_stream_file_read_t* read = (api_stream_file_read_t*)sigval.sival_ptr;
    ssize_t result;
    int error = aio_error(&read->aio);

    if (error == 0)
    {
        result = aio_return(&read->aio);
        if (result < 0)
        {
            read->error = api_error_translate(result);
            result = 0;
        }

        read->done = result;
    }
    else
    {
        read->error = api_error_translate(error);
    }

    api_async_wakeup(read->loop, read->task);
}

void aio_write_completion_handler(sigval_t sigval)
{
    api_stream_file_write_t* write =
        (api_stream_file_write_t*)sigval.sival_ptr;
    ssize_t result;
    int error = aio_error(&write->aio);

    if (error == 0)
    {
        result = aio_return(&write->aio);
        if (result < 0)
        {
            write->error = api_error_translate(result);
            result = 0;
        }

        write->done = result;
    }
    else
    {
        write->error = api_error_translate(error);
    }

    api_async_wakeup(write->loop, write->task);
}

size_t api_stream_file_on_read(struct api_filter_t* filter,
                               char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_file_read_t read;
    api_timer_t timeout;
    uint64_t now = stream->loop->now;
    uint64_t timeout_value = stream->read_timeout;
    int result;

    if (length == 0)
        return length;

    if (stream->status.read_timeout ||
        stream->status.eof ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    bzero(&read, sizeof(struct api_stream_file_read_t));

    read.aio.aio_fildes = stream->fd;
    read.aio.aio_buf = buffer;
    read.aio.aio_nbytes = length;
    read.aio.aio_offset = stream->impl.file.read_offset;
    read.aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    read.aio.aio_sigevent.sigev_notify_function = aio_read_completion_handler;
    read.aio.aio_sigevent.sigev_notify_attributes = 0;
    read.aio.aio_sigevent.sigev_value.sival_ptr = &read;
    read.done = 0;
    read.loop = stream->loop;
    read.task = stream->loop->scheduler.current;

    result = aio_read(&read.aio);

    if (result == -1)
    {
        stream->status.error = api_error_translate(errno);
        stream->filter_head->on_error(stream->filter_head, 
                                stream->status.error);
        return 0;
    }

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = read.task;

        api_timeout_exec(&stream->loop->timeouts, &timeout, timeout_value);
    }

    api_task_sleep(read.task);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->timeouts, &timeout, 0);

    stream->read_bandwidth.read += read.done;
    stream->read_bandwidth.period += (stream->loop->now - now);

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.read_timeout = 1;
        stream->filter_head->on_read_timeout(stream->filter_head);

        result = aio_cancel(stream->fd, &read.aio);
        /* handle error */

        return 0;
    }
    else
    {
        stream->impl.file.read_offset += read.done;
        if (read.done == 0)
            stream->status.eof = 1;

        if (read.error != 0)
            stream->status.error = read.error;

        return read.done;
    }
}

size_t api_stream_file_on_write(struct api_filter_t* filter,
                                const char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_file_write_t write;
    api_timer_t timeout;
    uint64_t timeout_value = stream->write_timeout;
    uint64_t now = stream->loop->now;
    uint64_t done = 0;
    int result;

    if (length == 0)
        return length;

    if (stream->status.write_timeout ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    if (stream->loop->terminated)
        return 0;

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = write.task;

        api_timeout_exec(&stream->loop->timeouts, &timeout, timeout_value);
    }

    do
    {
        bzero(&write, sizeof(struct api_stream_file_write_t));

        write.aio.aio_fildes = stream->fd;
        write.aio.aio_buf = (char*)buffer + done;
        write.aio.aio_nbytes = length - done;
        write.aio.aio_offset = stream->impl.file.write_offset;
        write.aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
        write.aio.aio_sigevent.sigev_notify_function =
                                    aio_write_completion_handler;
        write.aio.aio_sigevent.sigev_notify_attributes = 0;
        write.aio.aio_sigevent.sigev_value.sival_ptr = &write;
        write.done = 0;
        write.loop = stream->loop;
        write.task = stream->loop->scheduler.current;

        result = aio_write(&write.aio);

        if (result == -1)
        {
            stream->status.error = api_error_translate(errno);
            stream->filter_head->on_error(stream->filter_head,
                                            stream->status.error);
            break;
        }

        api_task_sleep(write.task);

        if (stream->loop->terminated)
            break;

        if (stream->status.write_timeout ||
            stream->status.error != API__OK ||
            stream->status.closed ||
            stream->status.peer_closed ||
            stream->status.terminated)
            break;

        if (timeout_value > 0 && timeout.elapsed)
            break;

        if (write.error != 0)
        {
            stream->status.error = write.error;
            break;
        }

        stream->impl.file.write_offset += write.done;
        done += write.done;

    }
    while (done < length);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->timeouts, &timeout, 0);

    stream->write_bandwidth.sent += done;
    stream->write_bandwidth.period += (stream->loop->now - now);

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.write_timeout = 1;
        stream->filter_head->on_write_timeout(stream->filter_head);

        result = aio_cancel(stream->fd, &write.aio);
        /* handle error */

        return 0;
    }
    else
    {
        return done;
    }
}

void api_stream_on_read_timeout(struct api_filter_t* filter)
{
}

void api_stream_on_write_timeout(struct api_filter_t* filter)
{
}

void api_stream_on_error(struct api_filter_t* filter, int code)
{
}

void api_stream_on_peerclosed(struct api_filter_t* filter)
{
}

void api_stream_on_closed(struct api_filter_t* filter)
{
}

void api_stream_on_terminate(struct api_filter_t* filter)
{
}

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

void api_stream_processor(api_stream_t* stream, int events)
{
    api_task_t* task = 0;

    if (events == -1)
    {
        stream->status.terminated = 1;
        stream->filter_head->on_terminate(stream->filter_head);
    }
    else
    if (events & EPOLLERR)
    {
        stream->status.error = api_error_translate(errno);
        stream->filter_head->on_error(stream->filter_head, 
                                        stream->status.error);
    }
    else if (events & EPOLLHUP)
    {
        stream->status.closed = 1;
        stream->filter_head->on_closed(stream->filter_head);
    }
    else if (events & EPOLLRDHUP)
    {
        stream->status.peer_closed = 1;
        stream->filter_head->on_peerclosed(stream->filter_head);
    }
    else
    {
        if ((events & EPOLLIN) || (events & EPOLLPRI))
        {
            api_stream_read_try(stream);
            task = ((api_stream_read_t*)stream->os_linux.reserved[0])->task;
        }
        else
        if (events & EPOLLOUT)
        {
            api_stream_write_try(stream);
            task = ((api_stream_write_t*)stream->os_linux.reserved[1])->task;
        }
        else
        {
            stream->status.error = api_error_translate(errno);
            stream->filter_head->on_error(stream->filter_head,
                                    stream->status.error);
        }
    }

    if (task == 0 && stream->os_linux.reserved[0] != 0)
        task = ((api_stream_read_t*)stream->os_linux.reserved[0])->task;

    if (task == 0 && stream->os_linux.reserved[1] != 0)
        task = ((api_stream_write_t*)stream->os_linux.reserved[1])->task;

    if (task != 0)
        api_task_wakeup(task);
}

void api_stream_init(api_stream_t* stream, api_stream_type_t type, fd_t fd)
{
    memset(stream, 0, sizeof(*stream));

    stream->type = type;
    stream->fd = fd;
    stream->os_linux.processor = api_stream_processor;
    stream->os_linux.e.data.ptr = &stream->os_linux;

    api_filter_attach(&stream->operations, stream);

    stream->operations.on_closed = api_stream_on_closed;
    stream->operations.on_error = api_stream_on_error;
    stream->operations.on_peerclosed = api_stream_on_peerclosed;
    stream->operations.on_terminate = api_stream_on_terminate;
    stream->operations.on_read_timeout = api_stream_on_read_timeout;
    stream->operations.on_write_timeout = api_stream_on_write_timeout;

    switch (type) {
    case STREAM_File:
        stream->operations.on_read = api_stream_file_on_read;
        stream->operations.on_write = api_stream_file_on_write;
        break;
    case STREAM_Tcp:
        stream->operations.on_read = api_stream_on_read;
        stream->operations.on_write = api_stream_on_write;
        break;
    case STREAM_Udp:
        break;
    case STREAM_Tty:
        break;
    case STREAM_Pipe:
        break;
    default: // STREAM_Memory
        break;
    }
}

int api_stream_attach(api_stream_t* stream, api_loop_t* loop)
{
    int error = API__OK;

    if (loop->terminated)
    {
        stream->status.terminated = 1;
        return API__TERMINATE;
    }

    if (stream->type == STREAM_Tcp)
    {
        stream->os_linux.e.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;
        error = epoll_ctl(loop->epoll, EPOLL_CTL_ADD, stream->fd,
                            &stream->os_linux.e);
    }
    
    if (!error)
    {
        stream->loop = loop;
        api_loop_ref(loop);
        return API__OK;
    }

    return api_error_translate(errno);
}

size_t api_stream_read(api_stream_t* stream, char* buffer, size_t length)
{
    size_t done = 0;

    if (length == 0)
        return length;

    if (stream->status.read_timeout ||
        stream->status.eof ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    if (stream->loop->terminated)
    {
        stream->status.terminated = 1;
        return 0;
    }

    if (stream->unread.length > 0)
    {
        if (stream->unread.length <= length)
        {
            done = stream->unread.length;
            memcpy(buffer, stream->unread.buffer + stream->unread.offset,
                stream->unread.length);
            api_free(api_pool_default(stream->loop), stream->unread.length,
                stream->unread.buffer);
            stream->unread.length = 0;
        }
        else
        {
            done = length;
            memcpy(buffer, stream->unread.buffer + stream->unread.offset,
                length);
            stream->unread.offset += length;
            stream->unread.length -= length;
        }

        return done;
    }

    return stream->filter_head->on_read(stream->filter_head, buffer, length);
}

size_t api_stream_unread(api_stream_t* stream,
                         const char* buffer, size_t length)
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

size_t api_stream_write(api_stream_t* stream,
                        const char* buffer, size_t length)
{
    if (length == 0)
        return length;

    if (stream->status.write_timeout ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    if (stream->loop->terminated)
        return 0;

    return stream->filter_head->on_write(stream->filter_head, buffer, length);
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
    transfer.writer = src->loop->scheduler.current;
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

int api_stream_close(api_stream_t* stream)
{
    int error = 0;

    if (stream->type == STREAM_File)
    {
        stream->status.closed = 1;
        close(stream->fd);
    }
    else
    {
        error = epoll_ctl(stream->loop->epoll, EPOLL_CTL_DEL, stream->fd,
                        &stream->os_linux.e);
        if (error == 0)
        {
            stream->status.closed = 1;
            close(stream->fd);
        }
    }

    stream->filter_head->on_closed(stream->filter_head);

    if (stream->unread.length > 0) 
    {
        api_free(api_pool_default(stream->loop), stream->unread.length,
                    stream->unread.buffer);
        stream->unread.length = 0;
    }

    if (stream->loop != 0)
    {
        api_loop_unref(stream->loop);
        stream->loop = 0;
        return API__OK;
    }

    return api_error_translate(errno);
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