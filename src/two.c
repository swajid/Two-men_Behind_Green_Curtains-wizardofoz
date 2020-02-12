/*
   This API contains the "dos" methods to be used by
   HTTP/2
 */

#include "two.h"

#include <assert.h>
#include <strings.h>

#include "content_type.h"
#include "event.h"
#include "http.h"
#include "http2.h"

#define LOG_MODULE LOG_MODULE_HTTP
#include "logging.h"

/***********************************************
 * Aplication static data
 ***********************************************/
typedef struct
{
    char path[TWO_MAX_PATH_SIZE];
    char *method;
    char *content_type;
    two_resource_handler_t handler;
} two_resource_t;

static two_resource_t server_resources[TWO_MAX_RESOURCES];
static int server_resources_size = 0;

// Server event loop
static event_loop_t loop;

// Server socket
static event_sock_t *server;

// Global callback for closing
static void (*global_close_cb)();

/***********************************************
 * Private server methods
 ***********************************************/

/**
 * Parse URI into path and query parameters
 *
 * TODO: improve according to https://tools.ietf.org/html/rfc3986
 */
void parse_uri(char *uri, char *path, char *query_params)
{
    if (strlen(uri) == 0) {
        strcpy(path, "/");
    }

    char *ptr = index(uri, '?');
    if (ptr) {
        if (query_params) {
            strcpy(query_params, ptr + 1);
        }
        *ptr = '\0';
    } else if (query_params) {
        strcpy(query_params, "");
    }

    strcpy(path, uri);
}

/*
 * Check for valid HTTP path according to
 * RFC 2396 (see https://tools.ietf.org/html/rfc2396#section-3.3)
 *
 * TODO: for now this function only checks that the path starts
 * by a '/'. Validity of the path should be implemented according to
 * the RFC
 *
 * @return 1 if the path is valid or 0 if not
 * */
int is_valid_path(char *path)
{
    if (path[0] != '/') {
        return 0;
    }
    return 1;
}

static void on_server_close(event_sock_t *server)
{
    (void)server;
    PRINTF("HTTP/2 server closed\n");
    if (global_close_cb != NULL) {
        global_close_cb();
    }
}

static void on_client_close(event_sock_t *client)
{
    (void)client;
}

static event_sock_t *on_new_connection(event_sock_t *server)
{
    event_sock_t *client = event_sock_create(server->loop);
    if (event_accept(server, client) == 0) {
        http2_new_client(client);
    } else {
        event_close(client, on_client_close);
    }
    return client;
}

/*
 * Get a resource handler for the given path
 */
two_resource_t *find_resource(char *method, char *path)
{
    two_resource_t *res;

    for (int i = 0; i < server_resources_size; i++) {
        res = &server_resources[i];
        DEBUG("res->method %s", res->method);
        if (strncmp(res->path, path, TWO_MAX_PATH_SIZE) == 0 &&
            strncmp(res->method, method, strlen(res->method)) == 0) {
            return res;
        }
    }

    return NULL;
}

/***********************************************
 * HTTP (http.h) implementation methods
 ***********************************************/
static char *allowed_http_methods[] = { "GET", "HEAD", "POST", "PUT",
                                        "DELETE" };
#define HTTP_METHODS_LEN                                                       \
    (sizeof(allowed_http_methods) / sizeof(*allowed_http_methods))

char *http_get_method(char *method)
{
    if (method == NULL) {
        return NULL;
    }

    for (unsigned int i = 0; i < HTTP_METHODS_LEN; i++) {
        if (strncmp(method, allowed_http_methods[i],
                    strlen(allowed_http_methods[i])) == 0) {
            return allowed_http_methods[i];
        }
    }
    return NULL;
}

/**
 * Utility function to check for method support
 *
 * @returns 1 if the method is supported by the implementation, 0 if not
 */
int http_has_method_support(char *method)
{
    method = http_get_method(method);
    if (method == NULL) {
        return 0;
    }

    if (strncmp("GET", method, 3) == 0 || strncmp("HEAD", method, 4) == 0) {
        return 1;
    }
    return 0;
}

/**
 * Send an http error with the given code and message
 */
