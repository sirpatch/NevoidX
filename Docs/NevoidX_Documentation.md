# NevoidX Interpreter Documentation

This document provides an overview of the NevoidX scripting language engine implemented in `src/NevoidX.c`.

## Overview

NevoidX is a small scripting interpreter written in C. It supports:

- Variable declarations and assignments (numeric, string, math-expression types)
- Arithmetic and mathematical expression evaluation
- Built-in math functions (sin, cos, tan, ... including extended set)
- Conditional blocks (`if`, `else if`, `else`)
- Named blocks and `goto`
- Basic I/O commands like `print`, `user.input*`, `user.choice*`
- Delay control for pacing scripts
- Simple shell-like interactive mode with `run`, `source`, `exec`, `vars`, `types` commands

## File Structure (`NevoidX.c`)

The source file implements the interpreter in a single compilation unit. Key sections:

### Data Structures
- `Variable`: stores name/value pairs for variables.
- `VarType`: tracks declared variable types (numeric, string, math-expr).
- `NamedBlock`: holds named code blocks defined via `void name { ... }`.
  - `variables`, `var_types`, `named_blocks` arrays hold the data.

### Utility Functions
- String trimming helpers: `ltrim`, `rtrim`, `trim`.

### Variable Management
- `set_var_type`, `get_var_type` for type tracking.
- `set_variable`, `get_variable` for value storage and retrieval.

### Math Expression Evaluator
A shunting-yard-based evaluator supporting:
- Tokenization (`tokenize`)
- Conversion to Reverse Polish Notation (`shunting_yard`)
- RPN evaluation (`evaluate_rpn`)
- High-level entrypoint: `evaluate_math_expr`

Supported operators: `+ - * / % ^` and unary minus.

Extended math functions are recognized by `is_known_func` and handled by
`apply_func` (single-argument) and `apply_func2` (two-argument functions such as
`min`, `max`, `atan2`, `hypot`, `mod`).

### Script Execution
- `execute_print`, `execute_math` handle the built-in commands for printing and
evaluating expressions, with support for variable references and quoted strings.

- `interpret_line_simple` is responsible for interpreting a single
(non-block) line of code:
  - `def.var`, `def.str`, `def.math` declarations
  - `delay` settings
  - assignments (including system commands, input helpers, `math(...)` expressions)
  - control structures (`print`, `math`, `goto`, `sys.command`)

- Blocks (`if`, `else if`, `else`) are parsed and executed by
  `interpret_stream` which reads from a file-like stream and handles nesting.
  It also manages named `void` blocks and supports the `goto` command by
  storing and later executing block bodies.

### Shell Mode
Functions provide interactive shell features:
- `print_shell_help`, `list_variables`, `list_var_types`, `start_shell`.
- The main function parses `--shell` flag and either executes a file or starts
a REPL.

### Main Entry Point
`main` handles command-line arguments, invoking `run_file` or entering shell mode.

## Using the Interpreter

Compile the source and link the math library:

```sh
gcc -o build/NevoidX.exe src/NevoidX.c -lm
```

Run a script or start an interactive shell:

```sh
build/NevoidX.exe script.nvx
build/NevoidX.exe --shell
```

### Writing Scripts

Scripts are plain text files with one command per line. Supported language
constructs include declarations, assignments, control flow, math expressions,
and I/O. Below is a sample script demonstrating common patterns:

```nvx
# declare variables
 def.var=x,y
 def.str=name
 def.math=expr

# assign literal values
 x=10
 y=20
 name="Alice"

# perform math and assign
 z=math(x+y*sin(3.14/4))

# store expression in a variable and re-use
 expr="x^2+y^2"
 result=math(vars(expr))

# conditional logic
 if(z > 0) {
     print("Positive result", z)
 }else {
     print("Non‑positive")
 }

# named block and goto
 void repeat {
     print("Looping")
     goto repeat
 }
```

### Developer Notes and Extension Points

- **Variable management**: `set_variable` and `get_variable` handle storage. To
  increase limits, adjust `variables` array size.
- **Type tracking**: `var_types` tells if a name is numeric, string or math-
  expression; new categories can be added by editing `VarType` and
  `get_var_type`/`set_var_type`.
- **Expression evaluator**: Uses `tokenize` → `shunting_yard` → `evaluate_rpn`.
  Add new operators or functions by updating `precedence`, `is_known_func`,
  `apply_func`, and `apply_func2`.
- **Commands parser**: `interpret_line_simple` contains the logic for each line
  type (defs, assignments, print/math, goto, sys.command, inputs). New commands
  can be added by adding branches here and implementing helper functions.
- **Block handling**: `interpret_stream` reads lines and processes `if`/`else`
  constructs and named blocks. Modifying it allows support for `while`, `for`,
  or other structures.
- **Shell interface**: Functions `start_shell`, `print_shell_help`, etc. provide
  the REPL; you can extend commands by modifying the loop in `start_shell`.

#### Adding a New Math Function
1. Add the function name to the list in `is_known_func`.
2. Implement the computation in `apply_func` (single argument) or
   `apply_func2` (two arguments).

