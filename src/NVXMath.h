#ifndef NVX_MATH_H
#define NVX_MATH_H

#include <stddef.h>

// evaluate a math expression string, return 1 on success and result in *result
int evaluate_math_expr(const char *expr, double *result);

#endif // NVX_MATH_H
