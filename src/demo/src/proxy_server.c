/*
 * Simple raw proxy server implementation
 */

#include "../../api/include/api.h"

const char* listen_ip = "0.0.0.0";
int listen_port = 8085;

const char* backend_ip = "217.69.139.201";
int backend_port = 80;

typedef struct proxy_t {
    api_event_t ready;
    api_tcp_t* client;
    api_tcp_t* server;
} proxy_t;

void transfer_request(api_loop_t* loop, void* arg)
{
    proxy_t* proxy = (proxy_t*)arg;

    api_stream_transfer(
        &proxy->server->stream, /* destination */
        &proxy->client->stream, /* source */
        100 * 1024,  /* chunk size */
        0  /* we dont need bytes transferred */
        );

    api_event_signal(&proxy->ready, loop);
}

void on_client(api_loop_t* loop, void* arg)
{
    api_tcp_t* client = (api_tcp_t*)arg;
    api_tcp_t server;
    proxy_t proxy;

    api_stream_attach(&client->stream, loop);

    client->stream.read_timeout = 10 * 1000;
    client->stream.write_timeout = 10 * 1000;

    if (API_OK == api_tcp_connect(&server, loop, backend_ip, backend_port, 10 * 1000))
    {
        server.stream.read_timeout = 10 * 1000;
        server.stream.write_timeout = 10 * 1000;

        api_event_init(&proxy.ready, loop);
        
        proxy.client = client;
        proxy.server = &server;

        // transfer request in parallel
        api_loop_post(loop, transfer_request, &proxy, 0);

        // transfer response
        api_stream_transfer(&client->stream, &server.stream, 100 * 1024, 0);

        api_event_wait(&proxy.ready, 0);

        api_stream_close(&server.stream);
    }

    api_stream_close(&client->stream);
    api_free(api_pool_default(loop), sizeof(api_tcp_t), client);
}

void proxy_server(api_loop_t* loop, void* arg)
{
    api_pool_t* pool = api_pool_default(loop);
    api_tcp_listener_t listener;
    api_tcp_t* tcp;

    if (API_OK != api_tcp_listen(&listener, loop, listen_ip, listen_port, 128))
        return;

    tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
    while (API_OK == api_tcp_accept(&listener, tcp))
    {
        api_loop_post(loop, on_client, tcp, 50 * 1024);

        tcp = (api_tcp_t*)api_alloc(pool, sizeof(api_tcp_t));
    }
    api_free(pool, sizeof(api_tcp_t), tcp);

    api_tcp_close(&listener);
}

int main(int argc, char *argv[])
{
    api_init();

    if (API_OK != api_loop_run(proxy_server, 0, 0))
        return 1;

    return 0;
}