#include "NVXVars.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// variable storage defined in header
Variable variables[100];
int variable_count = 0;

VarType var_types[100];
int type_count = 0;

// Named blocks (for `void name { ... }` and `goto name`)
typedef struct { char name[64]; char *body; } NamedBlock;
NamedBlock named_blocks[100];
int named_block_count = 0;

int script_delay = -1; // -1 = no delay, 0 = wait for input, >0 seconds delay between statements

void set_var_type(const char *name, int type) {
    for (int i = 0; i < type_count; i++) {
        if (strcmp(var_types[i].name, name) == 0) {
            var_types[i].type = type;
            return;
        }
    }
    if (type_count < 100) {
        strcpy(var_types[type_count].name, name);
        var_types[type_count].type = type;
        type_count++;
    }
}

int get_var_type(const char *name) {
    for (int i = 0; i < type_count; i++) {
        if (strcmp(var_types[i].name, name) == 0) {
            return var_types[i].type;
        }
    }
    return 0; // unknown type
}

void set_variable(const char *name, const char *value) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            strcpy(variables[i].value, value);
            return;
        }
    }
    if (variable_count < 100) {
        strcpy(variables[variable_count].name, name);
        strcpy(variables[variable_count].value, value);
        variable_count++;
    }
}

const char* get_variable(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void store_named_block(const char *name, const char *body) {
    for (int i = 0; i < named_block_count; ++i) {
        if (strcmp(named_blocks[i].name, name) == 0) {
            free(named_blocks[i].body);
            named_blocks[i].body = strdup(body);
            return;
        }
    }
    if (named_block_count < 100) {
        strncpy(named_blocks[named_block_count].name, name, sizeof(named_blocks[named_block_count].name)-1);
        named_blocks[named_block_count].name[sizeof(named_blocks[named_block_count].name)-1] = '\0';
        named_blocks[named_block_count].body = strdup(body);
        named_block_count++;
    }
}

const char *find_named_block_body(const char *name) {
    for (int i = 0; i < named_block_count; ++i) {
        if (strcmp(named_blocks[i].name, name) == 0) return named_blocks[i].body;
    }
    return NULL;
}
