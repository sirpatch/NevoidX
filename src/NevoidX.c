#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef struct {
    char name[50];
    char value[100];
} Variable;

typedef struct {
    char name[50];
    int type; // 1 = var (numeric), 2 = str (string)
} VarType;

Variable variables[100];
int variable_count = 0;

VarType var_types[100];
int type_count = 0;

// Named blocks (for `void name { ... }` and `goto name`)
typedef struct { char name[64]; char *body; } NamedBlock;
NamedBlock named_blocks[100];
int named_block_count = 0;

static void store_named_block(const char *name, const char *body) {
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

static const char *find_named_block_body(const char *name) {
    for (int i = 0; i < named_block_count; ++i) {
        if (strcmp(named_blocks[i].name, name) == 0) return named_blocks[i].body;
    }
    return NULL;
}

// script-wide control
int script_delay = -1; // -1 = no delay, 0 = wait for input, >0 seconds delay between statements

// pushback line when block reader reads ahead
char pushback_line[512];
int have_pushback = 0;

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
    for (int i = 0; i <variable_count; i++) {
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
// Trim helpers
static void ltrim(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void rtrim(char *s) {
    int n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static void trim(char *s) { ltrim(s); rtrim(s); }

// Expression evaluator using shunting-yard -> RPN evaluation
#include <ctype.h>

typedef enum {T_NUMBER, T_VAR, T_OP, T_LP, T_RP, T_FUNC} ExprTokenType;
typedef struct { ExprTokenType type; double value; char op; char name[64]; } Token;

static int is_known_func(const char *name) {
    static const char *funcs[] = {"sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sqrt", "hypot", "abs", "floor", "ceil", "round", "log", "log2", "ln", "exp", "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "min", "max", NULL};
    for (int i = 0; funcs[i]; i++) if (strcmp(name, funcs[i]) == 0) return 1;
    return 0;
}

static int precedence(char op) {
    switch (op) {
        case 'u': return 5; // unary minus
        case '^': return 4;
        case '*': case '/': case '%': return 3;
        case '+': case '-': return 2;
    }
    return 0;
}

static int is_right_assoc(char op) {
    return (op == '^' || op == 'u');
}

static int tokenize(const char *s, Token *out, int *out_len) {
    int pos = 0;
    int idx = 0;
    Token prev = {0};
    while (s[pos]) {
        if (isspace((unsigned char)s[pos])) { pos++; continue; }
        if ((s[pos] >= '0' && s[pos] <= '9') || (s[pos]=='.' && isdigit((unsigned char)s[pos+1])) || ((s[pos]=='-' ) && ((pos==0) || (prev.type==T_OP) || (prev.type==T_LP)) && (isdigit((unsigned char)s[pos+1]) || s[pos+1]=='.'))) {
            // number (handle unary minus attached to number)
            char *endptr;
            double v = strtod(s + pos, &endptr);
            out[idx].type = T_NUMBER;
            out[idx].value = v;
            pos += (endptr - (s + pos));
            prev = out[idx]; idx++;
            continue;
        }
        if (isalpha((unsigned char)s[pos]) || s[pos]=='_') {
            int j = 0;
            while (s[pos] && (isalnum((unsigned char)s[pos]) || s[pos]=='_')) {
                if (j < (int)sizeof(out[idx].name)-1) out[idx].name[j++] = s[pos];
                pos++;
            }
            out[idx].name[j] = '\0';
            // skip spaces after name
            int check_pos = pos;
            while (s[check_pos] && isspace((unsigned char)s[check_pos])) check_pos++;
            // if followed by '(' and is a known function, mark as FUNC
            if (s[check_pos] == '(' && is_known_func(out[idx].name)) {
                out[idx].type = T_FUNC;
            } else {
                out[idx].type = T_VAR;
            }
            prev = out[idx]; idx++;
            continue;
        }
        if (s[pos] == '(') { out[idx].type = T_LP; out[idx].op = '('; prev = out[idx]; idx++; pos++; continue; }
        if (s[pos] == ')') { out[idx].type = T_RP; out[idx].op = ')'; prev = out[idx]; idx++; pos++; continue; }
        // operators
        char c = s[pos];
        if (c=='+'||c=='-'||c=='*'||c=='/'||c=='%'||c=='^') {
            out[idx].type = T_OP; out[idx].op = c; prev = out[idx]; idx++; pos++; continue;
        }
        // unknown char
        return 0;
    }
    *out_len = idx;
    return 1;
}

static int shunting_yard(Token *tokens, int ntok, Token *out, int *out_len) {
    Token ops[256]; int ops_top = 0;
    int out_i = 0;
    for (int i = 0; i < ntok; ++i) {
        Token t = tokens[i];
        if (t.type == T_NUMBER || t.type == T_VAR) { out[out_i++] = t; continue; }
        if (t.type == T_FUNC) { ops[ops_top++] = t; continue; } // push function to ops stack
        if (t.type == T_OP) {
            char op = t.op;
            // detect unary minus if previous is none or operator or left paren
            if (op == '-') {
                if (i==0 || tokens[i-1].type==T_OP || tokens[i-1].type==T_LP) op = 'u';
            }
            while (ops_top > 0 && ops[ops_top-1].type == T_OP) {
                char topop = ops[ops_top-1].op;
                int prec1 = precedence(op);
                int prec2 = precedence(topop);
                if ((!is_right_assoc(op) && prec1 <= prec2) || (is_right_assoc(op) && prec1 < prec2)) {
                    out[out_i++] = ops[--ops_top];
                    continue;
                }
                break;
            }
            Token push = {T_OP, 0, op, {0}}; ops[ops_top++] = push;
            continue;
        }
        if (t.type == T_LP) { ops[ops_top++] = t; continue; }
        if (t.type == T_RP) {
            int found = 0;
            while (ops_top > 0) {
                Token top = ops[--ops_top];
                if (top.type == T_LP) { found = 1; break; }
                if (top.type == T_FUNC) { out[out_i++] = top; break; } // pop function to output
                out[out_i++] = top;
            }
            if (!found) return 0;
            continue;
        }
    }
    while (ops_top > 0) {
        Token top = ops[--ops_top];
        if (top.type == T_LP || top.type == T_RP) return 0;
        out[out_i++] = top;
    }
    *out_len = out_i;
    return 1;
}

static double apply_func(const char *fname, double arg) {
    if (strcmp(fname, "sin") == 0) return sin(arg);
    if (strcmp(fname, "cos") == 0) return cos(arg);
    if (strcmp(fname, "tan") == 0) return tan(arg);
    if (strcmp(fname, "asin") == 0) return asin(arg);
    if (strcmp(fname, "acos") == 0) return acos(arg);
    if (strcmp(fname, "atan") == 0) return atan(arg);
    if (strcmp(fname, "sinh") == 0) return sinh(arg);
    if (strcmp(fname, "cosh") == 0) return cosh(arg);
    if (strcmp(fname, "tanh") == 0) return tanh(arg);
    if (strcmp(fname, "asinh") == 0) return asinh(arg);
    if (strcmp(fname, "acosh") == 0) return acosh(arg);
    if (strcmp(fname, "atanh") == 0) return atanh(arg);
    if (strcmp(fname, "sqrt") == 0) return sqrt(arg);
    if (strcmp(fname, "abs") == 0) return fabs(arg);
    if (strcmp(fname, "floor") == 0) return floor(arg);
    if (strcmp(fname, "ceil") == 0) return ceil(arg);
    if (strcmp(fname, "round") == 0) return round(arg);
    if (strcmp(fname, "log") == 0) return log10(arg);
    if (strcmp(fname, "log2") == 0) return log2(arg);
    if (strcmp(fname, "ln") == 0) return log(arg);
    if (strcmp(fname, "exp") == 0) return exp(arg);
    return 0; // unknown function
}

static double apply_func2(const char *fname, double a, double b) {
    if (strcmp(fname, "min") == 0) return (a < b) ? a : b;
    if (strcmp(fname, "max") == 0) return (a > b) ? a : b;
    if (strcmp(fname, "atan2") == 0) return atan2(a, b);
    if (strcmp(fname, "hypot") == 0) return hypot(a, b);
    if (strcmp(fname, "mod") == 0) return fmod(a, b);
    return 0;
}

static int evaluate_rpn(Token *rpn, int len, double *out_val) {
    double stack[256]; int top = 0;
    for (int i = 0; i < len; ++i) {
        Token t = rpn[i];
        if (t.type == T_NUMBER) { stack[top++] = t.value; continue; }
        if (t.type == T_VAR) {
            const char *v = get_variable(t.name);
            if (!v) return 0;
            stack[top++] = atof(v);
            continue;
        }
        if (t.type == T_FUNC) {
            // min/max take 2 args, others take 1
            if (strcmp(t.name, "min") == 0 || strcmp(t.name, "max") == 0) {
                if (top < 2) return 0;
                double b = stack[--top];
                double a = stack[--top];
                double res = apply_func2(t.name, a, b);
                stack[top++] = res;
            } else {
                if (top < 1) return 0;
                double a = stack[--top];
                double res = apply_func(t.name, a);
                stack[top++] = res;
            }
            continue;
        }
        if (t.type == T_OP) {
            char op = t.op;
            if (op == 'u') {
                if (top < 1) return 0;
                double a = stack[--top];
                stack[top++] = -a;
                continue;
            }
            if (top < 2) return 0;
            double b = stack[--top];
            double a = stack[--top];
            double res = 0;
            switch (op) {
                case '+': res = a + b; break;
                case '-': res = a - b; break;
                case '*': res = a * b; break;
                case '/': if (b == 0) { printf("NVD Error: Division by zero.\n"); return 0; } res = a / b; break;
                case '%': res = fmod(a, b); break;
                case '^': res = pow(a, b); break;
                default: return 0;
            }
            stack[top++] = res;
            continue;
        }
    }
    if (top != 1) return 0;
    *out_val = stack[0];
    return 1;
}

int evaluate_math_expr(const char *expr, double *result) {
    Token toks[256]; int ntok = 0;
    if (!tokenize(expr, toks, &ntok)) return 0;
    Token rpn[256]; int rlen = 0;
    if (!shunting_yard(toks, ntok, rpn, &rlen)) return 0;
    if (!evaluate_rpn(rpn, rlen, result)) return 0;
    return 1;
}

// Delay helper - resets delay after use so it only affects one statement
static void do_delay() {
    if (script_delay < 0) return;
#ifdef _WIN32
    if (script_delay == 0) {
        printf("Press Enter to continue..."); fflush(stdout);
        char tmp[8]; fgets(tmp, sizeof(tmp), stdin);
    } else {
        Sleep(script_delay * 1000);
    }
#else
    if (script_delay == 0) {
        printf("Press Enter to continue..."); fflush(stdout);
        char tmp[8]; fgets(tmp, sizeof(tmp), stdin);
    } else {
        sleep((unsigned int)script_delay);
    }
#endif
    script_delay = -1;  // Reset after use so delay only affects one statement
}


// Parse comma-separated print arguments (respecting quoted strings)
void execute_print(char *line) {
    char *start = strchr(line, '(');
    if (!start) return;
    start++;

    char *end = strrchr(start, ')');
    if (!end) return;
    *end = '\0';
    
    // copy to buffer and trim
    char full_content[1024];
    strncpy(full_content, start, sizeof(full_content)-1); full_content[sizeof(full_content)-1] = '\0';
    trim(full_content);

    // Split by commas (respecting quotes) and evaluate each part
    int i = 0;
    int first_arg = 1;
    while (full_content[i]) {
        // skip leading spaces
        while (full_content[i] && isspace((unsigned char)full_content[i])) i++;
        if (!full_content[i]) break;

        // extract argument until comma or end (respecting quotes)
        char arg[512];
        int j = 0;
        int in_quotes = 0;
        while (full_content[i] && j < (int)sizeof(arg)-1) {
            if (full_content[i] == '"') {
                in_quotes = !in_quotes;
                arg[j++] = full_content[i++];
            } else if (full_content[i] == ',' && !in_quotes) {
                break;
            } else {
                arg[j++] = full_content[i++];
            }
        }
        arg[j] = '\0';
        
        // trim trailing spaces
        while (j > 0 && isspace((unsigned char)arg[j-1])) arg[--j] = '\0';

        // skip the comma
        if (full_content[i] == ',') i++;

        // Evaluate and print the argument
        if (strlen(arg) > 0) {
            char trimmed_arg[512];
            strncpy(trimmed_arg, arg, sizeof(trimmed_arg)-1); trimmed_arg[sizeof(trimmed_arg)-1] = '\0';
            trim(trimmed_arg);
            size_t len = strlen(trimmed_arg);

            // STRING LITERAL - if text is in quotes, print it as-is, no matter what
            if (len >= 2 && trimmed_arg[0] == '"' && trimmed_arg[len - 1] == '"') {
                // Print the content between quotes
                for (size_t k = 1; k < len - 1; k++) {
                    putchar(trimmed_arg[k]);
                }
            }
            // Check if simple variable (identifier)
            else if (isalpha((unsigned char)trimmed_arg[0]) || trimmed_arg[0] == '_') {
                int is_simple_var = 1;
                for (size_t k = 0; k < len; k++) {
                    if (!isalnum((unsigned char)trimmed_arg[k]) && trimmed_arg[k] != '_') { is_simple_var = 0; break; }
                }
                
                if (is_simple_var) {
                    int vtype = get_var_type(trimmed_arg);
                    if (vtype == 2 || vtype == 3) { // string type or math-expression string
                        const char *value = get_variable(trimmed_arg);
                        if (value) {
                            printf("%s", value);
                        }
                    } else {
                        // Try numeric variable or expression
                        double dres;
                        if (evaluate_math_expr(trimmed_arg, &dres)) {
                            if (fabs(dres - round(dres)) < 1e-9) printf("%lld", (long long)llround(dres));
                            else printf("%g", dres);
                        } else {
                            const char *value = get_variable(trimmed_arg);
                            if (value) {
                                printf("%s", value);
                            }
                        }
                    }
                }
            }
            // MATH EXPRESSION or fallback VARIABLE
            else {
                double dres;
                if (evaluate_math_expr(trimmed_arg, &dres)) {
                    if (fabs(dres - round(dres)) < 1e-9) printf("%lld", (long long)llround(dres));
                    else printf("%g", dres);
                } else {
                    const char *value = get_variable(trimmed_arg);
                    if (value) {
                        printf("%s", value);
                    }
                }
            }
        }
        
        first_arg = 0;
    }
    
    // Print newline at the end and flush
    printf("\n");
    fflush(stdout);
    do_delay();
}

void execute_math(char *line) {
    char *start = strchr(line, '(');
    if (!start) return;
    start++;
    char *end = strrchr(start, ')');
    if (!end) return;
    *end = '\0';

    // check for assignment inside math: VAR = expr
    char *eq = strchr(start, '=');
    if (eq) {
        *eq = '\0';
        char lhs[64]; char rhs[256];
        strncpy(lhs, start, sizeof(lhs)-1); lhs[sizeof(lhs)-1] = '\0';
        strncpy(rhs, eq+1, sizeof(rhs)-1); rhs[sizeof(rhs)-1] = '\0';
        trim(lhs); trim(rhs);
        double res;
        if (!evaluate_math_expr(rhs, &res)) { printf("NVD Error: Invalid math expression.\n"); return; }
        char buf[100];
        if (fabs(res - round(res)) < 1e-9) snprintf(buf, sizeof(buf), "%lld", (long long)llround(res));
        else snprintf(buf, sizeof(buf), "%g", res);
        set_variable(lhs, buf);
        return;
    }

    // otherwise evaluate and print
    double res;
    if (evaluate_math_expr(start, &res)) {
        if (fabs(res - round(res)) < 1e-9) printf("%lld\n", (long long)llround(res));
        else printf("%g\n", res);
        return;
    }
    printf("NVD Error: Invalid math statement.\n");
    do_delay();
}

// forward declare stream interpreter
void interpret_stream(FILE *file, char *first_line, int parent_exec);

// Execute a system command via the host shell. Returns exit code.
static int execute_sys_command(const char *cmd) {
    if (!cmd) return -1;
    // Use system() to run the command; output goes to the console.
    int rc = system(cmd);
    return rc;
}

// interpret a single, simple line (no block-reading)
void interpret_line_simple(FILE *file, char *line) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;
    // Trim leading/trailing whitespace
    trim(line);

    if (strlen(line) == 0)
        return;

    // List commands
    if (strcmp(line, "NevoidX.commands") == 0) {
        printf("Available commands:\n");
        printf(" - def.var=VAR1,VAR2     : declare numeric variables\n");
        printf(" - def.str=NAME1,NAME2   : declare string variables\n");
        printf(" - def.math=NAME1,NAME2  : declare math-expression variables\n");
        printf(" - VAR=VALUE           : assign literal, quoted string, or existing var\n");
        printf(" - VAR=math(expr)      : evaluate expr and assign result to VAR\n");
        printf(" - VAR=math(VARNAME)   : evaluate expression stored in VARNAME and assign\n");
        printf(" - VAR=user.input_var(\"prompt\") : numeric input\n");
        printf(" - VAR=user.input_str(\"prompt\") : string input\n");
        printf(" - VAR=user.input_math(\"prompt\") : raw expression input (stored as string)\n");
        printf(" - VAR=user.choice_var(\"prompt\",opt1,opt2,...) : choose option (stores option text)\n");
        printf(" - VAR=user.choice_str(\"prompt\",\"optA\",\"optB\") : choose string option\n");
        printf(" - print(...)             : print literals, vars, or math expressions\n");
        printf(" - math(expr)             : evaluate expression and print result\n");
        printf(" - if (cond) { ... } else if (cond) { ... } else { ... } : conditional blocks\n");
        printf(" - delay=N                : set script delay (N seconds). 0 waits for Enter.\n");
        do_delay();
        return;
    }

    // DEF statements (def.var= or def.str=)
    if (strncmp(line, "def.var=", 8) == 0) {
        char *list = line + 8;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        // parse comma-separated list manually
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            // skip leading spaces
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            // read until comma or end
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            // trim trailing spaces
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 1); // type 1 = numeric var
            }
            if (buf[i] == ',') i++; // skip comma
        }
        return;
    }

    if (strncmp(line, "def.str=", 8) == 0) {
        char *list = line + 8;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        // parse comma-separated list manually
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            // skip leading spaces
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            // read until comma or end
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            // trim trailing spaces
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 2); // type 2 = string
            }
            if (buf[i] == ',') i++; // skip comma
        }
        return;
    }

    if (strncmp(line, "def.math=", 9) == 0) {
        char *list = line + 9;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        // parse comma-separated list manually
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            // skip leading spaces
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            // read until comma or end
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            // trim trailing spaces
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 3); // type 3 = math (store expression string)
            }
            if (buf[i] == ',') i++; // skip comma
        }
        return;
    }

    // Delay setting: delay=N (affects the NEXT line)
    if (strncmp(line, "delay=", 6) == 0) {
        char *val = line + 6;
        char valcopy[64];
        strncpy(valcopy, val, sizeof(valcopy)-1); valcopy[sizeof(valcopy)-1] = '\0';
        trim(valcopy);
        int delay_val = atoi(valcopy);
        if (delay_val >= 0) script_delay = delay_val;
        return;
    }

    // Assignment - but skip = inside quoted strings
    char *equals = NULL;
    int in_quotes = 0;
    for (char *p = line; *p; p++) {
        if (*p == '"') in_quotes = !in_quotes;
        else if (*p == '=' && !in_quotes) { equals = p; break; }
    }
    if (equals) {
        *equals = '\0';
        char namebuf[64]; char valuebuf[256];
        strncpy(namebuf, line, sizeof(namebuf)-1); namebuf[sizeof(namebuf)-1] = '\0';
        strncpy(valuebuf, equals + 1, sizeof(valuebuf)-1); valuebuf[sizeof(valuebuf)-1] = '\0';
        trim(namebuf); trim(valuebuf);

        // sys.command(...) on RHS: execute shell command and store exit code as variable
        if (strncmp(valuebuf, "sys.command(", 12) == 0) {
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                char cmdbuf[512]; strncpy(cmdbuf, p, sizeof(cmdbuf)-1); cmdbuf[sizeof(cmdbuf)-1] = '\0'; trim(cmdbuf);
                // remove surrounding quotes if present
                size_t cl = strlen(cmdbuf);
                if (cl >= 2 && cmdbuf[0] == '"' && cmdbuf[cl-1] == '"') { cmdbuf[cl-1] = '\0'; memmove(cmdbuf, cmdbuf+1, strlen(cmdbuf)); }
                int rc = execute_sys_command(cmdbuf);
                char rcbuf[64]; snprintf(rcbuf, sizeof(rcbuf), "%d", rc);
                set_variable(namebuf, rcbuf);
                return;
            }
        }

        // user.input_var / user.input_str / user.input (fallback to string)
        int input_mode = 0; // 1=var, 2=str, 3=math (raw expression)
        if (strncmp(valuebuf, "user.input_var(", 15) == 0 || strncmp(valuebuf, "input_var(", 10) == 0) input_mode = 1;
        else if (strncmp(valuebuf, "user.input_str(", 15) == 0 || strncmp(valuebuf, "input_str(", 10) == 0) input_mode = 2;
        else if (strncmp(valuebuf, "user.input(", 11) == 0 || strncmp(valuebuf, "input(", 6) == 0) input_mode = 2;
        else if (strncmp(valuebuf, "user.input_math(", 16) == 0 || strncmp(valuebuf, "input_math(", 11) == 0) input_mode = 3;
        // choice_var / choice_str
        if (strncmp(valuebuf, "user.choice_var(", 16) == 0 || strncmp(valuebuf, "choice_var(", 11) == 0) input_mode = 4;
        if (strncmp(valuebuf, "user.choice_str(", 16) == 0 || strncmp(valuebuf, "choice_str(", 11) == 0) input_mode = 5;
        if (input_mode) {
            char *p = strchr(valuebuf, '(');
            char prompt[200] = "";
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                strncpy(prompt, p, sizeof(prompt)-1); prompt[sizeof(prompt)-1] = '\0';
                trim(prompt);
                // remove surrounding quotes if present
                size_t plen = strlen(prompt);
                if (plen >= 2 && prompt[0] == '"' && prompt[plen-1] == '"') {
                    prompt[plen-1] = '\0';
                    memmove(prompt, prompt+1, strlen(prompt));
                }
            }
            // For choice handlers, parse options from prompt text area (after first comma)
            if (input_mode == 4 || input_mode == 5) {
                // p currently points to start of args; reconstruct args string
                char argsbuf[512];
                strncpy(argsbuf, p, sizeof(argsbuf)-1); argsbuf[sizeof(argsbuf)-1] = '\0';
                // split by commas: first arg is prompt, rest are options
                char *args[64]; int nargs = 0;
                char token[256]; int tpos = 0; int inq = 0;
                for (int ii = 0;; ii++) {
                    char ch = argsbuf[ii];
                    if (ch == '\0' || (ch == ',' && !inq)) {
                        token[tpos] = '\0';
                        trim(token);
                        args[nargs] = strdup(token);
                        nargs++;
                        tpos = 0;
                        if (ch == '\0') break;
                    } else {
                        if (ch == '"') inq = !inq;
                        if (tpos < (int)sizeof(token)-1) token[tpos++] = ch;
                    }
                }
                // first arg is prompt
                char local_prompt[200] = ">";
                if (nargs > 0) {
                    strncpy(local_prompt, args[0], sizeof(local_prompt)-1); local_prompt[sizeof(local_prompt)-1] = '\0';
                    size_t lp = strlen(local_prompt);
                    if (lp >= 2 && local_prompt[0] == '"' && local_prompt[lp-1] == '"') { local_prompt[lp-1] = '\0'; memmove(local_prompt, local_prompt+1, strlen(local_prompt)); }
                }
                printf("%s\n", local_prompt); fflush(stdout);
                for (int oi = 1; oi < nargs; oi++) { printf("%d) %s\n", oi, args[oi]); }
                printf("Choose: "); fflush(stdout);
                char input_buffer[200];
                if (!fgets(input_buffer, sizeof(input_buffer), stdin)) input_buffer[0] = '\0';
                input_buffer[strcspn(input_buffer, "\n")] = 0;
                trim(input_buffer);
                int chosen_index = -1;
                if (strlen(input_buffer) > 0 && isdigit((unsigned char)input_buffer[0])) chosen_index = atoi(input_buffer);
                if (chosen_index > 0 && chosen_index < nargs) {
                    char *opt = args[chosen_index]; char tmpopt[200]; strncpy(tmpopt, opt, sizeof(tmpopt)-1); tmpopt[sizeof(tmpopt)-1] = '\0'; trim(tmpopt);
                    if (tmpopt[0] == '"') { size_t lp = strlen(tmpopt); if (lp >= 2 && tmpopt[lp-1] == '"') { tmpopt[lp-1] = '\0'; memmove(tmpopt, tmpopt+1, strlen(tmpopt)); } }
                    set_variable(namebuf, tmpopt);
                } else {
                    int matched = 0;
                    for (int oi = 1; oi < nargs; oi++) {
                        char *opt = args[oi]; char tmpopt[200]; strncpy(tmpopt, opt, sizeof(tmpopt)-1); tmpopt[sizeof(tmpopt)-1] = '\0'; trim(tmpopt);
                        if (tmpopt[0] == '"') { size_t lp = strlen(tmpopt); if (lp >= 2 && tmpopt[lp-1] == '"') { tmpopt[lp-1] = '\0'; memmove(tmpopt, tmpopt+1, strlen(tmpopt)); } }
                        if (strcmp(tmpopt, input_buffer) == 0) { set_variable(namebuf, tmpopt); matched = 1; break; }
                    }
                    if (!matched) set_variable(namebuf, input_buffer);
                }
                for (int ii = 0; ii < nargs; ii++) free(args[ii]);
                do_delay();
                return;
            }
            // fallback generic input behavior
            printf("%s", prompt[0] ? prompt : ">" ); fflush(stdout);
            char input_buffer[200];
            if (!fgets(input_buffer, sizeof(input_buffer), stdin)) input_buffer[0] = '\0';
            input_buffer[strcspn(input_buffer, "\n")] = 0;
            // if var mode, try to validate numeric
            if (input_mode == 1) {
                char *endptr;
                double v = strtod(input_buffer, &endptr);
                // accept if at least one digit consumed and remaining are spaces
                int ok = (endptr != input_buffer);
                while (ok && *endptr) { if (!isspace((unsigned char)*endptr)) { ok = 0; break; } endptr++; }
                if (ok) {
                    // store numeric string (no extra formatting)
                    char numbuf[128];
                    if (fabs(v - round(v)) < 1e-9) snprintf(numbuf, sizeof(numbuf), "%lld", (long long)llround(v));
                    else snprintf(numbuf, sizeof(numbuf), "%g", v);
                    set_variable(namebuf, numbuf);
                } else {
                    // not a pure number, store as raw string
                    set_variable(namebuf, input_buffer);
                }
            } else {
                // string mode
                set_variable(namebuf, input_buffer);
            }
            do_delay();
            return;
        }

        // math(...) on RHS or math.vars(...) on RHS
        if (strncmp(valuebuf, "math.vars(", 10) == 0 || strncmp(valuebuf, "math(", 5) == 0) {
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                // check if this has assignment inside (VAR = expr)
                char *eq = strchr(p, '=');
                if (eq) {
                    // assignment inside: math(D=expr)
                    *eq = '\0';
                    char lhs[64]; char rhs[256];
                    strncpy(lhs, p, sizeof(lhs)-1); lhs[sizeof(lhs)-1] = '\0';
                    strncpy(rhs, eq+1, sizeof(rhs)-1); rhs[sizeof(rhs)-1] = '\0';
                    trim(lhs); trim(rhs);
                    double res;
                    if (!evaluate_math_expr(rhs, &res)) { printf("NVD Error: Invalid math expression.\n"); return; }
                    char outbuf[100];
                    if (fabs(res - round(res)) < 1e-9) snprintf(outbuf, sizeof(outbuf), "%lld", (long long)llround(res));
                    else snprintf(outbuf, sizeof(outbuf), "%g", res);
                    set_variable(lhs, outbuf);
                    return;
                } else {
                    // no assignment inside: D=math(expr) means assign result to D
                    // If the inner content is a simple variable name, evaluate the variable's string value as the expression
                    char exprbuf[512];
                    strncpy(exprbuf, p, sizeof(exprbuf)-1); exprbuf[sizeof(exprbuf)-1] = '\0';
                    trim(exprbuf);
                    int is_simple_var = 0;
                    if (exprbuf[0] && (isalpha((unsigned char)exprbuf[0]) || exprbuf[0] == '_')) {
                        is_simple_var = 1;
                        for (size_t k = 1; k < strlen(exprbuf); k++) {
                            if (!isalnum((unsigned char)exprbuf[k]) && exprbuf[k] != '_') { is_simple_var = 0; break; }
                        }
                    }
                    double res;
                    if (is_simple_var) {
                        const char *varval = get_variable(exprbuf);
                        if (!varval) { printf("NVD Error: Undefined variable for math().\n"); return; }
                        if (!evaluate_math_expr(varval, &res)) { printf("NVD Error: Invalid math expression.\n"); return; }
                    } else {
                        if (!evaluate_math_expr(exprbuf, &res)) { printf("NVD Error: Invalid math expression.\n"); return; }
                    }
                    char outbuf[100];
                    if (fabs(res - round(res)) < 1e-9) snprintf(outbuf, sizeof(outbuf), "%lld", (long long)llround(res));
                    else snprintf(outbuf, sizeof(outbuf), "%g", res);
                    set_variable(namebuf, outbuf);
                    return;
                }
            }
        }

        // quoted string literal
        size_t vlen = strlen(valuebuf);
        if (vlen >= 2 && valuebuf[0] == '"' && valuebuf[vlen-1] == '"') {
            valuebuf[vlen-1] = '\0';
            set_variable(namebuf, valuebuf + 1);
            return;
        }

        // if value is an existing variable, copy its contents
        const char *oth = get_variable(valuebuf);
        if (oth) { set_variable(namebuf, oth); return; }

        // default: set literally
        set_variable(namebuf, valuebuf);
        return;
    }

    // PRINT
    if (strncmp(line, "math.vars", 9) == 0 && (line[9] == '(' || isspace((unsigned char)line[9]) || line[9] == '\0')) {
        execute_math(line);
        return;
    }

    if (strncmp(line, "math", 4) == 0 && (line[4] == '(' || isspace((unsigned char)line[4]) || line[4] == '\0')) {
        execute_math(line);
        return;
    }

    if (strncmp(line, "print", 5) == 0) {
        execute_print(line);
        return;
    }

    // GOTO: jump to a named void-block and execute it
    if (strncmp(line, "goto ", 5) == 0) {
        char name[64]; strncpy(name, line + 5, sizeof(name)-1); name[sizeof(name)-1] = '\0'; trim(name);
        const char *body = find_named_block_body(name);
        if (!body) {
            printf("NVD Error: Undefined label '%s'.\n", name);
            return;
        }
        // write body to a temporary stream and interpret it so nested blocks/ifs work
        FILE *tmp = tmpfile();
        if (!tmp) {
            printf("NVD Error: Could not open temporary buffer for goto.\n");
            return;
        }
        fwrite(body, 1, strlen(body), tmp);
        rewind(tmp);
        interpret_stream(tmp, NULL, 1);
        fclose(tmp);
        return;
    }

    // standalone sys.command("...") -> execute and print exit code (or nothing)
    if (strncmp(line, "sys.command(", 12) == 0) {
        char *p = strchr(line, '(');
        if (p) {
            p++;
            char *q = strrchr(p, ')');
            if (q) *q = '\0';
            char cmdbuf[512]; strncpy(cmdbuf, p, sizeof(cmdbuf)-1); cmdbuf[sizeof(cmdbuf)-1] = '\0'; trim(cmdbuf);
            size_t cl = strlen(cmdbuf);
            if (cl >= 2 && cmdbuf[0] == '"' && cmdbuf[cl-1] == '"') { cmdbuf[cl-1] = '\0'; memmove(cmdbuf, cmdbuf+1, strlen(cmdbuf)); }
            execute_sys_command(cmdbuf);
        }
        return;
    }
}

