#include "NVXShell.h"
#include "NVXVars.h"
#include "NVXScript.h"
#include <stdio.h>
#include <string.h>

static void print_shell_help(void) {
    printf("NevoidX shell commands:\n");
    printf("  run <file>      - execute a script file\n");
    printf("  source <file>   - same as run <file>\n");
    printf("  exec <command>  - execute system command\n");
    printf("  vars            - list variables and values\n");
    printf("  types           - list declared variable types\n");
    printf("  help, ?         - show this help\n");
    printf("  quit, exit      - leave the shell\n");
}

static void list_variables(void) {
    if (variable_count == 0) { printf("(no variables)\n"); return; }
    for (int i = 0; i < variable_count; ++i) {
        printf("%s = %s\n", variables[i].name, variables[i].value);
    }
}

static void list_var_types(void) {
    if (type_count == 0) { printf("(no declared types)\n"); return; }
    for (int i = 0; i < type_count; ++i) {
        const char *tname = "unknown";
        if (var_types[i].type == 1) tname = "numeric";
        else if (var_types[i].type == 2) tname = "string";
        else if (var_types[i].type == 3) tname = "math-expr";
        printf("%s : %s\n", var_types[i].name, tname);
    }
}

void start_shell(void) {
    char line[512];
    while (1) {
        printf("NVD> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        if (strlen(line) == 0) continue;
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
        if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) { print_shell_help(); continue; }
        if (strcmp(line, "vars") == 0) { list_variables(); continue; }
        if (strcmp(line, "types") == 0) { list_var_types(); continue; }
        if (strncmp(line, "run ", 4) == 0) {
            char *fn = line + 4; trim(fn);
            if (*fn) run_file(fn);
            else printf("Usage: run <filename>\n");
            continue;
        }
        if (strncmp(line, "source ", 7) == 0) {
            char *fn = line + 7; trim(fn);
            if (*fn) run_file(fn);
            else printf("Usage: source <filename>\n");
            continue;
        }
        if (strncmp(line, "exec ", 5) == 0) {
            char *cmd = line + 5; trim(cmd);
            if (*cmd) {
                execute_sys_command(cmd);
            } else printf("Usage: exec <system-command>\n");
            continue;
        }
        interpret_line_simple(NULL, line);
        do_delay();
    }
}