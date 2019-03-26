#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/select.h>

#include "event_loop.h"

#define MAX_NO_OF_HANDLES 32

// Active file descriptors
static fd_set active_fd_set;
static int active_fd_set_is_init = 0;

/* Bind an event to its file descriptor */
typedef struct {
    int is_used;
    event_handler_t event;
    int fd;
} handler_registration;
static handler_registration registered_handlers[MAX_NO_OF_HANDLES];

int register_handler(event_handler_t *event)
{
    assert(NULL != event);

    // First call
    if (active_fd_set_is_init  == 0) {
        FD_ZERO(&active_fd_set); // Initialize the set of active handles
        active_fd_set_is_init = 1;
    }

    int fd = event->get_fd(event->instance);
    int is_registered = -1;
    if (!FD_ISSET(fd, &active_fd_set)) {
        // Look for a free entry in the registered event array
        for (int i = 0; (i < MAX_NO_OF_HANDLES) && (is_registered < 0); i++) {
            if (registered_handlers[i].is_used == 0) {
                handler_registration *free_entry = &registered_handlers[i];

                free_entry->event = *event;
                free_entry->fd = fd;
                free_entry->is_used = 1;

                FD_SET(fd, &active_fd_set);

                is_registered = 0;
            }
        }
    }

    return is_registered;
}

int unregister_handler(event_handler_t *event)
{
    assert(NULL != event);

    // First call
    if (active_fd_set_is_init < 0) {
        FD_ZERO(&active_fd_set);    // Initialize the set of active handles
        active_fd_set_is_init = 1;        // And reset counter
    }

    int fd = event->get_fd(event->instance);
    int node_removed = -1;
    if (FD_ISSET(fd, &active_fd_set)) {
        for (int i = 0; (i < MAX_NO_OF_HANDLES) && (node_removed < 0); i++) {
            if (registered_handlers[i].is_used && registered_handlers[i].fd == fd) {
                registered_handlers[i].is_used = 0;
                close(fd);
                FD_CLR(fd, &active_fd_set);
            }
        }       
    }
    return node_removed;
}

event_handler_t * find_event(int fd) {
    event_handler_t * match = NULL;

    for (int i = 0; i < MAX_NO_OF_HANDLES && match != NULL; i++) {
        if (registered_handlers[i].is_used && registered_handlers[i].fd == fd) {
            match = &registered_handlers[i].event;
        }
    }
    return match;
}


void handle_events(void) {
    fd_set read_fd_set;
    for (;;) {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(EXIT_FAILURE); // TODO: should we exit here?
        }

        /* Service all the sockets with input pending. */
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET(i, &read_fd_set))
            {
                event_handler_t * signalled_event = find_event(i);
                if (signalled_event != NULL) {
                    signalled_event->handle(signalled_event->instance);
                }
            }
        }
    }
}
