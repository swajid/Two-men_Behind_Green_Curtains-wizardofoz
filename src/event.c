#include <errno.h>

#ifdef CONTIKI
#include "contiki-net.h"
#include "lib/assert.h"
#else
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "event.h"
#include "config.h"

#define LOG_MODULE LOG_MODULE_EVENT

#include "logging.h"

#ifdef CONTIKI
// Main contiki process
PROCESS(event_loop_process, "Event loop process");
#endif

/////////////////////////////////////////////////////
// Private methods
/////////////////////////////////////////////////////

// find socket file descriptor in socket queue
event_handler_t *event_handler_find(event_handler_t *queue, event_type_t type)
{
    return LL_FIND(queue, LL_ELEM(event_handler_t)->type == type);
}

// find free handler in loop memory
event_handler_t *event_handler_find_free(event_loop_t *loop, event_sock_t *sock)
{
    return LL_MOVE(event_handler_t, loop->handlers, sock->handlers);
}

// find socket file descriptor in socket queue
event_sock_t *event_sock_find(event_sock_t *queue, event_descriptor_t descriptor)
{
    return LL_FIND(queue, LL_ELEM(event_sock_t)->descriptor == descriptor);
}

void event_sock_connect(event_sock_t *sock, event_handler_t *handler)
{
    if (handler != NULL) {
        // if this happens there is an error with the implementation
        assert(handler->event.connection.cb != NULL);
        if (sock->loop->sockets != NULL) {
            handler->event.connection.cb(sock, 0);
        }
    }
}

void event_sock_close(event_sock_t *sock, int status)
{
    assert(status <= 0);
    event_handler_t *rh = event_handler_find(sock->handlers, EVENT_READ_TYPE);
    if (rh != NULL) {
        // mark the buffer as closed
        cbuf_end(&rh->event.read.buf);

        // notify the read handler
        rh->event.read.cb(sock, status, NULL);
    }

    event_handler_t *wh = event_handler_find(sock->handlers, EVENT_WRITE_TYPE);
    if (wh != NULL) {
        // mark the buffer as closed
        cbuf_end(&wh->event.write.buf);

        // empty the buffer
        cbuf_pop(&wh->event.write.buf, NULL, cbuf_len(&wh->event.write.buf));

        event_write_op_t *op = LL_POP(wh->event.write.ops);
        // notify of error if we could not notify the read
        if (rh == NULL && op != NULL) {
            op->cb(sock, status);

        }

        // remove all pending operations
        while (op != NULL) {
            LL_PUSH(op, sock->loop->write_ops);
            op = LL_POP(wh->event.write.ops);
        }
    }
}

void event_sock_read(event_sock_t *sock, event_handler_t *handler)
{
    assert(sock != NULL);
    if (handler == NULL) {
        return;
    }

    // check available buffer data
    int readlen = cbuf_maxlen(&handler->event.read.buf) - cbuf_len(&handler->event.read.buf);
    uint8_t buf[readlen];

#ifdef CONTIKI
    int count = MIN(readlen, uip_datalen());
    memcpy(buf, uip_appdata, count);
#else
    // perform read in non blocking manner
    int count = recv(sock->descriptor, buf, readlen, MSG_DONTWAIT);
    if ((count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) || count == 0) {
        event_sock_close(sock, count == 0 ? 0 : -errno);
        return;
    }

    // reset timer if any
    event_handler_t *timeout_handler = event_handler_find(sock->handlers, EVENT_TIMEOUT_TYPE);
    if (timeout_handler != NULL) {
        gettimeofday(&timeout_handler->event.timer.start, NULL);
    }
#endif
    DEBUG("received %d bytes from remote endpoint", count);
    // push data into buffer
    cbuf_push(&handler->event.read.buf, buf, count);
}

