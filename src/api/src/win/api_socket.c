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

#include "../../include/api.h"
#include "api_socket.h"
#include "api_error.h"

#pragma comment (lib, "Ws2_32.lib")

LPFN_ACCEPTEX lpfnAcceptEx = 0;
LPFN_DISCONNECTEX lpfnDisconnectEx = 0;
LPFN_CONNECTEX lpfnConnectEx = 0;
LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = 0;

void api_socket_init()
{
    WORD version = 0x202;
    WSADATA wsadata;
    SOCKET socket;
    DWORD dwBytes;
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    GUID GuidDisconnectEx = WSAID_DISCONNECTEX;
    GUID GuidConnectEx = WSAID_CONNECTEX;
    GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    int iResult;

    WSAStartup(version, &wsadata);

    socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                        NULL, 0, WSA_FLAG_OVERLAPPED);

    iResult = WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &GuidAcceptEx, sizeof(GuidAcceptEx),
                        &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                        &dwBytes, NULL, NULL);
    if (iResult == SOCKET_ERROR)
        return;

    iResult = WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &GuidDisconnectEx, sizeof(GuidDisconnectEx),
                        &lpfnDisconnectEx, sizeof(lpfnDisconnectEx),
                        &dwBytes, 0, 0);
    if (iResult == SOCKET_ERROR)
        return;

    iResult = WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &GuidConnectEx, sizeof(GuidConnectEx),
                        &lpfnConnectEx, sizeof(lpfnConnectEx),
                        &dwBytes, 0, 0);
    if (iResult == SOCKET_ERROR)
        return;

    iResult = WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &GuidGetAcceptExSockaddrs,
                        sizeof(GuidGetAcceptExSockaddrs),
                        &lpfnGetAcceptExSockaddrs,
                        sizeof(lpfnGetAcceptExSockaddrs), &dwBytes, 0, 0);
    if (iResult == SOCKET_ERROR)
        return;

    closesocket(socket);
}

int api_socket_non_block(SOCKET fd, int on)
{
    u_long v = on;

    if (SOCKET_ERROR == ioctlsocket(fd, FIONBIO, &v))
        return api_error_translate(WSAGetLastError());

    return API__OK;
}

int api_socket_send_buffer_size(SOCKET fd, int size)
{
    if (SOCKET_ERROR != setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                            (const char*)&size, sizeof(size)))
        return API__OK;

    return api_error_translate(WSAGetLastError());
}

int api_socket_recv_buffer_size(SOCKET fd, int size)
{
    if (SOCKET_ERROR != setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                            (const char*)&size, sizeof(size)))
        return API__OK;

    return api_error_translate(WSAGetLastError());
}

int api_tcp_nodelay(SOCKET fd, int enable)
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

int api_tcp_keepalive(SOCKET fd, int enable, unsigned int delay)
{
    if (SOCKET_ERROR == setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                            (const char*)&enable, sizeof(enable)))
        return api_error_translate(WSAGetLastError());

    return API__OK;
}