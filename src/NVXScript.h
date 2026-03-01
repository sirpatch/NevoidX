#ifndef NVX_SCRIPT_H
#define NVX_SCRIPT_H

#include <stdio.h>

// core interpreter routines
void run_file(const char *filename);

// line-level execution (used by shell)
void interpret_line_simple(FILE *file, char *line);

// helpers for other modules
int eval_condition(const char *cond);
void trim(char *s);
int execute_sys_command(const char *cmd);
void do_delay(void);

// helpers from requests module
int nvx_http_get(const char *url, char *response, size_t resp_size);
int nvx_http_post(const char *url, const char *body, char *response, size_t resp_size);

#endif // NVX_SCRIPT_H