void event_sock_handle_read(event_sock_t *sock, event_handler_t *handler)
{
    assert(sock != NULL);
    if (handler == NULL) {
        return;
    }

    int buflen = cbuf_len(&handler->event.read.buf);
    if (buflen > 0) {
        // else notify about the new data
        uint8_t read_buf[buflen];
        cbuf_peek(&handler->event.read.buf, read_buf, buflen);

        DEBUG("passing %d bytes to callback", buflen);
        int readlen = handler->event.read.cb(sock, buflen, read_buf);
        DEBUG("callback used %d bytes", readlen);

        // remove the read bytes from the buffer
        cbuf_pop(&handler->event.read.buf, NULL, readlen);
    }

    if (cbuf_has_ended(&handler->event.read.buf)) {
        // if a read terminated call the callback with -1
        handler->event.read.cb(sock, -1, NULL);
    }
}

void event_sock_handle_write(event_sock_t *sock, event_handler_t *handler, unsigned int written)
{
    // notify the waiting write operatinons
    event_write_op_t *op = LL_POP(handler->event.write.ops);

    while (op != NULL && written > 0) {
        if (op->bytes - written <= 0) {
            written -= op->bytes;

            // notify the callback
            op->cb(sock, 0);

            // return the operation to
            // the loop
            LL_PUSH(op, sock->loop->write_ops);
        }
        else {
            // if not enough bytes have been written yet
            // reduce the size and pop the operation back to
            // the list
            op->bytes -= written;
            LL_PUSH(op, handler->event.write.ops);
            return;
        }
        op = LL_POP(handler->event.write.ops);
    }
}

void event_sock_write(event_sock_t *sock, event_handler_t *handler)
{
    assert(sock != NULL);
    if (handler == NULL) {
        return;
    }

    // attempt to write remaining bytes
#ifdef CONTIKI
    // do nothing if already sending
    if (handler->event.write.sending > 0) {
        return;
    }

    int len = MIN(cbuf_len(&handler->event.write.buf), uip_mss());
#else
    int len = cbuf_len(&handler->event.write.buf);
#endif
    uint8_t buf[len];
    cbuf_peek(&handler->event.write.buf, buf, len);

#ifdef CONTIKI
    // Send data
    uip_send(buf, len);

    // set sending length
    handler->event.write.sending = len;
#else
    int written = send(sock->descriptor, buf, len, MSG_DONTWAIT);
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        event_sock_close(sock, -errno);
        return;
    }

    // reset timer if any
    event_handler_t *timeout_handler = event_handler_find(sock->handlers, EVENT_TIMEOUT_TYPE);
    if (timeout_handler != NULL) {
        gettimeofday(&timeout_handler->event.timer.start, NULL);
    }

    // remove written data from buffer
    cbuf_pop(&handler->event.write.buf, NULL, written);

    // notify waiting callbacks
    event_sock_handle_write(sock, handler, written);

    #endif
}

#ifndef CONTIKI
void event_loop_timers(event_loop_t *loop)
{
    // get current time
    struct timeval now;

    gettimeofday(&now, NULL);

    // for each socket and handler, check if elapsed time has been reached
    for (event_sock_t *sock = loop->polling; sock != NULL; sock = sock->next) {
        event_handler_t *handler = event_handler_find(sock->handlers, EVENT_TIMEOUT_TYPE);
        if (sock->state != EVENT_SOCK_CONNECTED || handler == NULL) {
            continue;
        }

        struct timeval diff;
        timersub(&now, &handler->event.timer.start, &diff);
        // time has finished
        if ((diff.tv_sec * 1000 + diff.tv_usec / 1000) >= handler->event.timer.millis) {
            // run the callback and reset timer
            int remove = handler->event.timer.cb(sock);
            handler->event.timer.start = now;
            if (remove > 0) {
                // remove handler from list
                LL_DELETE(handler, sock->handlers);

                // move handler to loop unused list
                LL_PUSH(handler, sock->loop->handlers);
            }
        }
    }
}

