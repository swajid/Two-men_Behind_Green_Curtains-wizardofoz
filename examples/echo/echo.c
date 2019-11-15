#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>


#include "event.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "logging.h"
    
event_loop_t loop;
event_sock_t *server;

void on_server_close(event_sock_t *handle) {
    (void)handle;
    INFO("Server closed");
    exit(0);
}

void close_server(int sig) {
    (void)sig;
    event_close(server, on_server_close);
}

void on_client_close(event_sock_t *handle) {
    (void)handle;
    INFO("Client closed");
}

void echo_write(event_sock_t *client, int status) {
    (void)client;
    if (status < 0) {
        ERROR("Failed to write");
    }
}

int echo_read(event_sock_t *client, ssize_t nread, uint8_t *buf)
{
    if (nread > 0) {
        DEBUG("Read '%.*s'", (int)nread - 1, buf);
        event_write(client, nread, buf, echo_write);
        
        return nread;
    }

    INFO("Remote connection closed");
    event_close(client, on_client_close);

    return 0;
}

void on_new_connection(event_sock_t *server, int status)
{
    if (status < 0) {
        ERROR("Connection error");
        // error!
        return;
    }

    INFO("New client connection");
    event_sock_t *client = event_sock_create(server->loop);
    if (event_accept(server, client) == 0) {
        event_read(client, echo_read);
    }
    else {
        event_close(client, on_client_close);
    }
}

int main()
{
    event_loop_init(&loop);
    server = event_sock_create(&loop);

    signal(SIGINT, close_server);

    int r = event_listen(server, 8888, on_new_connection);
    if (r < 0) {
        ERROR("Could not start server");
        return 1;
    }
    event_loop(&loop);
}