// Evaluate a simple condition string like "A < B" or "x==y" or "A=5" (treat single = as ==)
int eval_condition(const char *cond) {
    char buf[512]; strncpy(buf, cond, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0'; trim(buf);
    // look for operators: try double-operators first, then singles
    const char *ops[] = {"==","!=","<=",">=","<",">","=", NULL};
    for (int i = 0; ops[i]; i++) {
        char *pos = strstr(buf, ops[i]);
        if (pos) {
            char left[256]; char right[256];
            int olen = strlen(ops[i]);
            strncpy(left, buf, pos - buf); left[pos-buf] = '\0';
            strncpy(right, pos + olen, sizeof(right)-1); right[sizeof(right)-1] = '\0';
            trim(left); trim(right);
            // try numeric compare
            double a, b;
            int anum = evaluate_math_expr(left, &a);
            int bnum = evaluate_math_expr(right, &b);
            if (anum && bnum) {
                if (strcmp(ops[i], "==") == 0 || strcmp(ops[i], "=") == 0) return fabs(a-b) < 1e-9;
                if (strcmp(ops[i], "!=") == 0) return fabs(a-b) >= 1e-9;
                if (strcmp(ops[i], "<=") == 0) return a <= b;
                if (strcmp(ops[i], ">=") == 0) return a >= b;
                if (strcmp(ops[i], "<") == 0) return a < b;
                if (strcmp(ops[i], ">") == 0) return a > b;
            } else {
                // string compare for == and !=
                const char *lv = left; const char *rv = right;
                const char *vl = get_variable(left); if (vl) lv = vl;
                const char *vr = get_variable(right); if (vr) rv = vr;
                if (strcmp(ops[i], "==") == 0) return strcmp(lv, rv) == 0;
                if (strcmp(ops[i], "!=") == 0) return strcmp(lv, rv) != 0;
                // other operators not supported on strings
                return 0;
            }
        }
    }
    // if no operator, evaluate as truthy number or non-empty string
    double v;
    if (evaluate_math_expr(buf, &v)) return fabs(v) > 1e-9;
    const char *s = get_variable(buf);
    if (s) return strlen(s) > 0;
    return 0;
}

// collect a block starting at the position after the opening '{'
char *collect_block(FILE *file, char *after_brace) {
    // allocate a dynamic buffer
    size_t cap = 4096; size_t len = 0; char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    int depth = 1;
    // if after_brace contains remainder text, include it
    if (after_brace && *after_brace) {
        strcat(out, after_brace);
        strcat(out, "\n");
    }
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // update depth by counting braces
        for (char *p = line; *p; ++p) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
        }
        size_t l = strlen(line);
        if (len + l + 1 > cap) { cap *= 2; out = realloc(out, cap); }
        strcat(out, line);
        len += l;
        if (depth == 0) break;
    }
    return out;
}