void event_loop_poll(event_loop_t *loop, int millis)
{
    assert(loop->nfds >= 0);

    fd_set read_fds = loop->active_fds;
    fd_set write_fds = loop->active_fds;
    struct timeval tv = { 0, millis * 1000 };

    // poll list of file descriptors with select
    if (select(loop->nfds, &read_fds, &write_fds, NULL, &tv) < 0) {
        // if an error different from interrupt is caught
        // here, there is an issue with the implementation
        assert(errno == EINTR);
        return;
    }

    for (int i = 0; i < loop->nfds; i++) {
        event_sock_t *sock;
        if (FD_ISSET(i, &loop->active_fds)) {
            sock = event_sock_find(loop->polling, i);
        } else {
            continue;
        }

        // check read operations
        if (FD_ISSET(i, &read_fds)) {
            if (sock->state == EVENT_SOCK_CONNECTED) {
                // read into sock buffer if any
                event_sock_read(sock, event_handler_find(sock->handlers, EVENT_READ_TYPE));
            }
            else {
                event_sock_connect(sock, event_handler_find(sock->handlers, EVENT_CONNECTION_TYPE));
            }
        }

        // check write operations
        if (FD_ISSET(i, &write_fds)) {
            // write from buffer if possible
            event_sock_write(sock, event_handler_find(sock->handlers, EVENT_WRITE_TYPE));
        }
    }
}

void event_loop_pending(event_loop_t *loop)
{
    for (event_sock_t *sock = loop->polling; sock != NULL; sock = sock->next) {
        event_handler_t *handler = event_handler_find(sock->handlers, EVENT_READ_TYPE);
        if (handler != NULL) {
            event_sock_handle_read(sock, handler);
        }
    }
}
#else
void event_sock_handle_timeout(void *data)
{
    event_handler_t *handler = (event_handler_t *)data;
    event_sock_t *sock = handler->event.timer.sock;

    // run the callback
    int remove = handler->event.timer.cb(sock);

    if (remove > 0) {
        // stop ctimer
        ctimer_stop(&handler->event.timer.ctimer);

        // remove handler from list
        LL_DELETE(handler, sock->handlers);

        // move handler to loop unused list
        LL_PUSH(handler, sock->loop->handlers);
    }
    else {
        ctimer_restart(&handler->event.timer.ctimer);
    }
}

void event_sock_handle_ack(event_sock_t *sock, event_handler_t *handler)
{
    if (handler == NULL || handler->event.write.sending == 0) {
        return;
    }

    // remove sent data from output buffer
    cbuf_pop(&handler->event.write.buf, NULL, handler->event.write.sending);

    // notify the operations of successful write
    event_sock_handle_write(sock, handler, handler->event.write.sending);

    // update sending size
    handler->event.write.sending = 0;
}

void event_sock_handle_rexmit(event_sock_t *sock, event_handler_t *handler)
{
    if (handler == NULL || handler->event.write.sending == 0) {
        return;
    }

    // reset send counter
    handler->event.write.sending = 0;
}

