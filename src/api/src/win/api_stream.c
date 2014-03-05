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

#include "../../include/api.h"
#include "api_error.h"
#include "api_stream.h"
#include "api_async.h"

/* read/write request */
typedef struct api_stream_req_t {
    uint64_t done;
    api_task_t* task;	
} api_stream_req_t;

size_t api_stream_on_read(struct api_filter_t* filter,
                          char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_req_t read;
    api_timer_t timeout;
    uint64_t timeout_value = stream->read_timeout;
    uint64_t now = stream->loop->base.now;
    WSABUF wsabuf;
    DWORD flags = 0;
    DWORD sys_error;
    BOOL completed = FALSE;

    if (length == 0)
        return length;

    if (stream->status.read_timeout ||
        stream->status.eof ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    read.done = 0;
    read.task = stream->loop->base.scheduler.current;

    stream->os_win.reserved[0] = &read;

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = read.task;

        api_timeout_exec(&stream->loop->base.timeouts, &timeout, timeout_value);
    }

    switch (stream->type) {
        case STREAM_File: {
            *(uint64_t*)&stream->os_win.read.Offset =
                stream->impl.file.read_offset;

            completed = ReadFile((HANDLE)stream->fd, buffer, length,
                        (LPDWORD)&read.done, &stream->os_win.read);

            if (!completed)
            {
                sys_error = WSAGetLastError();
                if (sys_error == ERROR_SUCCESS)
                {
                    completed = TRUE;
                }
                else
                if (sys_error != WSA_IO_PENDING)
                {
                    completed = TRUE;
                    stream->status.error = api_error_translate(sys_error);
                    stream->filter_head->on_error(stream->filter_head,
                                                stream->status.error);
                }
            }

            break;
        }

        case STREAM_Tcp: {
            wsabuf.buf = buffer;
            wsabuf.len = length;
            completed = WSARecv((SOCKET)stream->fd, &wsabuf, 1,
                (LPDWORD)&read.done, &flags, &stream->os_win.read, NULL);

            if (completed == SOCKET_ERROR)
            {
                sys_error = WSAGetLastError();
                if (sys_error == ERROR_SUCCESS)
                {
                    // completed immediately
                    completed = TRUE;
                }
                else
                if (sys_error == WSA_IO_PENDING)
                {
                    // will complete asyncrounously
                    completed = FALSE;
                }
                else
                {
                    // error occured
                    completed = TRUE;
                    stream->status.error = api_error_translate(sys_error);
                    stream->filter_head->on_error(stream->filter_head,
                                            stream->status.error);
                }
            }
            else
            {
                // completed immediately
                completed = TRUE;
            }

            break;
        }
    }

    if (!completed)
        api_task_sleep(read.task);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->base.timeouts, &timeout, 0);

    stream->os_win.reserved[0] = 0;
    stream->read_bandwidth.read += read.done;
    stream->read_bandwidth.period += (stream->loop->base.now - now);

    if (stream->type == STREAM_File)
        stream->impl.file.read_offset += read.done;

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.read_timeout = 1;
        stream->filter_head->on_read_timeout(stream->filter_head);

        return 0;
    }
    else
    {
        return (size_t)read.done;
    }
}

size_t api_stream_on_write(struct api_filter_t* filter,
                        const char* buffer, size_t length)
{
    api_stream_t* stream = filter->stream;
    api_stream_req_t write;
    api_timer_t timeout;
    uint64_t timeout_value = stream->write_timeout;
    uint64_t now = stream->loop->base.now;
    size_t offset = 0;
    WSABUF wsabuf;
    DWORD flags = 0;
    DWORD sys_error;
    BOOL completed = FALSE;

    if (length == 0)
        return length;

    if (stream->status.write_timeout ||
        stream->status.error != API__OK ||
        stream->status.closed ||
        stream->status.peer_closed ||
        stream->status.terminated)
        return 0;

    if (stream->loop->base.terminated)
        return 0;

    write.done = 0;
    write.task = stream->loop->base.scheduler.current;

    stream->os_win.reserved[1] = &write;

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = write.task;

        api_timeout_exec(&stream->loop->base.timeouts, &timeout, timeout_value);
    }

    do
    {
        switch (stream->type) {
            case STREAM_File: {
                *(uint64_t*)&stream->os_win.write.Offset = 
                    stream->impl.file.write_offset;

                completed = WriteFile((HANDLE)stream->fd, buffer + offset,
                    length - offset, (LPDWORD)&write.done,
                    &stream->os_win.write);

                if (!completed)
                {
                    sys_error = WSAGetLastError();
                    if (sys_error == ERROR_SUCCESS)
                    {
                        completed = TRUE;
                    }
                    else
                    if (sys_error != WSA_IO_PENDING)
                    {
                        completed = TRUE;
                        stream->status.error = api_error_translate(sys_error);
                        stream->filter_head->on_error(stream->filter_head,
                                                stream->status.error);
                    }
                }

                break;
            }

            case STREAM_Tcp: {
                wsabuf.buf = (char*)buffer;
                wsabuf.len = length;
                completed = WSASend((SOCKET)stream->fd, &wsabuf, 1,
                    (LPDWORD)&write.done, 0, &stream->os_win.write, NULL);

                if (completed == SOCKET_ERROR)
                {
                    sys_error = WSAGetLastError();
                    if (sys_error == ERROR_SUCCESS)
                    {
                        // completed immediately
                        completed = TRUE;
                    }
                    else
                    if (sys_error == WSA_IO_PENDING)
                    {
                        // will complete asyncrounously
                        completed = FALSE;
                    }
                    else
                    {
                        // error occured
                        completed = TRUE;
                        stream->status.error = api_error_translate(sys_error);
                        stream->filter_head->on_error(stream->filter_head,
                                            stream->status.error);
                    }
                }
                else
                {
                    // completed immediately
                    completed = TRUE;
                }

                break;
            }
        }

        if (!completed)
            api_task_sleep(write.task);

        if (stream->type == STREAM_File)
            stream->impl.file.write_offset += write.done;

        if (stream->status.write_timeout ||
            stream->status.error != API__OK ||
            stream->status.closed ||
            stream->status.peer_closed ||
            stream->status.terminated)
            break;

        if (timeout_value > 0 && timeout.elapsed)
            break;

        offset += (size_t)write.done;
    }
    while (offset < length && write.done > 0);

    if (timeout_value > 0)
        api_timeout_exec(&stream->loop->base.timeouts, &timeout, 0);

    stream->os_win.reserved[1] = 0;
    stream->write_bandwidth.sent += write.done;
    stream->write_bandwidth.period += (stream->loop->base.now - now);

    if (timeout_value > 0 && timeout.elapsed)
    {
        stream->status.write_timeout = 1;
        stream->filter_head->on_write_timeout(stream->filter_head);
    }

    return offset;
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