// interpret a stream starting from first_line; parent_exec controls whether outer blocks execute
void interpret_stream(FILE *file, char *first_line, int parent_exec) {
    char linebuf[512];
    if (first_line) strncpy(linebuf, first_line, sizeof(linebuf)-1);
    else {
        if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
        else if (!fgets(linebuf, sizeof(linebuf), file)) return;
    }
    while (1) {
        if (!linebuf[0]) {
            if (have_pushback) {
                strncpy(linebuf, pushback_line, sizeof(linebuf)-1);
                have_pushback = 0;
            } else if (!fgets(linebuf, sizeof(linebuf), file)) break;
        }
        char tline[512]; strncpy(tline, linebuf, sizeof(tline)-1); tline[sizeof(tline)-1] = '\0'; trim(tline);
        if (strlen(tline) == 0) { linebuf[0] = '\0'; continue; }
        // handle void blocks: `void name { ... }` -> store named block, don't execute immediately
        if (strncmp(tline, "void ", 5) == 0) {
            // require '{' on the same line for simplicity
            char name[64] = "";
            char *brace = strchr(linebuf, '{');
            if (brace) {
                char *p = linebuf + 4; // after 'void'
                while (*p && isspace((unsigned char)*p)) p++;
                int j = 0;
                while (*p && !isspace((unsigned char)*p) && *p != '{' && j < (int)sizeof(name)-1) name[j++] = *p++;
                name[j] = '\0'; trim(name);
                char *after = brace + 1;
                char *blk = collect_block(file, after);
                if (blk) {
                    store_named_block(name, blk);
                    free(blk);
                }
                // prepare next input line
                linebuf[0] = '\0';
                if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
                else if (!fgets(linebuf, sizeof(linebuf), file)) break;
                continue;
            }
        }

        // if starts with if
        if ((strncmp(tline, "if ", 3) == 0) || (strncmp(tline, "if(", 3) == 0) || (strncmp(tline, "if\t",3)==0)) {
            // find condition
            char condbuf[256] = "";
            char *brace = strchr(linebuf, '{');
            if (brace) {
                // cond is between 'if' and '{'
                char *start = strstr(linebuf, "if"); if (start) start += 2;
                int upto = brace - linebuf;
                int len = upto - (start - linebuf);
                if (len > 0 && len < (int)sizeof(condbuf)) strncpy(condbuf, start, len);
                condbuf[len] = '\0';
            } else {
                char *p = strchr(linebuf, '(');
                char *q = strrchr(linebuf, ')');
                if (p && q && q > p) { strncpy(condbuf, p+1, q-p-1); condbuf[q-p-1] = '\0'; }
            }
            trim(condbuf);
            // strip outer parentheses if present
            if (condbuf[0] == '(' && condbuf[strlen(condbuf)-1] == ')') {
                condbuf[strlen(condbuf)-1] = '\0';
                memmove(condbuf, condbuf+1, strlen(condbuf));
            }
            char *after = NULL;
            if (brace) after = brace + 1;
            char *if_block = collect_block(file, after);
            int taken = 0;
            if (parent_exec && eval_condition(condbuf)) {
                // execute block
                taken = 1;
                    char *ln = strtok(if_block, "\n");
                    while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
            }
            free(if_block);
            
            // Handle else if/else chain
            while (1) {
                char look[512]; 
                if (!fgets(look, sizeof(look), file)) break;
                char tlook[512]; strncpy(tlook, look, sizeof(tlook)-1); tlook[sizeof(tlook)-1] = '\0'; trim(tlook);
                
                if (strncmp(tlook, "else if", 7) == 0 || strncmp(tlook, "elseif", 6) == 0) {
                    // Parse else if condition
                    char elseif_cond[256] = "";
                    char *brace = strchr(look, '{');
                    if (brace) {
                        char *start = strstr(look, "if"); if (start) start += 2;
                        int upto = brace - look;
                        int len = upto - (start - look);
                        if (len > 0 && len < (int)sizeof(elseif_cond)) strncpy(elseif_cond, start, len);
                        elseif_cond[len] = '\0';
                    } else {
                        char *p = strchr(look, '(');
                        char *q = strrchr(look, ')');
                        if (p && q && q > p) { strncpy(elseif_cond, p+1, q-p-1); elseif_cond[q-p-1] = '\0'; }
                    }
                    trim(elseif_cond);
                    if (elseif_cond[0] == '(' && elseif_cond[strlen(elseif_cond)-1] == ')') {
                        elseif_cond[strlen(elseif_cond)-1] = '\0';
                        memmove(elseif_cond, elseif_cond+1, strlen(elseif_cond));
                    }
                    
                    char *after = NULL;
                    if (brace) after = brace + 1;
                    char *elseif_block = collect_block(file, after);
                    
                    // Execute if not taken and condition is true
                    if (!taken && parent_exec && eval_condition(elseif_cond)) {
                        taken = 1;
                        char *ln = strtok(elseif_block, "\n");
                        while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
                    }
                    free(elseif_block);
                    // Continue to next else if/else
                    continue;
                    
                } else if (strncmp(tlook, "else", 4) == 0) {
                    // Found else block
                    char *bpos = strchr(look, '{');
                    char *after = NULL;
                    if (bpos) after = bpos + 1;  // Content after the {
                    char *else_block = collect_block(file, after);
                    
                    // Execute only if not taken
                    if (!taken && parent_exec && else_block) {
                        char *ln = strtok(else_block, "\n");
                        while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
                    }
                    if (else_block) free(else_block);
                    break;  // else block is final, exit loop
                    
                } else {
                    // Not an else/else if, push back and exit
                    strncpy(pushback_line, look, sizeof(pushback_line)-1); 
                    have_pushback = 1;
                    break;
                }
            }
            // After handling if/else chain, prepare for next iteration
            linebuf[0] = '\0';
            if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
            else if (!fgets(linebuf, sizeof(linebuf), file)) break;
            continue;
        }
        // not an if-block: simple line
        if (strncmp(tline, "else", 4) != 0) {
            interpret_line_simple(file, linebuf);
            do_delay();
        }
        linebuf[0] = '\0';
        if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
        else if (!fgets(linebuf, sizeof(linebuf), file)) break;
    }
}

void run_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("NVD Error: Could not open file %s\n", filename);
        exit(1);
    }
    have_pushback = 0;
    interpret_stream(file, NULL, 1);  // Call once with NULL to let interpret_stream read all lines
    fclose(file);
}

// Shell helpers
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
            if (*cmd) execute_sys_command(cmd);
            else printf("Usage: exec <system-command>\n");
            continue;
        }
        // fallback: try to interpret a single line of the scripting language
        interpret_line_simple(NULL, line);
        do_delay();
    }
}

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