void event_sock_handle_event(event_loop_t *loop, void *data)
{
    event_sock_t *sock = (event_sock_t *)data;

    if (sock != NULL && sock->uip_conn != NULL && sock->uip_conn != uip_conn) {
        /* Safe-guard: this should not happen, as the incoming event relates to
         * a previous connection */
        DEBUG("If you are reading this, there is an implementation error");
        return;
    }

    if (uip_connected()) {
        if (sock == NULL) { // new client connection
            sock = LL_FIND(loop->polling, \
                             LL_ELEM(event_sock_t)->state == EVENT_SOCK_LISTENING && \
                             UIP_HTONS(LL_ELEM(event_sock_t)->descriptor) == uip_conn->lport);

            // Save the connection for when
            // accept is called
            sock->uip_conn = uip_conn;

            // do connect
            event_sock_connect(sock, event_handler_find(sock->handlers, EVENT_CONNECTION_TYPE));
        }

        if (sock == NULL) { // no one waiting for the socket
            uip_abort();
        }
        else {
            if (uip_newdata()) {
                WARN("read new data but there is no socket ready to receive it yet");
            }
        }
        return;
    }

    // no socket for the connection
    // probably it was remotely closed
    if (sock == NULL) {
        uip_abort();
        return;
    }

    if (uip_timedout() || uip_aborted() || uip_closed()) { // Remote connection closed or timed out
        // perform close operations
        event_sock_close(sock, -1);

        // Finish connection
        uip_close();

        return;
    }

    event_handler_t *wh = event_handler_find(sock->handlers, EVENT_WRITE_TYPE);
    if (uip_acked()) {
        event_sock_handle_ack(sock, wh);
    }

    if (uip_rexmit()) {
        event_sock_handle_rexmit(sock, wh);
    }

    event_handler_t *rh = event_handler_find(sock->handlers, EVENT_READ_TYPE);
    if (uip_newdata()) {
        event_sock_read(sock, rh);
    }

    // Handle pending reads and writes if available
    // if it is a poll event, it will jump to here
    event_sock_handle_read(sock, rh);

    // Try to write each time
    event_sock_write(sock, wh);

    // reset timer if any activity was detected from the client side
    event_handler_t *timeout_handler = event_handler_find(sock->handlers, EVENT_TIMEOUT_TYPE);
    if (!uip_poll() && timeout_handler != NULL) {
        ctimer_restart(&timeout_handler->event.timer.ctimer);
    }

    // Close connection cleanly
    if (sock->state == EVENT_SOCK_CLOSING && wh != NULL && cbuf_len(&wh->event.write.buf) <= 0) {
        uip_close();
    }
}
#endif // CONTIKI

void event_loop_close(event_loop_t *loop)
{
    event_sock_t *curr = loop->polling;
    event_sock_t *prev = NULL;

#ifndef CONTIKI
    int max_fds = -1;
#endif

    while (curr != NULL) {
        // if the event is closing and we are not waiting to write
        event_handler_t *wh = event_handler_find(curr->handlers, EVENT_WRITE_TYPE);
        if (curr->state == EVENT_SOCK_CLOSING && (wh == NULL || wh->event.write.ops == NULL)) {
#ifndef CONTIKI
            // prevent sock to be used in polling
            FD_CLR(curr->descriptor, &loop->active_fds);

            // notify the client of socket closing
            shutdown(curr->descriptor, SHUT_WR);

            // close the socket and update its status
            close(curr->descriptor);
#else
            // tcp_markconn()
            if (curr->uip_conn != NULL) {
                tcp_markconn(curr->uip_conn, NULL);
            }

            // stop timer if any
            event_handler_t *timeout_handler = event_handler_find(curr->handlers, EVENT_TIMEOUT_TYPE);
            if (timeout_handler != NULL) {
                ctimer_stop(&timeout_handler->event.timer.ctimer);
            }

            event_handler_t *ch = event_handler_find(curr->handlers, EVENT_CONNECTION_TYPE);
            if (ch != NULL) {
                // If server socket
                // let UIP know that we are no longer accepting connections on the specified port
                PROCESS_CONTEXT_BEGIN(&event_loop_process);
                tcp_unlisten(UIP_HTONS(curr->descriptor));
                PROCESS_CONTEXT_END();
            }
#endif
            curr->state = EVENT_SOCK_CLOSED;

            // call the close callback
            curr->close_cb(curr);

            // Move curr socket to unused list
            event_sock_t *next = LL_PUSH(curr, loop->sockets);

            // remove the socket from the polling list
            // and move the socket back to the unused list
            if (prev == NULL) {
                loop->polling = next;
            }
            else {
                prev->next = next;
            }

            // move socket handlers back to the unused handler list
            for (event_handler_t *h = curr->handlers; h != NULL; h = LL_PUSH(h, loop->handlers)) {}

            // move the head forward
            curr = next;
            continue;
        }

#ifndef CONTIKI
        if (curr->descriptor > max_fds) {
            max_fds = curr->descriptor;
        }
#endif

        prev = curr;
        curr = curr->next;
    }

#ifndef CONTIKI
    // update the max file descriptor
    loop->nfds = max_fds + 1;
#endif
}

