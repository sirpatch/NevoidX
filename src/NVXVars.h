#ifndef NVX_VARS_H
#define NVX_VARS_H

// variable storage and type management used by the interpreter

// set and get variable by name (string value)
void set_variable(const char *name, const char *value);
const char* get_variable(const char *name);

// storage arrays exposed for shell/debug
extern int variable_count;
extern int type_count;

// underlying storage structures are exposed for introspection
typedef struct { char name[50]; char value[100]; } Variable;
extern Variable variables[100];

typedef struct { char name[50]; int type; } VarType;
extern VarType var_types[100];

// type management: 1=numeric,2=string,3=math-expression
void set_var_type(const char *name, int type);
int get_var_type(const char *name);

// named block storage (for void/goto)
void store_named_block(const char *name, const char *body);
const char *find_named_block_body(const char *name);

// script-wide control
extern int script_delay;

#endif // NVX_VARS_H
