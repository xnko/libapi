/*
 * Single threaded static site server with http and https support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../api/include/api.h"
#include "../../http/include/http.h"
#include "../../ssl/include/ssl.h"

#if defined(__linux__)

#define strcmp_nocase strcasecmp

const char* ssl_cert_file = "/home/artak/www.api.com.cert";
const char* ssl_key_file = "/home/artak/www.api.com.key";
const char* site = "/home/artak/site";

#else

#pragma warning(disable: 4996)

#define strcmp_nocase _stricmp

const char* ssl_cert_file = "d:/shared/www.api.com.cert";
const char* ssl_key_file = "d:/shared/www.api.com.key";
const char* site = "d:/shared/site";

#endif

const int http_port = 8080;
const int https_port = 8081;

#define NOTFOUND "HTTP/1.1 404 Not Found\r\n"

ssl_session_t ssl_session;

/* get mime type from file name */
const char* get_mime_type(const char* name)
{
	size_t dot = strlen(name) - 1;

	while (dot >= 0 && name[dot] != '.')
		--dot;

	if (0 == strcmp_nocase(name + dot, ".html")) return "text/html";
	if (0 == strcmp_nocase(name + dot, ".css")) return "text/css";
	if (0 == strcmp_nocase(name + dot, ".js")) return "text/javascript";
	if (0 == strcmp_nocase(name + dot, ".jpg")) return "image/jpg";
	if (0 == strcmp_nocase(name + dot, ".png")) return "image/png";
	if (0 == strcmp_nocase(name + dot, ".gif")) return "image/gif";

	return "application/octet-stream";
}

/* send http status line and headers for particular file */
int send_headers(api_tcp_t* tcp, const char* path)
{
	api_stat_t stat;
	const char* mime = get_mime_type(path);
	char content_length[50];

	if (API_OK != api_fs_stat(path, &stat) || stat.size == 0)
	{
		api_stream_write(&tcp->stream, NOTFOUND, sizeof(NOTFOUND) - 1);
		return -1;
	}

	api_stream_write(&tcp->stream, "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Type: ", 64);
	api_stream_write(&tcp->stream, mime, strlen(mime));
	api_stream_write(&tcp->stream, "\r\nContent-Length: ", 18);

	sprintf(content_length, "%d", (int)stat.size);

	api_stream_write(&tcp->stream, content_length, strlen(content_length));
	api_stream_write(&tcp->stream, "\r\n\r\n", 4);

	return API_OK;
}

/* http(s) request handler */
void serve_file(api_loop_t* loop, void* arg)
{
	api_tcp_t* tcp = (api_tcp_t*)arg;
	http_request_t request;
	api_stream_t file;
	char path[1024];
	const char* connection;
	int keep_alive = 1;

	/* set up read/write timeouts to 10 second */
	tcp->stream.read_timeout = 10 * 1000;
	tcp->stream.write_timeout = 10 * 1000;

	/* http pipelining */
	while (keep_alive)
	{
		/* parse http request and validate */
		if (http_request_parse(&request, &tcp->stream))
			break;

		/* reset read timeout from keep alive timeout */
		tcp->stream.read_timeout = 10 * 1000;

		strcpy(path, site);
		strcat(path, request.uri.path);

		/* default page */
		if (0 == strcmp(request.uri.path, "/"))
			strcat(path, "index.html");

		/* send http status line and headers */
		if (API_OK != send_headers(tcp, path))
			break;

		/* open requested file as stream */
		if (API_OK != api_fs_open(&file, path))
			break;

		/* attach file stream to loop */
		if (API_OK != api_stream_attach(&file, loop))
		{
			api_stream_close(&file);
			break;
		}

		/* transfer from file to http client */
		api_stream_transfer(&tcp->stream, &file, 500 * 1024, 0);

		api_stream_close(&file);

		/* keep connection for next request ? */
		connection = http_request_get_header(&request, "Connection");
		keep_alive = connection != 0 &&  0 == strcmp_nocase("Keep-Alive", connection);

		/* free memory */
		http_request_clean(&request, api_pool_default(tcp->stream.loop));

		/* set up keep alive period to 30 second */
		tcp->stream.read_timeout = 30 * 1000;
	}
}

/* http connection handler */
void http_connection(api_loop_t* loop, void* arg)
{
	api_tcp_t* tcp = (api_tcp_t*)arg;

	/* attach tcp connection to loop */
	api_stream_attach(&tcp->stream, loop);

	serve_file(loop, tcp);

	/* close tcp connection and free memory */
	api_stream_close(&tcp->stream);
	api_free(api_pool_default(loop), sizeof(api_tcp_t), tcp);
}

/* https connection handler */
void https_connection(api_loop_t* loop, void* arg)
{
	api_tcp_t* tcp = (api_tcp_t*)arg;
	ssl_stream_t https;

	/* attach tcp connection to loop */
	api_stream_attach(&tcp->stream, loop);

	/* attach ssl filter to tcp connection and do ssl accept handshake */
	ssl_stream_accept(&ssl_session, &https, &tcp->stream);

	serve_file(loop, tcp);

	/* detach ssl filter, close tcp connection and free memory */
	ssl_stream_detach(&https);
	api_stream_close(&tcp->stream);
	api_free(api_pool_default(loop), sizeof(api_tcp_t), tcp);
}

void http_server(api_loop_t* loop, void* arg)
{
	api_pool_t* pool = api_pool_default(loop);
	api_tcp_listener_t listener;
	api_tcp_t* tcp;

	/* listen for tcp connections */
	if (API_OK != api_tcp_listen(&listener, loop, "0.0.0.0", http_port, 128))
		return;

	tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
	/* accept tcp connection */
	while (API_OK == api_tcp_accept(&listener, tcp))
	{
		/* for each accepted connection handle it in same loop,
			http_connection needs more stack, so call it with larger stack */
		api_loop_post(loop, http_connection, tcp, 100 * 1024);

		tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
	}
	api_free(pool, sizeof(api_tcp_t), tcp);

	/* shutdown tcp listener */
	api_tcp_close(&listener);
}

void https_server(api_loop_t* loop, void* arg)
{
	api_pool_t* pool = api_pool_default(loop);
	api_tcp_listener_t listener;
	api_tcp_t* tcp;

	/* listen for tcp connections */
	if (API_OK != api_tcp_listen(&listener, loop, "0.0.0.0", https_port, 128))
		return;

	/* initialise ssl library */
	ssl_init();

	/* initialize ssl session */
	ssl_session_start(&ssl_session, ssl_cert_file, ssl_key_file);

	tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
	/* accept tcp connection */
	while (API_OK == api_tcp_accept(&listener, tcp))
	{
		/* for each accepted connection handle it in same loop,
			https_connection needs more stack, so call it with larger stack */
		api_loop_post(loop, https_connection, tcp, 100 * 1024);

		tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
	}
	api_free(pool, sizeof(api_tcp_t), tcp);

	/* shutdown tcp listener */
	api_tcp_close(&listener);

	/* shutdown ssl session */
	ssl_session_stop(&ssl_session);
}

void web_server(api_loop_t* loop, void* arg)
{
	/* start http server */
	api_loop_post(loop, http_server, 0, 0);

	/* start https server in same loop */
	api_loop_post(loop, https_server, 0, 0);
}

int main(int argc, char *argv[])
{
	/* initialize api library */
	api_init();

	/* convert current thread to loop and run web_server */
	if (API_OK != api_loop_run(web_server, 0, 0))
		return 1;

	return 0;
}