int event_loop_is_alive(event_loop_t *loop)
{
    return loop->polling != NULL;
}

// Public methods
int event_listen(event_sock_t *sock, uint16_t port, event_connection_cb cb)
{
    assert(sock != NULL);
    assert(cb != NULL);
    assert(sock->loop != NULL);
    assert(sock->state == EVENT_SOCK_CLOSED);

    event_loop_t *loop = sock->loop;

#ifndef CONTIKI
    sock->descriptor = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock->descriptor < 0) {
        return -1;
    }

    // allow address reuse to prevent "address already in use" errors
    int option = 1;
    setsockopt(sock->descriptor, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    /* Struct sockaddr_in6 needed for binding. Family defined for ipv6. */
    struct sockaddr_in6 sin6;
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    sin6.sin6_addr = in6addr_any;

    if (bind(sock->descriptor, (struct sockaddr *)&sin6, sizeof(sin6)) < 0) {
        close(sock->descriptor);
        return -1;
    }

    if (listen(sock->descriptor, EVENT_MAX_SOCKETS - 1) < 0) {
        close(sock->descriptor);
        return -1;
    }

    // update max fds value
    loop->nfds = sock->descriptor + 1;

    // add socket fd to read watchlist
    FD_SET(sock->descriptor, &loop->active_fds);
#else
    // use port as the socket descriptor
    sock->descriptor = port;

    // Let know UIP that we are accepting connections on the specified port
    PROCESS_CONTEXT_BEGIN(&event_loop_process);
    tcp_listen(UIP_HTONS(port));
    PROCESS_CONTEXT_END();
#endif // CONTIKI

    // update sock state
    sock->state = EVENT_SOCK_LISTENING;

    // add socket to polling queue
    LL_PUSH(sock, loop->polling);

    // add event handler to socket
    event_handler_t *handler = LL_MOVE(event_handler_t, loop->handlers, sock->handlers);
    assert(handler != NULL);

    // set handler event
    handler->type = EVENT_CONNECTION_TYPE;
    memset(&handler->event.connection, 0, sizeof(event_connection_t));
    handler->event.connection.cb = cb;

    return 0;
}

void event_read_start(event_sock_t *sock, uint8_t *buf, unsigned int bufsize, event_read_cb cb)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(buf != NULL);
    assert(cb != NULL);

    // read can only be performed on connected sockets
    assert(sock->state == EVENT_SOCK_CONNECTED);

    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_READ_TYPE);
    assert(handler == NULL); // read stop must be called before call to event_read_start

    // find free handler
    event_loop_t *loop = sock->loop;
    handler = event_handler_find_free(loop, sock);
    assert(handler != NULL); // should we return -1 instead?

    // set handler event
    handler->type = EVENT_READ_TYPE;
    memset(&handler->event.read, 0, sizeof(event_read_t));

    // initialize read buffer
    cbuf_init(&handler->event.read.buf, buf, bufsize);

    // set read callback
    handler->event.read.cb = cb;
}

int event_read(event_sock_t *sock, event_read_cb cb)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(cb != NULL);

    // read can only be performed on connected sockets
    assert(sock->state == EVENT_SOCK_CONNECTED);

    // see if there is already a read handler set
    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_READ_TYPE);
    assert(handler != NULL); // event_read_start() must be called before

    // Update read callback
    handler->event.read.cb = cb;

    return 0;
}

