#include "NVXNet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

#define MAX_ROUTES 32
static char *route_paths[MAX_ROUTES];
static nvx_route_handler route_handlers[MAX_ROUTES];
static int route_count = 0;

void nvx_register_route(const char *path, nvx_route_handler handler) {
    if (route_count < MAX_ROUTES) {
        route_paths[route_count] = strdup(path);
        route_handlers[route_count] = handler;
        route_count++;
    }
}

static int create_listener(const char *port) {
    struct addrinfo hints, *res, *rp;
    int s;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0) return -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1) continue;
        int on = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
        if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(s, 5) == 0) break;
        }
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }
    freeaddrinfo(res);
    return s;
}

static void handle_connection(int client) {
    char buf[8192];
    int r = recv(client, buf, sizeof(buf)-1, 0);
    if (r <= 0) return;
    buf[r] = '\0';
    // parse first line: METHOD PATH ...
    char method[16], path[256];
    sscanf(buf, "%15s %255s", method, path);
    // find handler
    for (int i = 0; i < route_count; i++) {
        if (strcmp(path, route_paths[i]) == 0) {
            char response[4096] = "";
            // locate body (after blank line)
            char *body = strstr(buf, "\r\n\r\n");
            if (body) body += 4;
            route_handlers[i](body?body: "", response, sizeof(response));
            char header[512];
            snprintf(header, sizeof(header), "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\n\r\n", strlen(response));
            send(client, header, strlen(header), 0);
            send(client, response, strlen(response), 0);
            break;
        }
    }
}

int nvx_run_server(const char *port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    int listener = create_listener(port);
    if (listener < 0) return -1;
    printf("NVX server listening on port %s\n", port);
    while (1) {
        int client;
#ifdef _WIN32
        client = accept(listener, NULL, NULL);
#else
        client = accept(listener, NULL, NULL);
#endif
        if (client < 0) break;
        handle_connection(client);
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
    }
#ifdef _WIN32
    closesocket(listener);
    WSACleanup();
#else
    close(listener);
#endif
    return 0;
}
