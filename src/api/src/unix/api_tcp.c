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

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <memory.h>

#include "../../include/api.h"
#include "api_socket.h"
#include "api_error.h"
#include "api_loop.h"


typedef struct api_tcp_listener_accept_t {
    api_tcp_t* tcp;
    int success;
    api_task_t* task;
} api_tcp_listener_accept_t;

int api_tcp_listener_accept_try(struct api_tcp_listener_t* listener)
{
    api_tcp_listener_accept_t* data =
        (api_tcp_listener_accept_t*)listener->os_linux.reserved;
    int error;

    do
    {
        data->tcp->stream.fd = accept(listener->fd,
                        (struct sockaddr*)&data->tcp->address.address,
                        &data->tcp->address.length);
        if (data->tcp->stream.fd == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                /* We have processed all incoming connections. */
                break;
            }
            else
            {
                listener->status.error = api_error_translate(errno);
                listener->on_error(listener, errno);
                break;
            }
        }
        else
        {
            data->success = listener->on_accept(listener, data->tcp);
            if (!data->success)
            {
                close(data->tcp->stream.fd);
            }
        }
    }
    while (!data->success);

    if (data->success)
    {
        error = api_socket_non_block(data->tcp->stream.fd, 1);
        error = api_socket_recv_buffer_size(data->tcp->stream.fd, 0);
        error = api_socket_send_buffer_size(data->tcp->stream.fd, 0);
        error = api_tcp_nodelay(data->tcp->stream.fd, 1);

        api_stream_init(&data->tcp->stream, STREAM_Tcp, data->tcp->stream.fd);
    }

    return data->success;
}

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

void api_tcp_listener_processor(struct api_tcp_listener_t* listener,
                                int events)
{
    int error;
    int wakeup = 1;
    api_task_t* task = 0;

    if (events == -1)
    {
        listener->status.terminated = 1;
        listener->on_terminate(listener);
    }
    else
    if (events & EPOLLERR)
    {
        listener->status.error = api_error_translate(errno);
        listener->on_error(listener, listener->status.error);
    }
    else if (events & EPOLLHUP)
    {
        listener->status.closed = 1;
        listener->on_closed(listener);
    }
    else
    {
        if ((events & EPOLLIN) || (events & EPOLLPRI))
        {
            wakeup = api_tcp_listener_accept_try(listener);
        }
        else
        {
            listener->status.error = api_error_translate(errno);
            listener->on_error(listener, listener->status.error);
        }
    }

    if (wakeup)
    {
        if (listener->os_linux.reserved != 0)
            task = 
            ((api_tcp_listener_accept_t*)listener->os_linux.reserved)->task;

        if (task != 0)
            api_task_wakeup(task);
    }
}

int api_tcp_listen(api_tcp_listener_t* listener, api_loop_t* loop,
                           const char* ip, int port, int backlog)
{
    struct sockaddr_in* addr_in =
        (struct sockaddr_in*)&listener->address.address;
    struct sockaddr_in6* addr_in6 =
        (struct sockaddr_in6*)&listener->address.address;
    struct sockaddr* a = (struct sockaddr*)&listener->address.address;
    int error;

    if (loop->terminated)
    {
        listener->status.terminated = 1;
        return API__TERMINATE;
    }

    memset(listener, 0, sizeof(*listener));

    if (strchr(ip, ':') == 0)
    {
        // ipv4
        listener->fd = 
            socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        listener->address.length = sizeof(struct sockaddr_in);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = inet_addr(ip);
        addr_in->sin_port = htons(port);

        listener->os_linux.af = AF_INET;
    }
    else
    {
        // ipv6
        listener->fd =
            socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        listener->address.length = sizeof(struct sockaddr_in6);
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(port);

        inet_pton(AF_INET6, ip, (void*)&addr_in6->sin6_addr.__in6_u);

        listener->os_linux.af = AF_INET6;
    }

    error = api_socket_non_block(listener->fd, 1);
    error = api_socket_recv_buffer_size(listener->fd, 0);
    error = api_socket_send_buffer_size(listener->fd, 0);
    error = api_tcp_nodelay(listener->fd, 1);

    if (0 != bind(listener->fd, a, listener->address.length))
        return api_error_translate(errno);

    if (0 != listen(listener->fd, backlog))
        return api_error_translate(errno);

    listener->loop = loop;
    listener->os_linux.processor = api_tcp_listener_processor;
    listener->os_linux.e.data.ptr = listener;

    listener->on_closed = api_tcp_listener_on_closed;
    listener->on_error = api_tcp_listener_on_error;
    listener->on_terminate = api_tcp_listener_on_terminate;
    listener->on_accept = api_tcp_listener_on_accept;

    listener->os_linux.e.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    error = epoll_ctl(loop->epoll, EPOLL_CTL_ADD, listener->fd,
                        &listener->os_linux.e);

    if (!error)
    {
        listener->loop == loop;
        api_loop_ref(loop);
    }

    return api_error_translate(error);
}