int event_read_stop(event_sock_t *sock)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);

    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_READ_TYPE);
    if (handler == NULL) {
        return 0;
    }

    // remove read handler from list
    LL_DELETE(handler, sock->handlers);

    // reset read callback
    handler->event.read.cb = NULL;

    // Move data to the beginning of the buffer
    int len = cbuf_len(&handler->event.read.buf);
    uint8_t buf[len];
    cbuf_pop(&handler->event.read.buf, buf, len);

    // Copy memory back to cbuf pointer
    memcpy(handler->event.read.buf.ptr, buf, len);

    // move handler to loop unused list
    LL_PUSH(handler, sock->loop->handlers);

    return len;
}

void event_write_enable(event_sock_t *sock, uint8_t *buf, unsigned int bufsize)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(bufsize > 0);
    assert(buf != NULL);

    // write can only be performed on a connected socket
    assert(sock->state == EVENT_SOCK_CONNECTED);

    // see if there is writing is already enabled
    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_WRITE_TYPE);

    // only one write handler is allowed per socket
    assert(handler == NULL);

    // find free handler
    event_loop_t *loop = sock->loop;
    handler = event_handler_find_free(loop, sock);

    // If this fails, you need to increase the value of EVENT_MAX_HANDLERS
    assert(handler != NULL);

    // set handler event and error callback
    handler->type = EVENT_WRITE_TYPE;

    // initialize write buffer
    cbuf_init(&handler->event.write.buf, buf, bufsize);
}

int event_write(event_sock_t *sock, unsigned int size, uint8_t *bytes, event_write_cb cb)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(bytes != NULL);
    assert(cb != NULL);

    // write can only be performed on a connected socket
    assert(sock->state == EVENT_SOCK_CONNECTED);

    // find write handler
    event_loop_t * loop = sock->loop;
    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_WRITE_TYPE);

    // this will fail if event_write is called before event_write_enable
    assert(handler != NULL);

    // write bytes to output buffer
    int to_write = cbuf_push(&handler->event.write.buf, bytes, size);

    // add a write operation to the handler
    if (to_write > 0) {
        event_write_op_t *op = LL_MOVE(event_write_op_t, loop->write_ops, handler->event.write.ops);

        // If this fails increase CONFIG_EVENT_MAX_WRITE_OPS
        assert(op != NULL);

        op->cb = cb;
        op->bytes = to_write;
    }

    // get free operation from loop
#ifdef CONTIKI
    // poll the socket to perform write
    tcpip_poll_tcp(sock->uip_conn);
#endif

    return to_write;
}

void event_timeout(event_sock_t *sock, unsigned int millis, event_timer_cb cb)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(cb != NULL);

    // write can only be performed on a connected socket
    assert(sock->state == EVENT_SOCK_CONNECTED);

    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_TIMEOUT_TYPE);
    if (handler == NULL) {
        handler = event_handler_find_free(sock->loop, sock);
        assert(handler != NULL);
    }

    handler->type = EVENT_TIMEOUT_TYPE;
    handler->event.timer.cb = cb;
    handler->event.timer.millis = millis;

#ifndef CONTIKI
    // get start time
    gettimeofday(&handler->event.timer.start, NULL);
#else
    // set socket for handler
    handler->event.timer.sock = sock;
    ctimer_set(&handler->event.timer.ctimer, millis * CLOCK_SECOND / 1000, event_sock_handle_timeout, handler);
#endif
}

