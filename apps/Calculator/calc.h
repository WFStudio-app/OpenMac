/*
 * calc.h — expression evaluator engine for OpenMac Calculator.app.
 * Original OpenMac code, proprietary license (see ../../LICENSE).
 * Grammar (calc-style, left-associative):
 *   expr    := term (('+' | '-') term)*
 *   term    := factor (('*' | '/' | '%') factor)*
 *   factor  := unary ('^' factor)?          (right-associative power)
 *   unary   := ('-' | '+')? atom
 *   atom    := number | '(' expr ')' | func '(' expr ')'
 *   func    := sqrt | cbrt | abs | ln | log | log10 | log2 | exp
 *              sin | cos | tan | asin | acos | atan | sinh | cosh | tanh
 *              round | floor | ceil | fact
 * Constants: pi, e, tau. Errors carry a 1-based column for the TUI caret.
 */
#ifndef _OMAC_CALC_H
#define _OMAC_CALC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double value;
    char   err[192];   /* empty string == success */
    int    err_col;    /* 1-based column of the error, 0 if unknown */
} calc_result_t;

/* Evaluate a full expression string. Never aborts on bad input;
 * fills result.err instead. Leading/trailing spaces are tolerated. */
calc_result_t calc_evaluate(const char *expr);

/* 0 on success, 1 parse error, 2 math domain error (for CLI exit codes). */
int calc_status(const calc_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* _OMAC_CALC_H */
