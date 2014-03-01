/*
 * 'hello world' http server
 */

#include <malloc.h>

#include "../../api/include/api.h"

#define RESPONSE \
  "HTTP/1.1 200 OK\r\n" \
  "Content-Type: text/plain\r\n" \
  "Content-Length: 12\r\n" \
  "\r\n" \
  "hello world\n"

void serve_connection(api_loop_t* loop, void* arg)
{
    api_tcp_t* tcp = (api_tcp_t*)arg;
    char buffer[1024];

    api_stream_attach(&tcp->stream, loop);

    if (0 != api_stream_read(&tcp->stream, buffer, 1024))
        api_stream_write(&tcp->stream, RESPONSE, sizeof(RESPONSE) - 1);

    api_stream_close(&tcp->stream);
    api_free(api_pool_default(loop), sizeof(api_tcp_t), tcp);
}

/* single threaded hello server */
void hello_server_st(api_loop_t* loop, void* arg)
{
    api_pool_t* pool = api_pool_default(loop);
    api_tcp_listener_t listener;
    api_tcp_t* tcp;
    int error;

    error = api_tcp_listen(&listener, loop, "0.0.0.0", 8080, 128);
    if (error != API_OK)
        return;

    tcp = (api_tcp_t*)malloc(sizeof(api_tcp_t));
    while (API_OK == api_tcp_accept(&listener, tcp))
    {
        api_loop_post(loop, serve_connection, tcp, 0);
        tcp = (api_tcp_t*)malloc(sizeof(api_tcp_t));
    }
    free(tcp);

    api_tcp_close(&listener);
}

/* multithreaded hello server */
void hello_server_mt(api_loop_t* loop, void* arg)
{
    int threads = 4;
    api_loop_t** loops;
    api_pool_t* pool = api_pool_default(loop);
    api_tcp_listener_t listener;
    api_tcp_t* tcp;
    int error;
    int i = 0;

    error = api_tcp_listen(&listener, loop, "0.0.0.0", 8080, 128);
    if (error != API_OK)
        return;

    // start worker loops
    loops = (api_loop_t**)malloc(threads * sizeof(api_loop_t*));
    for (i = 0; i < threads; ++i)
        api_loop_start(&loops[i]);

    i = 0;

    tcp = (api_tcp_t*)malloc(sizeof(api_tcp_t));
    while (API_OK == api_tcp_accept(&listener, tcp))
    {
        // round robin
        api_loop_post(loops[i], serve_connection, tcp, 0);
        i = (i + 1) % threads;

        tcp = (api_tcp_t*)malloc(sizeof(api_tcp_t));
    }
    free(tcp);

    api_tcp_close(&listener);
}

int main(int argc, char *argv[])
{
    // assign hello_server_st for single threaded
    api_loop_fn server = hello_server_mt;

    api_init();

    if (API_OK != api_loop_run(server, 0, 0))
    {
        return 1;
    }

    return 0;
}