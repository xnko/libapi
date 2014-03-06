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

#include "api_stream.h"
#include "api_socket.h"
#include "api_error.h"
#include "../api_list.h"


typedef struct api_tcp_listener_accept_t {
    api_task_t* task;
} api_tcp_listener_accept_t;

int api_tcp_listener_on_accept(struct api_tcp_listener_t* listener,
                               api_tcp_t* connection)
{
    return 1;
}

void api_tcp_listener_on_error(struct api_tcp_listener_t* listener, int code)
{
}

void api_tcp_listener_on_closed(struct api_tcp_listener_t* listener)
{
}

void api_tcp_listener_on_terminate(struct api_tcp_listener_t* listener)
{
}

void api_tcp_listener_processor(void* e, DWORD transferred,
                            OVERLAPPED* overlapped, struct api_loop_t* loop,
                            DWORD error)
{
    api_tcp_listener_t* listener = 
        (api_tcp_listener_t*)((char*)e - offsetof(api_tcp_listener_t, os_win));
    
    api_tcp_listener_accept_t* req = 
        (api_tcp_listener_accept_t*)listener->os_win.reserved;

    api_task_wakeup(req->task);
}

int api_tcp_listen(api_tcp_listener_t* listener, api_loop_t* loop,
                           const char* ip, int port, int backlog)
{
    struct sockaddr_in* addr_in = 
        (struct sockaddr_in*)&listener->address.address;
    struct sockaddr_in6* addr_in6 = 
        (struct sockaddr_in6*)&listener->address.address;
    struct sockaddr* a = (struct sockaddr*)&listener->address.address;
    HANDLE handle;
    DWORD sys_error = 0;
    int error;

    if (loop->base.terminated)
    {
        listener->status.terminated = 1;
        return API__TERMINATE;
    }

    memset(listener, 0, sizeof(*listener));

    if (strchr(ip, ':') == 0)
    {
        // ipv4
        listener->fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                NULL, 0, WSA_FLAG_OVERLAPPED);

        listener->address.length = sizeof(struct sockaddr_in);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = inet_addr(ip);
        addr_in->sin_port = htons(port);

        listener->os_win.af = AF_INET;
    }
    else
    {
        // ipv6
        listener->fd = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, 
                                NULL, 0, WSA_FLAG_OVERLAPPED);

        listener->address.length = sizeof(struct sockaddr_in6);
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(port);

        inet_pton(AF_INET6, ip, (void*)&addr_in6->sin6_addr);

        listener->os_win.af = AF_INET6;
    }

    error = api_socket_non_block(listener->fd, 1);
    error = api_socket_recv_buffer_size(listener->fd, 0);
    error = api_socket_send_buffer_size(listener->fd, 0);
    error = api_tcp_nodelay(listener->fd, 1);

    if (0 != bind(listener->fd, a, listener->address.length))
        return api_error_translate(WSAGetLastError());

    if (0 != listen(listener->fd, backlog))
        return api_error_translate(WSAGetLastError());

    listener->loop = loop;
    listener->os_win.processor = api_tcp_listener_processor;

    listener->on_closed = api_tcp_listener_on_closed;
    listener->on_error = api_tcp_listener_on_error;
    listener->on_terminate = api_tcp_listener_on_terminate;
    listener->on_accept = api_tcp_listener_on_accept;

    handle = CreateIoCompletionPort((HANDLE)listener->fd, loop->iocp, 
                                (ULONG_PTR)&listener->os_win, 0);
    if (handle == NULL)
    {
        sys_error = GetLastError();
        closesocket(listener->fd);
        error = api_error_translate(sys_error);
    }

    if (API__OK != error)
    {
        listener->loop = loop;
        api_loop_ref(loop);
        return API__OK;
    }
    else
    {
        SetFileCompletionNotificationModes((HANDLE)listener->fd,
                    FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
    }

    return error;
}

