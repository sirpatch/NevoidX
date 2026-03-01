#include "NVXMath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

// Expression evaluator using shunting-yard -> RPN evaluation

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
            int check_pos = pos;
            while (s[check_pos] && isspace((unsigned char)s[check_pos])) check_pos++;
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
        char c = s[pos];
        if (c=='+'||c=='-'||c=='*'||c=='/'||c=='%'||c=='^') {
            out[idx].type = T_OP; out[idx].op = c; prev = out[idx]; idx++; pos++; continue;
        }
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
        if (t.type == T_FUNC) { ops[ops_top++] = t; continue; }
        if (t.type == T_OP) {
            char op = t.op;
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
                if (top.type == T_FUNC) { out[out_i++] = top; break; }
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

extern const char* get_variable(const char *name); // forward from vars

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
