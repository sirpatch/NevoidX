#ifndef NVX_REQUESTS_H
#define NVX_REQUESTS_H

#include <stddef.h>

// simple HTTP client functions
// url should be of form "http://hostname[:port]/path"
// response buffer gets the full body (not headers); returns length or -1 on error
int nvx_http_get(const char *url, char *response, size_t resp_size);
int nvx_http_post(const char *url, const char *body, char *response, size_t resp_size);

#endif // NVX_REQUESTS_H