int api_tcp_accept(api_tcp_listener_t* listener, api_tcp_t* tcp)
{
    api_tcp_listener_accept_t accept;

    if (listener->loop->terminated)
        return API__TERMINATE;

    if (listener->status.closed ||
        listener->status.terminated ||
        listener->status.error != API__OK)
        return listener->status.error;

    accept.task = listener->loop->scheduler.current;
    accept.tcp = tcp;
    accept.success = 0;

    tcp->address.address.ss_family = listener->os_linux.af;
    tcp->address.length = sizeof(tcp->address.address);

    listener->os_linux.reserved = &accept;

    api_loop_read_add(listener->loop, listener->fd, &listener->os_linux.e);

    api_task_sleep(accept.task);

    api_loop_read_del(listener->loop, listener->fd, &listener->os_linux.e);

    listener->os_linux.reserved = 0;

    if (accept.success)
        return API__OK;

    return listener->status.error;
}

int api_tcp_close(api_tcp_listener_t* listener)
{
    int error;

    error = epoll_ctl(listener->loop->epoll, EPOLL_CTL_DEL, listener->fd,
                        &listener->os_linux.e);

    close(listener->fd);
    listener->status.closed = 1;

    if (listener->loop != 0)
    {
        api_loop_unref(listener->loop);
        listener->loop = 0;
    }

    return api_error_translate(error);
}

void api_tcp_connect_processor(api_stream_t* stream, int events)
{
    if (events == -1)
    {
        stream->status.terminated = 1;
    }
    else
    if (events & EPOLLERR)
    {
        stream->status.error = api_error_translate(errno);
    }
    else if (events & EPOLLHUP)
    {
        stream->status.closed = 1;
    }
    else if ((events & EPOLLOUT) != EPOLLOUT)
    {
        stream->status.error = api_error_translate(errno);
    }

    api_task_wakeup((api_task_t*)stream->os_linux.reserved[0]);
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
    int error = API__OK;
    api_timer_t timeout;
    uint64_t timeout_value = tmeout;

    memset(tcp, 0, sizeof(*tcp));

    if (loop->terminated)
    {
        tcp->stream.status.terminated = 1;
        return API__TERMINATE;
    }

    tcp->stream.os_linux.processor = api_tcp_connect_processor;
    tcp->stream.os_linux.reserved[0] = loop->scheduler.current;
    tcp->stream.os_linux.e.data.ptr = &tcp->stream.os_linux;

    if (strchr(ip, ':') == 0)
    {
        // ipv4
        tcp->stream.fd = 
            socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        tcp->address.length = sizeof(struct sockaddr_in);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = inet_addr(ip);
        addr_in->sin_port = htons(port);
    }
    else
    {
        // ipv6
        tcp->stream.fd =
            socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        tcp->address.length = sizeof(struct sockaddr_in6);
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(port);

        inet_pton(AF_INET6, ip, (void*)&addr_in6->sin6_addr.__in6_u);
    }

    error = api_socket_non_block(tcp->stream.fd, 1);
    error = api_socket_recv_buffer_size(tcp->stream.fd, 0);
    error = api_socket_send_buffer_size(tcp->stream.fd, 0);
    error = api_tcp_nodelay(tcp->stream.fd, 1);

    if (0 != connect(tcp->stream.fd, a, tcp->address.length))
    {
        if (errno != EINPROGRESS)
        {
            error = api_error_translate(errno);
        }
        else
        {
            // wait needed

            tcp->stream.os_linux.e.events = 
                EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLOUT;

            error = epoll_ctl(loop->epoll, EPOLL_CTL_ADD, tcp->stream.fd,
                                        &tcp->stream.os_linux.e);

            error = api_error_translate(error);
            if (API__OK == error)
            {
                error = api_loop_write_add(loop, tcp->stream.fd, 
                                        &tcp->stream.os_linux.e);

                if (API__OK != error)
                {
                    epoll_ctl(loop->epoll, EPOLL_CTL_DEL, tcp->stream.fd,
                        &tcp->stream.os_linux.e);
                }
                else
                {
                    if (timeout_value > 0)
                    {
                        memset(&timeout, 0, sizeof(timeout));
                        timeout.task = loop->scheduler.current;

                        api_timeout_exec(&loop->timeouts, &timeout,
                                                    timeout_value);
                    }

                    api_task_sleep(loop->scheduler.current);

                    if (timeout_value > 0)
                        api_timeout_exec(&loop->timeouts, &timeout, 0);

                    if (timeout_value > 0 && timeout.elapsed)
                    {
                        tcp->stream.status.read_timeout = 1;
                        error = api_error_translate(ETIMEDOUT);
                    }

                    if (tcp->stream.status.closed ||
                        tcp->stream.status.terminated ||
                        tcp->stream.status.error != API__OK)
                    {
                        epoll_ctl(loop->epoll, EPOLL_CTL_DEL, tcp->stream.fd,
                                                &tcp->stream.os_linux.e);
                    }
                    else
                    {
                        api_loop_write_del(loop, tcp->stream.fd,
                                            &tcp->stream.os_linux.e);
                    }
                }
            }
        }
    }
    else
    {
        // completed immediately
    }

    if (API__OK == error &&
        tcp->stream.status.closed == 0 &&
        tcp->stream.status.terminated == 0 &&
        tcp->stream.status.error == API__OK)
    {
        api_stream_init(&tcp->stream, STREAM_Tcp, tcp->stream.fd);
        tcp->stream.loop = loop;
        api_loop_ref(loop);
        return API__OK;
    }

    close(tcp->stream.fd);

    if (API__OK != error)
        return error;

    if (API__OK != tcp->stream.status.error)
        return tcp->stream.status.error;

    return -1;
}