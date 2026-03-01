#ifndef NVX_JSON_H
#define NVX_JSON_H

#include <stddef.h>

// simple JSON helpers: build and parse flat string->string objects

// encode a single key/value pair into json format (no escaping performed)
// result buffer must be large enough; returns number of chars written (excluding null)
size_t nvx_json_pair(char *out, size_t out_size, const char *key, const char *value);

// build an object from multiple pairs. pairs is array of alternating key,value strings ending with NULL
// e.g. nvx_json_object(buf, sizeof buf, "a","1","b","two",NULL);
size_t nvx_json_object(char *out, size_t out_size, const char **pairs);

// parse flat object, find value for given key; returns 1 if found, 0 otherwise
int nvx_json_get(const char *json, const char *key, char *out, size_t out_size);

#endif // NVX_JSON_H
