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

#include <string.h>
#include <stdlib.h>
#include "../include/http.h"
#include "http_parser/http_parser.h"

#if !defined(__linux__)
#pragma warning(disable: 4996)
#endif

void str_free(api_pool_t* pool, char* str)
{
    if (str != 0)
        api_free(pool, strlen(str) + 1, str);
}

/*
The strndup function copies not more than n characters (characters that
follow a null character are not copied) from string to a dynamically
allocated buffer. The copied string shall always be null terminated.
*/
char *str_ndup(api_pool_t* pool, const char *string, size_t s)
{
    char *p,*r;

    if (string == 0)
        return 0;

    p = (char*)string;
    while (s > 0)
    {
        if (*p == 0)
            break;
    
        p++;
        s--;
    }

    s = (p - string);
    r = (char*)api_alloc(pool, 1 + s);
    if (r)
    {
        strncpy(r, string, s);
        r[s] = 0;
    }

    return r;
}

#if defined(__linux__)
#define strcmp_nocase strcasecmp
#else
#define strcmp_nocase _stricmp
#endif

typedef struct buffer_t {
    char* data;
    size_t used;
    size_t alloc;
} buffer_t;

typedef enum state_t {
    STATE_Url,
    STATE_HeaderName,
    STATE_HeaderValue
} state_t;

typedef struct http_parser_t {
    http_request_t* request;
    api_pool_t* pool;
    int headers_done;
    http_header_t* header;
    state_t state;
    buffer_t buffer;
    int last_was_value;
} http_parser_t;

void concat(api_pool_t* pool, buffer_t* buffer, const char* at, size_t length)
{
    char* buf;
    size_t last;

    if (buffer->data == 0)
    {
        buffer->alloc = length + 1;
        buffer->data = (char*)api_alloc(pool, buffer->alloc);
        buffer->used = length;

        strncpy(buffer->data, at, length);
        buffer->data[length] = 0;
    }
    else
    {
        if (buffer->alloc - buffer->used - 1 >= length)
        {
            strncpy(buffer->data + buffer->used, at, length);
            buffer->used += length;
            buffer->data[buffer->used] = 0;
        }
        else
        {
            last = buffer->alloc;
            buffer->alloc = buffer->used + 2 * length;
            buf = (char*)api_alloc(pool, buffer->alloc);
            strcpy(buf, buffer->data);
            strncpy(buf + buffer->used, at, length);
            api_free(pool, last, buffer->data);

            buffer->data = buf;
            buffer->used += length;
            buffer->data[buffer->used] = 0;
        }
    }
}

int http_on_message_begin_cb(http_parser* parser)
{
    return 0;
}

int http_on_url_cb(http_parser* parser, const char *at, size_t length)
{
    http_parser_t* ctx = (http_parser_t*)parser->data;

    ctx->state = STATE_Url;
    concat(ctx->pool, &ctx->buffer, at, length);
    return 0;
}

int http_on_status_complete_cb(http_parser* parser)
{
    return 0;
}

int http_on_header_field_cb(http_parser* parser, const char *at, size_t length)
{
    http_parser_t* ctx = (http_parser_t*)parser->data;

    if (ctx->state == STATE_Url)
    {
        ctx->request->url = ctx->buffer.data;
        ctx->buffer.alloc = 0;
        ctx->buffer.used = 0;
        ctx->buffer.data = 0;
    }

    ctx->state = STATE_HeaderName;

    if (ctx->last_was_value)
    {
        ctx->header->value = ctx->buffer.data;
        ctx->buffer.alloc = 0;
        ctx->buffer.used = 0;
        ctx->buffer.data = 0;

        ctx->header->next = ctx->request->headers;
        ctx->request->headers = ctx->header;
        ctx->header = 0;

        ctx->last_was_value = 0;
    }

    if (ctx->header == 0)
    {
        ctx->header = (http_header_t*)api_alloc(ctx->pool, sizeof(*ctx->header));
        memset(ctx->header, 0, sizeof(*ctx->header));
    }

    concat(ctx->pool, &ctx->buffer, at, length);
    
    return 0;
}

