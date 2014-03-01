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

#ifndef API_SOCKET_H_INCLUDED
#define API_SOCKET_H_INCLUDED

#include "../../include/api.h"

extern LPFN_ACCEPTEX lpfnAcceptEx;
extern LPFN_DISCONNECTEX lpfnDisconnectEx;
extern LPFN_CONNECTEX lpfnConnectEx;
extern LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;

void api_socket_init();


/*
 *  socket
 */

int api_socket_non_block(SOCKET fd, int on);
int api_socket_send_buffer_size(SOCKET fd, int size);
int api_socket_recv_buffer_size(SOCKET fd, int size);


/*
 *  tcp
 */

int api_tcp_nodelay(SOCKET fd, int enable);
int api_tcp_keepalive(SOCKET fd, int enable, unsigned int delay);


#endif // API_SOCKET_H_INCLUDED