void http_error(http_response_t *res, int code, char *msg, unsigned int maxlen)
{
    assert(strlen(msg) < maxlen);

    // prepare response data
    res->status       = code;
    res->content_type = content_type_allowed("text/plain");

    // prepare msg body
    res->content_length = 0;
    memset(res->body, 0, maxlen);
    if (msg != NULL) {
        memcpy(res->body, msg, strlen(msg));
        res->content_length = strlen(msg);
    }
}

void http_handle_request(http_request_t *req, http_response_t *res,
                         unsigned int maxlen)
{
    if (!http_has_method_support(req->method)) {
        http_error(res, 501, "Not Implemented", maxlen);
        goto end;
    }

    // process the request
    char path[TWO_MAX_PATH_SIZE];
    parse_uri(req->path, path, NULL);

    // find callback for resource
    two_resource_t *uri_resource;
    if ((uri_resource = find_resource(req->method, path)) == NULL) {
        http_error(res, 404, "Not Found", maxlen);
        goto end;
    }

    int content_length = 0;

    // clean response memory
    memset(res->body, 0, maxlen);
    if ((content_length =
           uri_resource->handler(req->method, path, res->body, maxlen)) < 0) {
        http_error(res, 500, "Server error", maxlen);
        goto end;
    }

    res->status         = 200;
    res->content_length = content_length;
    res->content_type   = uri_resource->content_type;

end:
    INFO("%s %s HTTP/2.0 - %d", req->method, req->path, res->status);
    DEBUG("Request");
    for (int i = 0; i < (signed)req->headers.len; i++) {
        DEBUG("%s: %s", req->headers.list[i].name, req->headers.list[i].value);
    }
    DEBUG("Response status %d", res->status);
    DEBUG("Content-Type: %s", res->content_type);
    DEBUG("Content-Length: %d", res->content_length);
    DEBUG("%s", res->body);
}

/***********************************************
 * Public methods
 ***********************************************/

int two_server_start(unsigned int port)
{
    event_loop_init(&loop);
    server = event_sock_create(&loop);

    int r = event_listen(server, port, on_new_connection);
    if (r < 0) {
        ERROR("Failed to open socket for listening");
        return 1;
    }
    INFO("Starting HTTP/2 server in port %u", port);
    event_loop(&loop);

    return 0;
}

void two_server_stop(void (*close_cb)())
{
    global_close_cb = close_cb;
    event_close(server, on_server_close);
}

int two_register_resource(char *method, char *path, char *content_type,
                          two_resource_handler_t handler)
{
    assert(method != NULL && path != NULL && content_type != NULL &&
           handler != NULL);
    assert(strlen(path) < TWO_MAX_PATH_SIZE);

    if (!http_has_method_support(method)) {
        errno = EINVAL;
        ERROR("Method %s not implemented yet", method);
        return -1;
    }

    if (!is_valid_path(path)) {
        errno = EINVAL;
        ERROR("Path %s does not have a valid format", path);
        return -1;
    }

    char *ct = content_type_allowed(content_type);
    if (ct == NULL) {
        errno = EINVAL;
        ERROR("Unsupported content/type: %s", content_type);
        return -1;
    }

    // Checks if the app_resources variable is initialized
    static uint8_t inited_app_resources = 0;
    if (!inited_app_resources) {
        memset(&server_resources, 0,
               sizeof(two_resource_t) * TWO_MAX_RESOURCES);
        inited_app_resources = 1;
    }
    // Checks if the path and method already exist
    two_resource_t *res;
    for (int i = 0; i < server_resources_size; i++) {
        res = &server_resources[i];
        if (strncmp(res->path, path, TWO_MAX_PATH_SIZE) == 0 &&
            strcmp(res->method, method) == 0) {
            // If it does, replaces the resource
            res->content_type = ct;
            res->handler      = handler;
            return 0;
        }
    }

    // Checks if the list is full
    if (server_resources_size >= TWO_MAX_RESOURCES) {
        ERROR("Server resource limit (%d) reached. Try changing value for "
              "CONFIG_TWO_MAX_RESOURCES",
              TWO_MAX_RESOURCES);
        return -1;
    }

    // Adds the resource to the list
    res = &server_resources[server_resources_size++];

    // Set values
    strncpy(res->path, path, TWO_MAX_PATH_SIZE);
    res->method       = http_get_method(method);
    res->content_type = ct;
    res->handler      = handler;

    return 0;
}
