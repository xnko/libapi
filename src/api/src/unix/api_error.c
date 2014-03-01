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

#include "api_error.h"

int api_error_translate(int error)
{

	switch (error) {
	case 0:			return API__OK;
	case EPERM:		return API__NOT_PERMITTED;
	case 2:			return API__NOT_FOUND;
	case EIO:		return API__IO_ERROR;
	case EBADF:		return API__BAD_FILE;
	case EAGAIN:	return API__TEMPORARY_UNAVAILABLE;
	case ENOMEM:	return API__NO_MEMORY;
	case EACCES:	return API__ACCESS_DENIED;
	case EFAULT:	return API__FAULT;
	case EEXIST:	return API__ALREADY_EXIST;
	case ENODEV:	return API__NO_DEVICE;
	case EINVAL:	return API__INVALID_ARGUMENT;
	case ENFILE:	return API__LIMIT;
	case EMFILE:	return API__TOO_MANY_FILES;
	case ENOTTY:	return API__NOT_TYPEWRITER;
	case ENOSPC:	return API__NO_SPACE;
	case EADDRINUSE:return API__ADDRESS_IN_USE;
	}

	return error;
}