int event_accept(event_sock_t *server, event_sock_t *client)
{
    assert(server != NULL && client != NULL);
    assert(server->loop != NULL && client->loop != NULL);

    // check correct socket state
    assert(server->state == EVENT_SOCK_LISTENING);
    assert(client->state == EVENT_SOCK_CLOSED);

    event_loop_t *loop = client->loop;

#ifdef CONTIKI
    // Check that a new connection has been established
    assert(server->uip_conn != NULL);

    // Connection belongs to the client
    client->uip_conn = server->uip_conn;
    server->uip_conn = NULL;

    // attach socket state to uip_connection
    tcp_markconn(client->uip_conn, client);

    // use same port for client
    client->descriptor = server->descriptor;
#else
    int clifd = accept(server->descriptor, NULL, NULL);
    if (clifd < 0) {
        ERROR("Failed to accept new client");
        return -1;
    }

    // update max fds value
    loop->nfds = clifd + 1;

    // add socket fd to read watchlist
    FD_SET(clifd, &loop->active_fds);

    // set client variables
    client->descriptor = clifd;
#endif

    // set client state
    client->state = EVENT_SOCK_CONNECTED;

    // add socket to polling list
    LL_PUSH(client, loop->polling);

    return 0;
}

int event_close(event_sock_t *sock, event_close_cb cb)
{
    // check socket status
    assert(sock != NULL);
    assert(sock->loop != NULL);
    assert(cb != NULL);

    // set sock state and callback
    sock->state = EVENT_SOCK_CLOSING;
    sock->close_cb = cb;

    // find write handler
    event_handler_t *handler = event_handler_find(sock->handlers, EVENT_WRITE_TYPE);
    if (handler != NULL) { // mark the buffer as closed
        cbuf_end(&handler->event.write.buf);
    }

#ifdef CONTIKI
    // notify the loop process
    process_post(&event_loop_process, PROCESS_EVENT_CONTINUE, NULL);
#endif

    return 0;
}

void event_loop_init(event_loop_t *loop)
{
    assert(loop != NULL);
    // fail if loop pointer is not initialized

    // reset loop memory
    memset(loop, 0, sizeof(event_loop_t));

#ifndef CONTIKI
    // reset active file descriptor list
    FD_ZERO(&loop->active_fds);
#endif

    // reset socket memory
    LL_INIT(loop->sockets, EVENT_MAX_SOCKETS);
    LL_INIT(loop->handlers, EVENT_MAX_HANDLERS);
    LL_INIT(loop->write_ops, EVENT_MAX_WRITE_OPS);
}

event_sock_t *event_sock_create(event_loop_t *loop)
{
    assert(loop != NULL);

    // check that the loop is initialized
    // and there are available clients
    assert(loop->sockets != NULL);

    // get the first unused sock
    // and remove sock from unsed list
    event_sock_t *sock = LL_POP(loop->sockets);

    // reset sock memory
    memset(sock, 0, sizeof(event_sock_t));

    // assign sock variables
    sock->loop = loop;
    sock->state = EVENT_SOCK_CLOSED;

    return sock;
}

int event_sock_unused(event_loop_t *loop)
{
    assert(loop != NULL);
    return LL_COUNT(loop->sockets);
}

#ifdef CONTIKI
static event_loop_t *loop;

void event_loop(event_loop_t *l)
{
    assert(l != NULL);
    assert(loop == NULL);

    // set global variable
    loop = l;
    process_start(&event_loop_process, NULL);
}

PROCESS_THREAD(event_loop_process, ev, data)
#else
void event_loop(event_loop_t *loop)
#endif
{
#ifdef CONTIKI
    PROCESS_BEGIN();
#else
    // prevent send() raising a SIGPIPE when
    // remote connection has closed
    signal(SIGPIPE, SIG_IGN);
#endif
    assert(loop != NULL);
    assert(loop->running == 0);

    loop->running = 1;
    while (event_loop_is_alive(loop)) {
#ifdef CONTIKI
        PROCESS_WAIT_EVENT();

        // todo: handle timer events

        if (ev == tcpip_event) {
            // handle network events
            event_sock_handle_event(loop, data);
        }
#else
        // todo: perform timer events
        event_loop_timers(loop);

        // perform I/O events with 0 timeout
        event_loop_poll(loop, 0);

        // handle unprocessed read data
        event_loop_pending(loop);
#endif

        // perform close events
        event_loop_close(loop);
    }
    loop->running = 0;
#ifdef CONTIKI
    PROCESS_END();
#endif
}
