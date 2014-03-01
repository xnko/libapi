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

#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../../api/include/api.h"

#ifdef _WIN32
#if defined(BUILD_HTTP_SHARED)
    #define HTTP_EXTERN __declspec(dllexport)
#elif defined(USE_HTTP_SHARED)
    #define HTTP_EXTERN __declspec(dllimport)
#else
    #define HTTP_EXTERN
#endif
#elif __GNUC__ >= 4
    #define HTTP_EXTERN __attribute__((visibility("default")))
#else
    #define HTTP_EXTERN
#endif

typedef struct http_header_t {
    char* name;
    char* value;
    struct http_header_t* next;
} http_header_t;

typedef struct http_cookie_t {
    char* name;
    char* value;
    time_t expires;
    char* path;
    struct http_cookie_t* next;
} http_cookie_t;

typedef struct http_param_t {
    char* name;
    char* value;
    struct http_param_t* next;
} http_param_t;

typedef struct http_request_t {
    int major;
    int minor;
    const char* method;
    char* body;
    char* url;
    struct {
        char* schema;
        char* host;
        char* path;
        char* query;
        char* fragment;
    } uri;
    http_param_t* params;
    http_header_t* headers;
    http_cookie_t* cookies;
} http_request_t;

HTTP_EXTERN const char* http_request_parse(http_request_t* request,
                                           api_stream_t* stream);
HTTP_EXTERN void http_request_clean(http_request_t* request, api_pool_t* pool);
HTTP_EXTERN const char* http_request_get_header(http_request_t* request,
                                                const char* name);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HTTP_H_INCLUDED