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

#include "../../include/api.h"
#include "api_socket.h"
#include "api_error.h"


int api_socket_non_block(int fd, int on)
{
    int r;

    do
    {
        r = ioctl(fd, FIONBIO, &on);
    }
    while (r == -1 && errno == EINTR);

    if (r)
        return api_error_translate(errno);

    return API__OK;
}

int api_socket_send_buffer_size(int fd, int size)
{
    if (0 == setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)))
        return API__OK;

    return api_error_translate(errno);
}

int api_socket_recv_buffer_size(int fd, int size)
{
    if (0 == setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)))
        return API__OK;

    return api_error_translate(errno);
}

int api_tcp_nodelay(int fd, int enable)
{
    int result = setsockopt(fd,
                    IPPROTO_TCP,     /* set option at TCP level */
                    TCP_NODELAY,     /* name of option */
                    (char*)&enable,  /* the cast is historical cruft */
                    sizeof(int));    /* length of option value */

    if (result == 0)
        return API__OK;

    return api_error_translate(result);
}

int api_tcp_keepalive(int fd, int enable, unsigned int delay)
{
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)))
        return api_error_translate(errno);

    if (enable && setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,
                            &delay, sizeof(delay)))
        return api_error_translate(errno);

    return API__OK;
}