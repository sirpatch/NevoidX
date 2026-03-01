#include "NVXScript.h"
#include "NVXVars.h"
#include "NVXMath.h"
#include "NVXJSON.h"
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

void trim(char *s) { ltrim(s); rtrim(s); }

// Delay helper - resets delay after use so it only affects one statement
void do_delay() {
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

// forward declare stream interpreter
void interpret_stream(FILE *file, char *first_line, int parent_exec);

// Execute a system command via the host shell. Returns exit code.
int execute_sys_command(const char *cmd) {
    if (!cmd) return -1;
    int rc = system(cmd);
    return rc;
}

void execute_print(char *line) {
    char *start = strchr(line, '(');
    if (!start) return;
    start++;

    char *end = strrchr(start, ')');
    if (!end) return;
    *end = '\0';
    
    char full_content[1024];
    strncpy(full_content, start, sizeof(full_content)-1); full_content[sizeof(full_content)-1] = '\0';
    trim(full_content);

    int i = 0;
    while (full_content[i]) {
        while (full_content[i] && isspace((unsigned char)full_content[i])) i++;
        if (!full_content[i]) break;

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
        while (j > 0 && isspace((unsigned char)arg[j-1])) arg[--j] = '\0';

        if (full_content[i] == ',') i++;

        if (strlen(arg) > 0) {
            char trimmed_arg[512];
            strncpy(trimmed_arg, arg, sizeof(trimmed_arg)-1); trimmed_arg[sizeof(trimmed_arg)-1] = '\0';
            trim(trimmed_arg);
            size_t len = strlen(trimmed_arg);
            if (len >= 2 && trimmed_arg[0] == '"' && trimmed_arg[len - 1] == '"') {
                for (size_t k = 1; k < len - 1; k++) {
                    putchar(trimmed_arg[k]);
                }
            }
            else if (isalpha((unsigned char)trimmed_arg[0]) || trimmed_arg[0] == '_') {
                int is_simple_var = 1;
                for (size_t k = 0; k < len; k++) {
                    if (!isalnum((unsigned char)trimmed_arg[k]) && trimmed_arg[k] != '_') { is_simple_var = 0; break; }
                }
                if (is_simple_var) {
                    int vtype = get_var_type(trimmed_arg);
                    if (vtype == 2 || vtype == 3) {
                        const char *value = get_variable(trimmed_arg);
                        if (value) {
                            printf("%s", value);
                        }
                    } else {
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
            else {
                double dres;
                if (evaluate_math_expr(trimmed_arg, &dres)) {
                    if (fabs(dres - round(dres)) < 1e-9) printf("%lld", (long long)llround(dres));
                    else printf("%g", dres);
                } else if (strncmp(trimmed_arg, "nvx.json_get(", 13) == 0) {
                    // evaluate json_get in print context; ignore quotes
                    char copy[1024]; strncpy(copy, trimmed_arg, sizeof(copy)-1); copy[sizeof(copy)-1]='\0';
                    // reuse assignment logic by temporarily storing to temp var
                    // naive: call function and capture output
                    char *p = strchr(copy,'(');
                    if (p) {
                        p++;
                        char *q = strrchr(p,')'); if(q)*q='\0';
                        char jsonstr[1024]=""; char keystr[256]="";
                        int inq=0, idx=-1;
                        for(int i=0; p[i]; i++){
                            if(p[i]=='"') inq=!inq;
                            if(p[i]==',' && !inq){idx=i;break;}
                        }
                        if(idx>=0){strncpy(jsonstr,p,idx);jsonstr[idx]='\0'; strncpy(keystr,p+idx+1,sizeof(keystr)-1);keystr[sizeof(keystr)-1]='\0';}
                        else strncpy(jsonstr,p,sizeof(jsonstr)-1);
                        trim(jsonstr); trim(keystr);
                        if(jsonstr[0]=='"' && jsonstr[strlen(jsonstr)-1]=='"'){memmove(jsonstr,jsonstr+1,strlen(jsonstr)); jsonstr[strlen(jsonstr)-1]='\0';}
                        if(keystr[0]=='"' && keystr[strlen(keystr)-1]=='"'){memmove(keystr,keystr+1,strlen(keystr)); keystr[strlen(keystr)-1]='\0';}
                        char outbuf[1024]="";
                        if(nvx_json_get(jsonstr,keystr,outbuf,sizeof(outbuf))) printf("%s", outbuf);
                    }
                } else {
                    const char *value = get_variable(trimmed_arg);
                    if (value) {
                        printf("%s", value);
                    }
                }
            }
        }
    }
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

    double res;
    if (evaluate_math_expr(start, &res)) {
        if (fabs(res - round(res)) < 1e-9) printf("%lld\n", (long long)llround(res));
        else printf("%g\n", res);
        return;
    }
    printf("NVD Error: Invalid math statement.\n");
    do_delay();
}

static char pushback_line[512];
static int have_pushback = 0;

int eval_condition(const char *cond) {
    char buf[512]; strncpy(buf, cond, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0'; trim(buf);
    const char *ops[] = {"==","!=","<=",">=","<",">","=", NULL};
    for (int i = 0; ops[i]; i++) {
        char *pos = strstr(buf, ops[i]);
        if (pos) {
            char left[256]; char right[256];
            int olen = strlen(ops[i]);
            strncpy(left, buf, pos - buf); left[pos-buf] = '\0';
            strncpy(right, pos + olen, sizeof(right)-1); right[sizeof(right)-1] = '\0';
            trim(left); trim(right);
            double a, b;
            int anum = evaluate_math_expr(left, &a);
            int bnum = evaluate_math_expr(right, &b);
            if (anum && bnum) {
                if (strcmp(ops[i], "==") == 0 || strcmp(ops[i], "=") == 0) return fabs(a-b) < 1e-9;
                if (strcmp(ops[i], "!=") == 0) return fabs(a-b) >= 1e-9;
                if (strcmp(ops[i], "<=") == 0) return a <= b;
                if (strcmp(ops[i], ">=") == 0) return a >= b;
                if (strcmp(ops[i], "<") == 0) return a < b;
                if (strcmp(ops[i],">") == 0) return a > b;
            } else {
                const char *lv = left; const char *rv = right;
                const char *vl = get_variable(left); if (vl) lv = vl;
                const char *vr = get_variable(right); if (vr) rv = vr;
                if (strcmp(ops[i], "==") == 0) return strcmp(lv, rv) == 0;
                if (strcmp(ops[i], "!=") == 0) return strcmp(lv, rv) != 0;
                return 0;
            }
        }
    }
    double v;
    if (evaluate_math_expr(buf, &v)) return fabs(v) > 1e-9;
    const char *s = get_variable(buf);
    if (s) return strlen(s) > 0;
    return 0;
}

char *collect_block(FILE *file, char *after_brace) {
    size_t cap = 4096; size_t len = 0; char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    int depth = 1;
    if (after_brace && *after_brace) {
        strcat(out, after_brace);
        strcat(out, "\n");
    }
    char line[512];
    while (fgets(line, sizeof(line), file)) {
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
        if (strncmp(tline, "void ", 5) == 0) {
            char name[64] = "";
            char *brace = strchr(linebuf, '{');
            if (brace) {
                char *p = linebuf + 4;
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
                linebuf[0] = '\0';
                if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
                else if (!fgets(linebuf, sizeof(linebuf), file)) break;
                continue;
            }
        }
        if ((strncmp(tline, "if ", 3) == 0) || (strncmp(tline, "if(", 3) == 0) || (strncmp(tline, "if\t",3)==0)) {
            char condbuf[256] = "";
            char *brace = strchr(linebuf, '{');
            if (brace) {
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
            if (condbuf[0] == '(' && condbuf[strlen(condbuf)-1] == ')') {
                condbuf[strlen(condbuf)-1] = '\0';
                memmove(condbuf, condbuf+1, strlen(condbuf));
            }
            char *after = NULL;
            if (brace) after = brace + 1;
            char *if_block = collect_block(file, after);
            int taken = 0;
            if (parent_exec && eval_condition(condbuf)) {
                taken = 1;
                    char *ln = strtok(if_block, "\n");
                    while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
            }
            free(if_block);
            while (1) {
                char look[512]; 
                if (!fgets(look, sizeof(look), file)) break;
                char tlook[512]; strncpy(tlook, look, sizeof(tlook)-1); tlook[sizeof(tlook)-1] = '\0'; trim(tlook);
                
                if (strncmp(tlook, "else if", 7) == 0 || strncmp(tlook, "elseif", 6) == 0) {
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
                    
                    if (!taken && parent_exec && eval_condition(elseif_cond)) {
                        taken = 1;
                        char *ln = strtok(elseif_block, "\n");
                        while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
                    }
                    free(elseif_block);
                    continue;
                    
                } else if (strncmp(tlook, "else", 4) == 0) {
                    char *bpos = strchr(look, '{');
                    char *after = NULL;
                    if (bpos) after = bpos + 1;
                    char *else_block = collect_block(file, after);
                    
                    if (!taken && parent_exec && else_block) {
                        char *ln = strtok(else_block, "\n");
                        while (ln) { interpret_line_simple(file, ln); do_delay(); ln = strtok(NULL, "\n"); }
                    }
                    if (else_block) free(else_block);
                    break;
                } else {
                    strncpy(pushback_line, look, sizeof(pushback_line)-1); 
                    have_pushback = 1;
                    break;
                }
            }
            linebuf[0] = '\0';
            if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
            else if (!fgets(linebuf, sizeof(linebuf), file)) break;
            continue;
        }
        if (strncmp(tline, "else", 4) != 0) {
            interpret_line_simple(file, linebuf);
            do_delay();
        }
        linebuf[0] = '\0';
        if (have_pushback) { strncpy(linebuf, pushback_line, sizeof(linebuf)-1); have_pushback = 0; }
        else if (!fgets(linebuf, sizeof(linebuf), file)) break;
    }
}

void interpret_line_simple(FILE *file, char *line) {
    line[strcspn(line, "\n")] = 0;
    trim(line);
    if (strlen(line) == 0)
        return;
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
    if (strncmp(line, "def.var=", 8) == 0) {
        char *list = line + 8;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 1); // type 1 = numeric var
            }
            if (buf[i] == ',') i++;
        }
        return;
    }
    if (strncmp(line, "def.str=", 8) == 0) {
        char *list = line + 8;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 2); // type 2 = string
            }
            if (buf[i] == ',') i++;
        }
        return;
    }
    if (strncmp(line, "def.math=", 9) == 0) {
        char *list = line + 9;
        char buf[512];
        strncpy(buf, list, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
        trim(buf);
        int i = 0;
        while (buf[i]) {
            char var_name[64];
            int j = 0;
            while (buf[i] && isspace((unsigned char)buf[i])) i++;
            while (buf[i] && buf[i] != ',' && j < (int)sizeof(var_name)-1) {
                var_name[j++] = buf[i++];
            }
            while (j > 0 && isspace((unsigned char)var_name[j-1])) j--;
            var_name[j] = '\0';
            if (j > 0) {
                set_var_type(var_name, 3); // type 3 = math (store expression string)
            }
            if (buf[i] == ',') i++;
        }
        return;
    }
    if (strncmp(line, "delay=", 6) == 0) {
        char *val = line + 6;
        char valcopy[64];
        strncpy(valcopy, val, sizeof(valcopy)-1); valcopy[sizeof(valcopy)-1] = '\0';
        trim(valcopy);
        int delay_val = atoi(valcopy);
        if (delay_val >= 0) script_delay = delay_val;
        return;
    }
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

        if (strncmp(valuebuf, "sys.command(", 12) == 0) {
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                char cmdbuf[512]; strncpy(cmdbuf, p, sizeof(cmdbuf)-1); cmdbuf[sizeof(cmdbuf)-1] = '\0'; trim(cmdbuf);
                size_t cl = strlen(cmdbuf);
                if (cl >= 2 && cmdbuf[0] == '"' && cmdbuf[cl-1] == '"') { cmdbuf[cl-1] = '\0'; memmove(cmdbuf, cmdbuf+1, strlen(cmdbuf)); }
                int rc = execute_sys_command(cmdbuf);
                char rcbuf[64]; snprintf(rcbuf, sizeof(rcbuf), "%d", rc);
                set_variable(namebuf, rcbuf);
                return;
            }
        }
        // HTTP helper calls
        if (strncmp(valuebuf, "nvx.http_get(", 13) == 0 || strncmp(valuebuf, "nvx.http_post(", 14) == 0) {
            int isPost = (strncmp(valuebuf, "nvx.http_post(", 14) == 0);
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                // split on comma outside quotes
                char url[1024] = "";
                char body[2048] = "";
                int inq = 0; int idx = -1;
                for (int i = 0; p[i]; i++) {
                    if (p[i] == '"') inq = !inq;
                    if (p[i] == ',' && !inq) { idx = i; break; }
                }
                if (idx >= 0) {
                    strncpy(url, p, idx); url[idx] = '\0';
                    strncpy(body, p + idx + 1, sizeof(body)-1); body[sizeof(body)-1] = '\0';
                } else {
                    strncpy(url, p, sizeof(url)-1); url[sizeof(url)-1] = '\0';
                }
                trim(url); trim(body);
                // remove surrounding quotes
                if (url[0] == '"' && url[strlen(url)-1] == '"') {
                    memmove(url, url+1, strlen(url));
                    url[strlen(url)-1] = '\0';
                }
                if (body[0] == '"' && body[strlen(body)-1] == '"') {
                    memmove(body, body+1, strlen(body));
                    body[strlen(body)-1] = '\0';
                }
                char resp[4096] = "";
                int rc;
                if (isPost) rc = nvx_http_post(url, body, resp, sizeof(resp));
                else rc = nvx_http_get(url, resp, sizeof(resp));
                if (rc >= 0) {
                    set_variable(namebuf, resp);
                    set_var_type(namebuf, 2); // mark as string so print() won't treat as numeric expression
                }
                return;
            }
        }
        // JSON helper call
        if (strncmp(valuebuf, "nvx.json_get(", 13) == 0) {
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                // split into json and key
                char jsonstr[2048] = "";
                char keystr[256] = "";
                int inq = 0; int idx = -1;
                for (int i = 0; p[i]; i++) {
                    if (p[i] == '"') inq = !inq;
                    if (p[i] == ',' && !inq) { idx = i; break; }
                }
                if (idx >= 0) {
                    strncpy(jsonstr, p, idx); jsonstr[idx] = '\0';
                    strncpy(keystr, p + idx + 1, sizeof(keystr)-1); keystr[sizeof(keystr)-1] = '\0';
                } else {
                    strncpy(jsonstr, p, sizeof(jsonstr)-1); jsonstr[sizeof(jsonstr)-1] = '\0';
                }
                trim(jsonstr); trim(keystr);
                if (jsonstr[0]=='\"' && jsonstr[strlen(jsonstr)-1]=='\"') {
                    memmove(jsonstr,jsonstr+1,strlen(jsonstr));
                    jsonstr[strlen(jsonstr)-1]='\0';
                } else {
                    const char *vv = get_variable(jsonstr);
                    if (vv) {
                        strncpy(jsonstr, vv, sizeof(jsonstr)-1);
                        jsonstr[sizeof(jsonstr)-1] = '\0';
                    }
                }
                if (keystr[0]=='\"' && keystr[strlen(keystr)-1]=='\"') {
                    memmove(keystr,keystr+1,strlen(keystr));
                    keystr[strlen(keystr)-1]='\0';
                }
                char outbuf[1024] = "";
                if (nvx_json_get(jsonstr, keystr, outbuf, sizeof(outbuf))) {
                    set_variable(namebuf, outbuf);
                    set_var_type(namebuf, 2);
                }
                return;
            }
        }
        int input_mode = 0; 
        if (strncmp(valuebuf, "user.input_var(", 15) == 0 || strncmp(valuebuf, "input_var(", 10) == 0) input_mode = 1;
        else if (strncmp(valuebuf, "user.input_str(", 15) == 0 || strncmp(valuebuf, "input_str(", 10) == 0) input_mode = 2;
        else if (strncmp(valuebuf, "user.input(", 11) == 0 || strncmp(valuebuf, "input(", 6) == 0) input_mode = 2;
        else if (strncmp(valuebuf, "user.input_math(", 16) == 0 || strncmp(valuebuf, "input_math(", 11) == 0) input_mode = 3;
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
                size_t plen = strlen(prompt);
                if (plen >= 2 && prompt[0] == '"' && prompt[plen-1] == '"') {
                    prompt[plen-1] = '\0';
                    memmove(prompt, prompt+1, strlen(prompt));
                }
            }
            if (input_mode == 4 || input_mode == 5) {
                char argsbuf[512];
                strncpy(argsbuf, p, sizeof(argsbuf)-1); argsbuf[sizeof(argsbuf)-1] = '\0';
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
            printf("%s", prompt[0] ? prompt : ">" ); fflush(stdout);
            char input_buffer[200];
            if (!fgets(input_buffer, sizeof(input_buffer), stdin)) input_buffer[0] = '\0';
            input_buffer[strcspn(input_buffer, "\n")] = 0;
            if (input_mode == 1) {
                char *endptr;
                double v = strtod(input_buffer, &endptr);
                int ok = (endptr != input_buffer);
                while (ok && *endptr) { if (!isspace((unsigned char)*endptr)) { ok = 0; break; } endptr++; }
                if (ok) {
                    char numbuf[128];
                    if (fabs(v - round(v)) < 1e-9) snprintf(numbuf, sizeof(numbuf), "%lld", (long long)llround(v));
                    else snprintf(numbuf, sizeof(numbuf), "%g", v);
                    set_variable(namebuf, numbuf);
                } else {
                    set_variable(namebuf, input_buffer);
                }
            } else {
                set_variable(namebuf, input_buffer);
            }
            do_delay();
            return;
        }
        if (strncmp(valuebuf, "math.vars(", 10) == 0 || strncmp(valuebuf, "math(", 5) == 0) {
            char *p = strchr(valuebuf, '(');
            if (p) {
                p++;
                char *q = strrchr(p, ')');
                if (q) *q = '\0';
                char *eq = strchr(p, '=');
                if (eq) {
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
        size_t vlen = strlen(valuebuf);
        if (vlen >= 2 && valuebuf[0] == '"' && valuebuf[vlen-1] == '"') {
            valuebuf[vlen-1] = '\0';
            set_variable(namebuf, valuebuf + 1);
            return;
        }
        const char *oth = get_variable(valuebuf);
        if (oth) { set_variable(namebuf, oth); return; }
        set_variable(namebuf, valuebuf);
        return;
    }
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
    if (strncmp(line, "goto ", 5) == 0) {
        char name[64]; strncpy(name, line + 5, sizeof(name)-1); name[sizeof(name)-1] = '\0'; trim(name);
        const char *body = find_named_block_body(name);
        if (!body) {
            printf("NVD Error: Undefined label '%s'.\n", name);
            return;
        }
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

void run_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("NVD Error: Could not open file %s\n", filename);
        exit(1);
    }
    have_pushback = 0;
    interpret_stream(file, NULL, 1);
    fclose(file);
}
