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

#ifndef API_ERRNO_H_INCLUDED
#define API_ERRNO_H_INCLUDED

#include <errno.h>

#define API__OK                     0
#define API__NOT_PERMITTED          EPERM
#define	API__NOT_FOUND              2
#define	API__IO_ERROR               EIO
#define	API__BAD_FILE               EBADF
#define	API__TEMPORARY_UNAVAILABLE  EAGAIN
#define	API__NO_MEMORY              ENOMEM
#define	API__ACCESS_DENIED          EACCES
#define	API__FAULT                  EFAULT
#define	API__ALREADY_EXIST          EEXIST
#define	API__NO_DEVICE              ENODEV
#define	API__INVALID_ARGUMENT       EINVAL
#define	API__LIMIT                  ENFILE
#define	API__TOO_MANY_FILES         EMFILE
#define	API__NOT_TYPEWRITER         ENOTTY
#define	API__NO_SPACE               ENOSPC
#define	API__ADDRESS_IN_USE         EADDRINUSE
#define	API__TIMEDOUT               ETIMEDOUT

#define	API__TERMINATE              10000 /* not sure */

#endif // API_ERRNO_H_INCLUDED