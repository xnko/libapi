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

#include "../include/ssl.h"
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>

int ssl_is_fatal_error(int ssl_error)
{
	switch(ssl_error) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			return 0;
	}

	return 1;
}

size_t ssl_on_read(struct api_filter_t* filter, char* buffer, size_t length)
{
	ssl_stream_t* ssl_stream = (ssl_stream_t*)filter;
	size_t done = 0;
	int error;
	int bytes = 0;

	bytes = SSL_read((SSL*)ssl_stream->ssl, buffer, length);
	if (bytes > 0)
		return bytes;
	else
	{
		error = SSL_get_error((SSL*)ssl_stream->ssl, bytes);
		if (ssl_is_fatal_error(error))
		{
			ssl_stream->ssl_error = error;
			return 0;
		}
	}

	done = filter->next->on_read(filter->next, buffer, length);
	if (done == 0)
		return 0;

	error = BIO_write((BIO*)ssl_stream->bio_read, buffer, done);
	if (error <= 0)
	{
		ssl_stream->ssl_error = error;
		return 0;
	}

	bytes = SSL_read((SSL*)ssl_stream->ssl, buffer, length);
	if (bytes > 0)
		return bytes;
		
	return 0;
}

size_t ssl_on_write(struct api_filter_t* filter, const char* buffer, size_t length)
{
	char buf[1024];
	int bytes_read = 0;
	ssl_stream_t* ssl_stream = (ssl_stream_t*)filter;

	int r = SSL_write((SSL*)ssl_stream->ssl, buffer, length);
	if (r <= 0)
	{
		ssl_stream->ssl_error = SSL_get_error((SSL*)ssl_stream->ssl, r);
		return 0;
	}

	while ((bytes_read = BIO_read((BIO*)ssl_stream->bio_write, 
									buf, sizeof(buf))) > 0)
	{
		if (filter->next->on_write(filter->next, buf, bytes_read) == 0)
			return 0;
	}

	return length;
}

void ssl_init()
{
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
}

int ssl_get_error_description(unsigned long error, char* buffer, size_t length)
{
	if (SSL_ERROR_NONE == error)
		error = ERR_get_error();

	if (SSL_ERROR_NONE == error)
		return 0;

	ERR_error_string_n(error, buffer, length);
	return 1;
}

int ssl_session_start(ssl_session_t* session, 
					  const char* cert_file, const char* key_file)
{
	int rc;
	SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());
	session->ctx = 0;

	rc = SSL_CTX_use_certificate_chain_file(ctx, cert_file);
	if (rc != 1)
	{
		SSL_CTX_free(ctx);
		return 0;
	}

	rc = SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM);
	if (!rc) {
		SSL_CTX_free(ctx);
		return 0;
	}

	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

	session->ctx = ctx;

	return 1;
}

void ssl_session_stop(ssl_session_t* session)
{
	SSL_CTX_free((SSL_CTX*)session->ctx);
}

int ssl_stream_accept(ssl_session_t* session,
					  ssl_stream_t* ssl_stream, api_stream_t* stream)
{
	api_loop_t* loop = stream->loop;
	char buffer[512];
	size_t length = 0;
	int bytes;
	int error;

	if (loop == 0)
		return -1;

	memset(ssl_stream, 0, sizeof(*ssl_stream));

	ssl_stream->stream = stream;
	ssl_stream->session = session;

	ssl_stream->ssl = SSL_new((SSL_CTX*)session->ctx);
	SSL_set_accept_state((SSL*)ssl_stream->ssl);

	ssl_stream->bio_read = BIO_new(BIO_s_mem());
	ssl_stream->bio_write = BIO_new(BIO_s_mem());
	SSL_set_bio((SSL*)ssl_stream->ssl, (BIO*)ssl_stream->bio_read,
									(BIO*)ssl_stream->bio_write);

	// accept

	do
	{
		length = api_stream_read(stream, buffer, 512);
		if (length == 0)
		{
			return 0;
		}

		error = BIO_write((BIO*)ssl_stream->bio_read, buffer, length);
		if (error <= 0)
		{
			ssl_stream->ssl_error = error;
			return 0;
		}


		bytes = SSL_read((SSL*)ssl_stream->ssl, buffer, length);
		if (bytes <= 0)
		{
			error = SSL_get_error((SSL*)ssl_stream->ssl, bytes);
			if (ssl_is_fatal_error(error))
			{
				ssl_stream->ssl_error = error;
				return 0;
			}
		}

		if (bytes > 0)
		{
			api_stream_unread(stream, buffer, bytes);
		}

		while (BIO_pending((BIO*)ssl_stream->bio_write))
		{
			bytes = BIO_read((BIO*)ssl_stream->bio_write, buffer, 512);

			if (bytes > 0)
			{
				if (api_stream_write(ssl_stream->stream, buffer, bytes) == 0)
					return 0;
			}
			else
				if (bytes < 0)
				{
					ssl_stream->ssl_error = bytes;
					return 0;
				}
				else
					break;
		}

		if (SSL_is_init_finished((SSL*)ssl_stream->ssl))
			break;
	}
	while (1);

	api_filter_attach(&ssl_stream->filter, stream);
	ssl_stream->filter.on_read = ssl_on_read;
	ssl_stream->filter.on_write = ssl_on_write;
	ssl_stream->attached = 1;

	return 1;
}

void ssl_stream_detach(ssl_stream_t* ssl_stream)
{
	if (ssl_stream->attached)
		api_filter_detach(&ssl_stream->filter, ssl_stream->stream);

	SSL_shutdown((SSL*)ssl_stream->ssl);
	SSL_free((SSL*)ssl_stream->ssl);
}