int http_on_header_value_cb(http_parser* parser, const char *at, size_t length)
{
    http_parser_t* ctx = (http_parser_t*)parser->data;

    if (!ctx->last_was_value)
    {
        ctx->header->name = ctx->buffer.data;
        ctx->buffer.alloc = 0;
        ctx->buffer.used = 0;
        ctx->buffer.data = 0;
    }

    ctx->last_was_value = 1;

    concat(ctx->pool, &ctx->buffer, at, length);

    return 0;
}

int http_on_headers_complete_cb(http_parser* parser)
{
    http_parser_t* ctx = (http_parser_t*)parser->data;

    if (ctx->header != 0)
    {
        ctx->header->value = ctx->buffer.data;
        ctx->header->next = ctx->request->headers;
        ctx->request->headers = ctx->header;
        ctx->header = 0;
    }
    else
    {
        ctx->request->url = ctx->buffer.data;
    }

    ctx->buffer.alloc = 0;
    ctx->buffer.used = 0;
    ctx->buffer.data = 0;

    ctx->headers_done = 1;
    return 1;
}

int http_on_body_cb(http_parser* parser, const char *at, size_t length)
{
    return 0;
}

int http_on_message_complete_cb(http_parser* parser)
{
    return 0;
}

char* decode_query(api_pool_t* pool, const char* in, size_t length)
{
    char* result = (char*)api_alloc(pool, length + 1);
    char c = 0;
    char decode_buffer[5] = { '0', 'x', 0, 0, 0 };
    int i = 0;

    while (length-- && (c = *in++)) {
        if (c == '%' && *in && *(in + 1)) {
            decode_buffer[2] = *in++;
            decode_buffer[3] = *in++;

            c = (char)strtol(decode_buffer, 0, 16);
            //c = 16 * (decode_buffer[2] - '0') + (decode_buffer[3] - '0');
        }
        else if (c == '+')
            c = ' ';

        result[i++] = c;
    }

    result[i] = 0;
    return result;
}

void parse_query(api_pool_t* pool, http_request_t* request)
{
    const char* buffer = request->uri.query;
    const char* start = buffer, *end = 0, *equation = 0;
    http_param_t* param = 0;

    while (*buffer)
    {
        end = start;
        equation = 0;

        while (*end && *end != '&')
        {
            if (!equation && *end == '=')
                equation = end;

            ++end;
        }

        if (end > start)
        {
            if (equation)
            {
                if (equation > start)
                {
                    param = (http_param_t*)api_alloc(pool, sizeof(*param));
                    param->name = decode_query(pool, start, equation - start);
                    param->value = 0;

                    if (equation < end - 1)
                    {
                        param->value = decode_query(pool, equation + 1,
                                                end - equation - 1);
                    }

                    param->next = request->params;
                    request->params = param;
                }
            }
            else
            {
                param = (http_param_t*)api_alloc(pool, sizeof(*param));
                param->name = decode_query(pool, start, end - start);
                param->value = 0;

                param->next = request->params;
                request->params = param;
            }

            if (*end)
                start = end + 1;
            else
                break;
        }
        else
        {
            break;
        }
    }
}

