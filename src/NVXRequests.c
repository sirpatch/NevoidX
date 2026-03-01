#include "NVXRequests.h"
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

static int socket_connect(const char *host, const char *port) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) return -1;
    int s = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1) continue;
        if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0) break;
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
        s = -1;
    }
    freeaddrinfo(res);
    return s;
}

static int sendall(int sock, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return 0;
}

static int recvall(int sock, char *buf, int bufsize) {
    int total = 0;
    int r;
    while ((r = recv(sock, buf + total, bufsize - total - 1, 0)) > 0) {
        total += r;
        if (total >= bufsize-1) break;
    }
    if (total >= 0) buf[total] = '\0';
    return total;
}

static void parse_url(const char *url, char *host, size_t hostsz, char *port, size_t portsz, char *path, size_t pathsz) {
    // simple http parser
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = strchr(p, '/');
    size_t hostlen = slash ? (size_t)(slash - p) : strlen(p);
    const char *colon = memchr(p, ':', hostlen);
    if (colon) {
        size_t hlen = (size_t)(colon - p);
        strncpy(host, p, hlen); host[hlen] = '\0';
        size_t plen = hostlen - hlen - 1;
        strncpy(port, colon+1, plen); port[plen] = '\0';
    } else {
        strncpy(host, p, hostlen); host[hostlen] = '\0';
        strcpy(port, "80");
    }
    if (slash) strncpy(path, slash, pathsz-1);
    else strcpy(path, "/");
}

static int http_request(const char *method, const char *url, const char *body, char *response, size_t resp_size) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    char host[256], port[16], path[1024];
    parse_url(url, host, sizeof(host), port, sizeof(port), path, sizeof(path));
    int s = socket_connect(host, port);
    if (s < 0) return -1;
    char req[4096];
    if (body) {
        snprintf(req, sizeof(req), "%s %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %zu\r\n\r\n%s", method, path, host, strlen(body), body);
    } else {
        snprintf(req, sizeof(req), "%s %s HTTP/1.0\r\nHost: %s\r\n\r\n", method, path, host);
    }
    if (sendall(s, req, strlen(req)) < 0) {
#ifdef _WIN32
        closesocket(s);
        WSACleanup();
#else
        close(s);
#endif
        return -1;
    }
    // receive and strip headers
    char buf[8192];
    int r = recvall(s, buf, sizeof(buf));
    if (r < 0) {
#ifdef _WIN32
        closesocket(s);
        WSACleanup();
#else
        close(s);
#endif
        return -1;
    }

    // find double CRLF
    char *bodystart = strstr(buf, "\r\n\r\n");
    if (bodystart) bodystart += 4;
    else bodystart = buf;
    if (response && resp_size > 0) {
        strncpy(response, bodystart, resp_size-1);
        response[resp_size-1] = '\0';
    }
#ifdef _WIN32
    closesocket(s);
    WSACleanup();
#else
    close(s);
#endif
    return (int)strlen(bodystart);
}

int nvx_http_get(const char *url, char *response, size_t resp_size) {
    return http_request("GET", url, NULL, response, resp_size);
}

int nvx_http_post(const char *url, const char *body, char *response, size_t resp_size) {
    return http_request("POST", url, body, response, resp_size);
}
