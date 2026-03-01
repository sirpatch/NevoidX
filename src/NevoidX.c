#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NVXVars.h"
#include "NVXScript.h"
#include "NVXShell.h"

int main(int argc, char *argv[]) {
    int shell_mode = 0;
    const char *file_to_run = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--shell") == 0 || strcmp(argv[i], "-s") == 0) shell_mode = 1;
        else file_to_run = argv[i];
    }
    if (file_to_run && !shell_mode) {
        run_file(file_to_run);
        return 0;
    }
    if (file_to_run && shell_mode) {
        // preload file then drop to interactive shell
        run_file(file_to_run);
        start_shell();
        return 0;
    }
    if (shell_mode) {
        start_shell();
        return 0;
    }
    printf("Usage: %s [--shell] or <filename>\n", argc>0?argv[0]:"NevoidX");
    return 1;
}