const char* http_request_parse(http_request_t* request, api_stream_t* stream)
{
    char buffer[1024];
    size_t nread;
    size_t size;
    http_parser_t ctx;
    http_parser parser;
    http_parser_settings settings;
    struct http_parser_url url;
    int r;
    const char* result = 0;

    memset(request, 0, sizeof(*request));
    memset(&ctx, 0, sizeof(ctx));
    ctx.request = request;
    ctx.headers_done = 0;
    parser.data = &ctx;
    ctx.pool = api_pool_default(stream->loop);

    settings.on_message_begin = http_on_message_begin_cb;
    settings.on_url = http_on_url_cb;
    settings.on_status_complete = http_on_status_complete_cb;
    settings.on_header_field = http_on_header_field_cb;
    settings.on_header_value = http_on_header_value_cb;
    settings.on_headers_complete = http_on_headers_complete_cb;
    settings.on_body = http_on_body_cb;
    settings.on_message_complete = http_on_message_complete_cb;

    http_parser_init(&parser, HTTP_REQUEST);

    while (1)
    {
        nread = api_stream_read(stream, buffer, 1024);
        if (nread == 0)
        {
            result = "stream not readable";
            break;
        }

        size = http_parser_execute(&parser, &settings, buffer, nread);
    
        if (parser.http_errno > 0)
        {
            result = 
                http_errno_description((enum http_errno)parser.http_errno);
            break;
        }

        if (ctx.headers_done)
        {
            if (size < nread)
                api_stream_unread(stream, buffer + size, nread - size);

            break;
        }
    }

    request->major = parser.http_major;
    request->minor = parser.http_minor;
    request->method = http_method_str((enum http_method)parser.method);

    if (result == 0)
    {
        memset(&url, 0, sizeof(url));
        r = http_parser_parse_url(request->url, strlen(request->url),
                                parser.method == HTTP_CONNECT, &url);

        if (r == 0)
        {
            if (url.field_set & (1 << UF_SCHEMA))
                request->uri.schema = str_ndup(ctx.pool,
                    request->url + url.field_data[UF_SCHEMA].off,
                    url.field_data[UF_SCHEMA].len);

            if (url.field_set & (1 << UF_HOST))
                request->uri.host = str_ndup(ctx.pool,
                    request->url + url.field_data[UF_HOST].off,
                    url.field_data[UF_HOST].len);

            if (url.field_set & (1 << UF_PATH))
                request->uri.path = decode_query(ctx.pool,
                    request->url + url.field_data[UF_PATH].off,
                    url.field_data[UF_PATH].len);

            if (url.field_set & (1 << UF_QUERY))
            {
                request->uri.query = str_ndup(ctx.pool,
                        request->url + url.field_data[UF_QUERY].off,
                        url.field_data[UF_QUERY].len);
                parse_query(ctx.pool, request);
            }

            if (url.field_set & (1 << UF_FRAGMENT))
                request->uri.fragment = str_ndup(ctx.pool,
                    request->url + url.field_data[UF_FRAGMENT].off,
                    url.field_data[UF_FRAGMENT].len);

            // parse cookies
        }
        else
        {
            result = "invalud uri";
        }
    }
    
    if (ctx.buffer.data != 0)
        api_free(ctx.pool, ctx.buffer.alloc, ctx.buffer.data);

    if (ctx.header != 0)
    {
        str_free(ctx.pool, ctx.header->name);
        str_free(ctx.pool, ctx.header->value);
        api_free(ctx.pool, sizeof(*ctx.header), ctx.header);
    }

    if (result != 0)
        http_request_clean(request, ctx.pool);

    return result;
}

void http_request_clean(http_request_t* request, api_pool_t* pool)
{
    http_header_t* header = request->headers;
    http_header_t* next;
    http_cookie_t* cookie = request->cookies;
    http_cookie_t* next_c;
    http_param_t* param = request->params;
    http_param_t* next_p;

    str_free(pool, request->url);
    str_free(pool, request->body);
    str_free(pool, request->uri.fragment);
    str_free(pool, request->uri.host);
    str_free(pool, request->uri.path);
    str_free(pool, request->uri.query);
    str_free(pool, request->uri.schema);

    while (header != 0)
    {
        next = header->next;

        str_free(pool, header->name);
        str_free(pool, header->value);
        api_free(pool, sizeof(*header), header);

        header = next;
    }

    while (cookie != 0)
    {
        next_c = cookie->next;
        
        str_free(pool, cookie->name);
        str_free(pool, cookie->value);
        str_free(pool, cookie->path);
        api_free(pool, sizeof(*cookie), cookie);

        cookie = next_c;
    }

    while (param != 0)
    {
        next_p = param->next;
        
        str_free(pool, param->name);
        str_free(pool, param->value);
        api_free(pool, sizeof(*param), param);

        param = next_p;
    }
}

const char* http_request_get_header(http_request_t* request, const char* name)
{
    http_header_t* header = request->headers;

    while (header != 0)
    {
        if (strcmp_nocase(header->name, name) == 0)
            return header->value;

        header = header->next;
    }

    return 0;
}