int api_tcp_accept(api_tcp_listener_t* listener, api_tcp_t* tcp)
{
    api_tcp_listener_accept_t accept;
    CHAR buffer[2 * (sizeof(SOCKADDR_IN) + 16)];
    SOCKADDR *lpLocalSockaddr = NULL;
    SOCKADDR *lpRemoteSockaddr = NULL;
    int localSockaddrLen = 0;
    int remoteSockaddrLen = 0;
    DWORD dwBytes;
    BOOL result;
    BOOL success = FALSE;
    BOOL completed = FALSE;
    DWORD sys_error;
    int error = API__OK;

    memset(&listener->os_win.ovl, 0, sizeof(listener->os_win.ovl));

    if (listener->loop->base.terminated)
        return API__TERMINATE;

    if (listener->status.closed ||
        listener->status.terminated ||
        listener->status.error != API__OK)
        return listener->status.error;

    accept.task = listener->loop->base.scheduler.current;

    listener->os_win.reserved = &accept;

    do {

        success = FALSE;
        completed = FALSE;

        tcp->stream.fd = WSASocket(listener->os_win.af, SOCK_STREAM,
                    IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        result = lpfnAcceptEx(listener->fd,
                            tcp->stream.fd,
                            buffer, 0, 
                            sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
                            &dwBytes, (LPOVERLAPPED)&listener->os_win.ovl);
        if (!result)
        {
            sys_error = WSAGetLastError();
            if (sys_error == ERROR_SUCCESS)
            {
                completed = TRUE;
            }
            else
            if (sys_error != WSA_IO_PENDING)
            {
                listener->status.error = api_error_translate(sys_error);
                closesocket(tcp->stream.fd);
                break;
            }
        }
        else
        {
            completed = TRUE;
        }

        if (!completed)
            api_task_sleep(accept.task);

        lpfnGetAcceptExSockaddrs(buffer, 0, 
            sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
            &lpLocalSockaddr, &localSockaddrLen,
            &lpRemoteSockaddr, &remoteSockaddrLen);

        tcp->address.length = remoteSockaddrLen;
        memcpy(&tcp->address.address, lpRemoteSockaddr, remoteSockaddrLen);

        success = listener->on_accept(listener, tcp);
        if (success)
        {
            error = api_socket_non_block(tcp->stream.fd, 1);
            error = api_socket_recv_buffer_size(tcp->stream.fd, 0);
            error = api_socket_send_buffer_size(tcp->stream.fd, 0);
            error = api_tcp_nodelay(tcp->stream.fd, 1);

            api_stream_init(&tcp->stream, STREAM_Tcp, tcp->stream.fd);
        }
        else
        {
            closesocket(tcp->stream.fd);
        }
    }
    while (!success);

    listener->os_win.reserved = 0;

    if (success)
        return API__OK;

    return listener->status.error;
}

int api_tcp_close(api_tcp_listener_t* listener)
{
    closesocket(listener->fd);
    listener->status.closed = 1;

    if (listener->loop != 0)
    {
        api_loop_unref(listener->loop);
        listener->loop = 0;
    }

    return API__OK;
}

void api_tcp_connect_processor(void* e, DWORD transferred,
                            OVERLAPPED* overlapped, struct api_loop_t* loop,
                            DWORD error)
{
    api_stream_t* stream = 
        (api_stream_t*)((char*)e - offsetof(api_stream_t, os_win));

    api_task_t* task = (api_task_t*)stream->os_win.reserved[0];

    api_task_wakeup(task);
}

int api_tcp_connect(api_tcp_t* tcp,
                    api_loop_t* loop,
                    const char* ip, int port, uint64_t tmeout)
{
    struct sockaddr_in* addr_in = 
        (struct sockaddr_in*)&tcp->address.address;
    struct sockaddr_in6* addr_in6 = 
        (struct sockaddr_in6*)&tcp->address.address;
    struct sockaddr* a = (struct sockaddr*)&tcp->address.address;
    api_timer_t timeout;
    uint64_t timeout_value = tmeout;
    HANDLE handle;
    DWORD dwSent = 0;
    DWORD sys_error = 0;
    BOOL result;
    BOOL completed = FALSE;
    int af;
    int error = API__OK;

    memset(tcp, 0, sizeof(*tcp));

    if (loop->base.terminated)
    {
        tcp->stream.status.terminated = 1;
        return API__TERMINATE;
    }

    tcp->stream.os_win.processor = api_tcp_connect_processor;
    tcp->stream.os_win.reserved[0] = loop->base.scheduler.current;

    if (strchr(ip, ':') == 0)
    {
        // ipv4
        tcp->stream.fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                NULL, 0, WSA_FLAG_OVERLAPPED);

        af = AF_INET;

        tcp->address.length = sizeof(struct sockaddr_in);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    else
    {
        // ipv6
        tcp->stream.fd = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, 
                                NULL, 0, WSA_FLAG_OVERLAPPED);

        af = AF_INET6;

        tcp->address.length = sizeof(struct sockaddr_in6);
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = 0;
    }

    error = api_socket_non_block(tcp->stream.fd, 1);
    error = api_socket_recv_buffer_size(tcp->stream.fd, 0);
    error = api_socket_send_buffer_size(tcp->stream.fd, 0);
    error = api_tcp_nodelay(tcp->stream.fd, 1);

    if (0 != bind(tcp->stream.fd, a, tcp->address.length))
    {
        error = api_error_translate(WSAGetLastError());
        closesocket(tcp->stream.fd);
        return error;
    }

    handle = CreateIoCompletionPort((HANDLE)tcp->stream.fd, loop->iocp, 
        (ULONG_PTR)&tcp->stream.os_win, 0);
    if (handle == NULL)
    {
        sys_error = GetLastError();
        closesocket(tcp->stream.fd);
        return api_error_translate(sys_error);
    }

    SetFileCompletionNotificationModes((HANDLE)tcp->stream.fd,
                    FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    if (af == AF_INET)
    {
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = inet_addr(ip);
        addr_in->sin_port = htons(port);
    }
    else
    {
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(port);

        inet_pton(AF_INET6, ip, (void*)&addr_in6->sin6_addr);
    }

    if (timeout_value > 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.task = loop->base.scheduler.current;

        api_timeout_exec(&loop->base.timeouts, &timeout, timeout_value);
    }

    result = lpfnConnectEx(tcp->stream.fd, a, tcp->address.length,
                        NULL, 0,
                        &dwSent, (LPOVERLAPPED)&tcp->stream.os_win.read);

    if (!result)
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
            error = api_error_translate(sys_error);
            tcp->stream.status.error = error;
        }
    }
    else
    {
        completed = TRUE;
    }

    if (!completed)
        api_task_sleep(loop->base.scheduler.current);

    if (timeout_value > 0)
        api_timeout_exec(&loop->base.timeouts, &timeout, 0);

    if (API_OK == error && timeout_value > 0 && timeout.elapsed)
    {
        tcp->stream.status.read_timeout = 1;
        error = api_error_translate(ERROR_TIMEOUT);
    }

    if (API__OK == error)
    {
        api_stream_init(&tcp->stream, STREAM_Tcp, tcp->stream.fd);
        tcp->stream.loop = loop;
        
        api_loop_ref(loop);
        return API__OK;
    }
    else
    {
        closesocket(tcp->stream.fd);
    }

    return error;
}