void api_stream_processor(void* e, DWORD transferred,
                          OVERLAPPED* overlapped, struct api_loop_t* loop,
                          DWORD error)
{
    api_stream_t* stream = 
        (api_stream_t*)((char*)e - offsetof(api_stream_t, os_win));
    api_stream_req_t* req;
    int is_read = 0;

    if (overlapped == &stream->os_win.read)
    {
        req = (api_stream_req_t*)stream->os_win.reserved[0];
        is_read = 1;
    }
    else
    {
        req = (api_stream_req_t*)stream->os_win.reserved[1];
        is_read = 0;
    }

    /* we cannot remove handle from iocp on timeout, so handle it here and
     * just ignore when timeout happened on a stream
     */
    if (req)
    {
        if (is_read)
        {
            if (stream->status.read_timeout)
                return;
        }
        else
        {
            if (stream->status.write_timeout)
                return;
        }

        req->done = transferred;
        if (transferred == 0)
        {
            if (error == ERROR_HANDLE_EOF)
                stream->status.eof = 1;
            else
                stream->status.error = api_error_translate(error);
        }

        api_task_wakeup(req->task);
    }
}

void api_stream_init(api_stream_t* stream, api_stream_type_t type, fd_t fd)
{
    memset(stream, 0, sizeof(*stream));

    stream->type = type;
    stream->fd = fd;
    stream->os_win.processor = api_stream_processor;

    api_filter_attach(&stream->operations, stream);

    stream->operations.on_closed = api_stream_on_closed;
    stream->operations.on_error = api_stream_on_error;
    stream->operations.on_peerclosed = api_stream_on_peerclosed;
    stream->operations.on_terminate = api_stream_on_terminate;
    stream->operations.on_read_timeout = api_stream_on_read_timeout;
    stream->operations.on_write_timeout = api_stream_on_write_timeout;
    stream->operations.on_read = api_stream_on_read;
    stream->operations.on_write = api_stream_on_write;
}

int api_stream_attach(api_stream_t* stream, api_loop_t* loop)
{
    HANDLE handle = 0;
    int error = API_OK;

    if (loop->base.terminated)
    {
        stream->status.terminated = 1;
        return API__TERMINATE;
    }

    if (stream->type == STREAM_File || stream->type == STREAM_Tcp)
    {
        handle = CreateIoCompletionPort((HANDLE)stream->fd, loop->iocp,
                                        (UINT_PTR)&stream->os_win, 0);
        if (handle == NULL)
            error = api_error_translate(GetLastError());
        else
        {
            SetFileCompletionNotificationModes((HANDLE)stream->fd,
                            FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
        }
    }
    
    if (!error)
    {
        stream->loop = loop;
        api_loop_ref(loop);
    }

    return error;
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

    if (stream->loop->base.terminated)
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

    if (stream->loop->base.terminated)
        return 0;

    return stream->filter_head->on_write(stream->filter_head, buffer, length);
}

int api_stream_close(api_stream_t* stream)
{
    int error = API__OK;

    if (stream->status.closed)
        return error;

    if (stream->type == STREAM_File)
    {
        stream->status.closed = 1;
        CloseHandle((HANDLE)stream->fd);
        stream->fd = 0;
    }
    else
    if (stream->type == STREAM_Tcp)
    {
        stream->status.closed = 1;
        closesocket((SOCKET)stream->fd);
        stream->fd = 0;
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
    }

    return error;
}