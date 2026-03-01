#include "NVXJSON.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

size_t nvx_json_pair(char *out, size_t out_size, const char *key, const char *value) {
    if (!out || out_size == 0) return 0;
    // naive: "key":"value"
    int written = snprintf(out, out_size, "\"%s\":\"%s\"", key, value);
    if (written < 0) return 0;
    if ((size_t)written >= out_size) return out_size-1;
    return (size_t)written;
}

size_t nvx_json_object(char *out, size_t out_size, const char **pairs) {
    if (!out || out_size == 0) return 0;
    size_t pos = 0;
    out[pos++] = '{';
    int first = 1;
    for (const char **p = pairs; p && *p; p += 2) {
        const char *k = *p;
        const char *v = *(p+1);
        if (!k || !v) break;
        char pairbuf[256];
        nvx_json_pair(pairbuf, sizeof(pairbuf), k, v);
        if (!first) {
            if (pos + 1 < out_size) out[pos++] = ',';
        }
        size_t len = strlen(pairbuf);
        if (pos + len < out_size) {
            memcpy(out+pos, pairbuf, len);
            pos += len;
        } else break;
        first = 0;
    }
    if (pos < out_size) out[pos++] = '}';
    if (pos < out_size) out[pos] = '\0';
    else out[out_size-1] = '\0';
    return pos;
}

int nvx_json_get(const char *json, const char *key, char *out, size_t out_size) {
    if (!json || !key || !out) return 0;
    // find "key" (not too smart about escapes)
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return 0;
    }
    p += strlen(pattern);
    // skip whitespace
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ':') {
        return 0;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\"') {
        // string value
        p++;
        const char *q = p;
        while (*q && *q != '\"') {
            if (*q == '\\' && q[1]) q += 2;
            else q++;
        }
        if (!*q) return 0;
        size_t len = q - p;
        if (len >= out_size) len = out_size - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        return 1;
    } else if (*p == '{' || *p == '[') {
        // copy balanced object/array
        char open = *p;
        char close = (open == '{' ? '}' : ']');
        const char *start = p;
        int depth = 0;
        do {
            if (*p == open) depth++;
            else if (*p == close) depth--;
            p++;
        } while (*p && depth > 0);
        size_t len = p - start;
        if (len >= out_size) len = out_size - 1;
        memcpy(out, start, len);
        out[len] = '\0';
        return 1;
    } else {
        // number, boolean, or null
        const char *q = p;
        while (*q && *q != ',' && *q != '}' && *q != ']' && !isspace((unsigned char)*q)) q++;
        size_t len = q - p;
        if (len >= out_size) len = out_size - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        return 1;
    }
}
