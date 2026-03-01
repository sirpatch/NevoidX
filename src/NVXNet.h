#ifndef NVX_NET_H
#define NVX_NET_H

#include <stddef.h>

// minimalist HTTP server. Register a handler for a path.
// Handler receives request body (for POST), and must fill response buffer.

typedef void (*nvx_route_handler)(const char *body, char *response, size_t resp_size);

// register route; path should begin with '/'
void nvx_register_route(const char *path, nvx_route_handler handler);

// start server on given port (string, e.g. "8080"). This call blocks until terminated.
int nvx_run_server(const char *port);

#endif // NVX_NET_H