Example – add `cbrt` (cube root):

```diff
static int is_known_func(const char *name) {
-    static const char *funcs[] = {"sin", ... , NULL};
+    static const char *funcs[] = {"sin", ..., "cbrt", NULL};
@@
static double apply_func(const char *fname, double arg) {
    if (strcmp(fname, "sin") == 0) return sin(arg);
+    if (strcmp(fname, "cbrt") == 0) return cbrt(arg);
    // ...
}
```

### Future Enhancements

- Modularize the interpreter into multiple source files for maintainability.
- Improve error reporting with line numbers and context.
- Add support for user-defined functions or arrays.
- Implement additional input/output primitives or file operations.

---

This document is aimed at developers who wish to maintain or extend the
NevoidX interpreter or create scripts against it. It provides conceptual
background, code pointers, and examples to get started.
## Extended Math Support
The evaluator now includes additional math functions:

- Trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`,
  `cosh`, `tanh`, `asinh`, `acosh`, `atanh`.
- Square root and hypot: `sqrt`, `hypot`.
- Absolute: `abs`.
- Rounding: `floor`, `ceil`, `round`.
- Logarithms: `log` (base 10), `log2`, `ln` (natural log), `exp`.
- Two-argument utilities: `min`, `max`, `mod`.

Use them in expressions like `math(sqrt(16) + atan2(y, x))`.

## JSON Support
New JSON helper module (`NVXJSON`) provides simple encoding and parsing
of flat objects. Functions are available to scripts via the registration
mechanism or c‑level API once you add bindings.

### API
- `nvx_json_pair(out, size, key, value)` – format a single pair.
- `nvx_json_object(out, size, pairs)` – build object from null‑terminated
  key/value list.
- `nvx_json_get(json, key, out, size)` – retrieve value by key.

Example usage in C:
```c
char buf[256];
const char *pairs[] = {"name","Alice","age","30", NULL};
nvx_json_object(buf, sizeof buf, pairs);
// buf == "{\"name\":\"Alice\",\"age\":\"30\"}"
char val[64];
nvx_json_get(buf, "age", val, sizeof val); // val == "30"
```

## HTTP Client and Server
Networking is handled by two new modules.

### `NVXRequests` (client)
Provides `nvx_http_get` and `nvx_http_post` over plain sockets.

```
char resp[4096];
if (nvx_http_get("http://example.com/", resp, sizeof resp) > 0) {
    print(resp);
}
```

### `NVXNet` (server)
You can register handlers for paths and start a blocking server:

```c
void hello(const char *body, char *resp, size_t sz) {
    snprintf(resp, sz, "Hello world!");
}

int main(void) {
    nvx_register_route("/hello", hello);
    nvx_run_server("8080");
}
```

Requests module examples (from script):
```
# in script may call external C via built-in command extension
# http_get returns the response body as a string variable (the interpreter marks it as string,
# so print() will not try to evaluate it as a math expression).
"result"=nvx.http_get("http://localhost:8080/hello")
print(result)
```

## Build and Integration Notes
The project is now split across multiple `.c`/`.h` files:

```
src/NevoidX.c          # main entry point
src/NVXMath.{c,h}       # expression evaluation
src/NVXVars.{c,h}       # variable storage/types/named blocks
src/NVXScript.{c,h}     # interpreter and helpers
src/NVXShell.{c,h}      # interactive shell
src/NVXJSON.{c,h}       # simple JSON utilities
src/NVXRequests.{c,h}   # HTTP client
src/NVXNet.{c,h}        # minimalist HTTP server
```

Recompile with the networking modules linked:

```sh
gcc -Wall -std=c11 src/*.c -o build/NevoidX.exe -lws2_32 -lm
```

## Examples
### Simple server script
```nvx
# not real language yet; demonstration of concept
var=nvx.http_get("http://api.example.com/data")   # returns body as string
print(var)
```

### JSON helper

The `nvx.json_get` function can extract a value from a JSON string. It accepts
either a literal JSON document or the name of a variable containing JSON. The
value may be a string, number, boolean, null, or even an object/array – the
function will return the complete substring for the value. Examples:

```nvx
json = nvx.http_get("http://httpbin.org/json")
# fetch from variable, not a literal constant
slides = nvx.json_get(json, "slideshow")
print(slides)            # prints nested object
title = nvx.json_get(json, "title")  # top‑level string
print(title)
```

### Calculator script
```nvx
def.var=a,b
print("Enter a:")
a=user.input_var("")
print("Enter b:")
b=user.input_var("")
print("Sum=", math(a+b))
```

## Notes and Limitations

- Variable names must start with a letter or underscore and contain alphanumerics or underscores.
- The evaluator does not support strings inside math expressions (except via variable expansion).
- `goto` and named blocks are simple and do not support parameters.
- Error handling prints messages to stdout and may abort on severe issues.

## Contributing

For contributions, edit `src/NevoidX.c` and recompile. Consider refactoring for
modularity as needed.

---