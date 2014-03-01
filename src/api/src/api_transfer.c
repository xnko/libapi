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

#include "../include/api.h"
#include "api_task.h"
#include "api_list.h"

typedef struct api_buffer_t {
	struct api_buffer_t* next;
	struct api_buffer_t* prev;
	char*	buffer;
	size_t	used;
} buffer_node;

typedef struct api_transfer_t {
	api_stream_t* src;
	api_list_t buffers;
	api_task_t* reader;
	api_task_t* writer;
	size_t chunk_size;
	int read_done;
	int write_done;
} api_transfer_t;


void api_transfer_reader(api_loop_t* loop, void* arg)
{
	api_transfer_t* transfer = (api_transfer_t*)arg;
	api_pool_t* pool = api_pool_default(transfer->src->loop);
	api_buffer_t* buffer;
	size_t nread;

	transfer->reader = 0; // current task

	while (1)
	{
		buffer = (api_buffer_t*)api_alloc(pool, sizeof(*buffer));
		buffer->buffer = (char*)api_alloc(pool, transfer->chunk_size);

		nread = api_stream_read(transfer->src, buffer->buffer,
								transfer->chunk_size);

		if (nread == 0)
		{
			api_free(pool, transfer->chunk_size, buffer->buffer);
			api_free(pool, sizeof(*buffer), buffer);

			transfer->read_done = 1;
			break;
		}
		else
		{
			buffer->used = nread;
			api_list_push_tail(&transfer->buffers, (api_node_t*)buffer);
		}

		if (transfer->write_done)
			break;

		// async post wakeup
	}

	// async post wakeup
}

size_t api_stream_transfer(api_stream_t* src,
						   api_stream_t* dst,
						   size_t chunk_size)
{
	api_transfer_t transfer;
	size_t total;
	size_t wrote;
	int error;
	int failed = 0;
	api_buffer_t* buffer;
	api_pool_t* pool = api_pool_default(src->loop);

	memset(&transfer, 0, sizeof(transfer));

	transfer.writer = 0; // current task
	transfer.chunk_size = chunk_size;

	error = api_loop_post(src->loop, api_transfer_reader, &transfer);
	if (API__OK != error)
		return 0;

	while (1)
	{
		api_task_sleep(transfer.writer);

		buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
		while (buffer != 0)
		{
			wrote = api_stream_write(dst, buffer->buffer, buffer->used);

			api_free(pool, chunk_size, buffer->buffer);
			api_free(pool, sizeof(*buffer), buffer);

			if (buffer->used == wrote)
			{
				buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
			}
			else
			{
				failed = 1;
				transfer.write_done = 1;
				break;
			}
		}

		if (failed)
			break;

		total += wrote;
		if (transfer.read_done == 1 && transfer.buffers.head == 0)
			break;
	}

	api_task_delete(transfer.reader);

	buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
	while (buffer != 0)
	{
		api_free(pool, chunk_size, buffer->buffer);
		api_free(pool, sizeof(*buffer), buffer);

		buffer = (api_buffer_t*)api_list_pop_head(&transfer.buffers);
	}

	return total;
}