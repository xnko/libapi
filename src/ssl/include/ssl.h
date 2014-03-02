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

#ifndef SSL_H_INCLUDED
#define SSL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../../api/include/api.h"

#ifdef _WIN32
#if defined(BUILD_SSL_SHARED)
    #define SSL_EXTERN __declspec(dllexport)
#elif defined(USE_SSL_SHARED)
    #define SSL_EXTERN __declspec(dllimport)
#else
    #define SSL_EXTERN
#endif
#elif __GNUC__ >= 4
    #define SSL_EXTERN __attribute__((visibility("default")))
#else
    #define SSL_EXTERN
#endif

typedef struct ssl_session_t {
    void* ctx;
} ssl_session_t;

typedef struct ssl_stream_t {
    api_filter_t filter;
    struct api_stream_t* stream;
    ssl_session_t* session;
    void* ssl;
    void* bio_read;
    void* bio_write;
    void* task;
    unsigned long ssl_error;
    int attached;
} ssl_stream_t;

SSL_EXTERN void ssl_init();
SSL_EXTERN int ssl_get_error_description(unsigned long error,
                                        char* buffer, size_t length);
SSL_EXTERN int ssl_session_start(ssl_session_t* session, 
                                const char* cert_file, const char* key_file);
SSL_EXTERN void ssl_session_stop(ssl_session_t* session);
SSL_EXTERN int ssl_stream_accept(ssl_session_t* session,
                                ssl_stream_t* ssl_stream, api_stream_t* stream);
SSL_EXTERN int ssl_stream_connect(ssl_session_t* session, 
                               ssl_stream_t* ssl_stream, api_stream_t* stream);
SSL_EXTERN void ssl_stream_detach(ssl_stream_t* ssl_stream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SSL_H_INCLUDED