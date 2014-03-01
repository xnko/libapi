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

#include <sys/stat.h>

#include "../../include/api.h"
#include "api_error.h"

int api_fs_stat(const char* path, api_stat_t* stat)
{
    struct _stat64i32 s;
    _stat64i32(path, &s);

    stat->date_access = s.st_atime;
    stat->date_create = s.st_ctime;
    stat->date_modified = s.st_mtime;
    stat->size = s.st_size;

    return API_OK;
}

int api_fs_create(api_stream_t* stream, const char* path)
{
    HANDLE fd = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ
                            | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                            CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
    if (fd == INVALID_HANDLE_VALUE)
        return api_error_translate(GetLastError());

    api_stream_init(stream, STREAM_File, (fd_t)fd);

    return API__OK;
}

int api_fs_open(api_stream_t* stream, const char* path)
{
    HANDLE fd = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ
                            | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (fd == INVALID_HANDLE_VALUE)
        return api_error_translate(GetLastError());

    api_stream_init(stream, STREAM_File, (fd_t)fd);
	
    return API__OK;
}

int api_fs_enum(api_fs_enum_t* options)
{
    return 0;
}