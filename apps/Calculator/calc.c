/*
 * calc.c — recursive-descent expression evaluator for Calculator.app.
 * Original OpenMac code, proprietary license (see ../../LICENSE).
 * No Apple source is used; only the user-visible behaviour of macOS
 * Calculator (Basic/Scientific modes) is reproduced functionally.
 */
#include "calc.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_TAU
#define M_TAU (2.0 * M_PI)
#endif

// ---------------- tokenizer ----------------

typedef enum {
    T_END, T_NUM, T_IDENT, T_OP, T_LPAR, T_RPAR, T_COMMA
} tok_kind_t;

typedef struct {
    tok_kind_t  kind;
    double      num;
    char        ident[32];
    char        op;
    int         col;   /* 1-based column where token starts */
} token_t;

typedef struct {
    const char *src;
    size_t      pos;
    token_t     cur;
    calc_result_t out;   /* error state lives here */
    int         failed;
} parser_t;

static void fail(parser_t *p, int col, const char *fmt, ...) {
    if (p->failed) return;
    p->failed = 1;
    p->out.err_col = col;
    va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintf(p->out.err, sizeof p->out.err, fmt, ap);
    __builtin_va_end(ap);
}

static void next_token(parser_t *p) {
    token_t t;
    memset(&t, 0, sizeof t);
    while (isspace((unsigned char)p->src[p->pos])) p->pos++;
    t.col = (int)p->pos + 1;
    char c = p->src[p->pos];

    if (c == '\0') { t.kind = T_END; p->cur = t; return; }

    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)p->src[p->pos+1]))) {
        char *end = NULL;
        errno = 0;
        double v = strtod(p->src + p->pos, &end);
        if (errno == ERANGE) { fail(p, t.col, "number out of range"); p->cur = t; return; }
        t.kind = T_NUM; t.num = v;
        p->pos = (size_t)(end - p->src);
        p->cur = t; return;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        size_t n = 0;
        while (isalnum((unsigned char)p->src[p->pos]) || p->src[p->pos] == '_') {
            if (n < sizeof t.ident - 1) t.ident[n++] = p->src[p->pos];
            p->pos++;
        }
        t.ident[n] = '\0';
        t.kind = T_IDENT;
        p->cur = t; return;
    }
    p->pos++;
    switch (c) {
        case '(': t.kind = T_LPAR; break;
        case ')': t.kind = T_RPAR; break;
        case ',': t.kind = T_COMMA; break;
        case '+': case '-': case '*': case '/': case '%': case '^':
            t.kind = T_OP; t.op = c; break;
        default:
            fail(p, t.col, "unexpected character '%c'", c);
            t.kind = T_END; p->cur = t; return;
    }
    p->cur = t;
}

/* ---------------- constants & functions ---------------- */

typedef struct { const char *name; double value; } const_entry_t;
static const const_entry_t kConstants[] = {
    {"pi",  M_PI}, {"e", M_E}, {"tau", M_TAU}, {NULL, 0}
};

/* unary math funcs mirroring macOS Calculator scientific buttons */
typedef struct { const char *name; double (*fn)(double); int argc; } func_entry_t;

static double fn_cbrt(double x)  { return cbrt(x); }
static double fn_log(double x)   { return log10(x); }   /* macOS "log" = base 10 */
static double fn_ln(double x)    { return log(x); }
static double fn_fact(double x) {
    if (x < 0 || floor(x) != x) { errno = EDOM; return NAN; }
    if (x > 170) { errno = ERANGE; return HUGE_VAL; }
    double r = 1.0;
    for (int i = 2; i <= (int)x; i++) r *= i;
    return r;
}
static const func_entry_t kFuncs[] = {
    {"sqrt", sqrt,   1}, {"cbrt", fn_cbrt, 1}, {"abs", fabs, 1},
    {"ln",   fn_ln,  1}, {"log",  fn_log,  1}, {"log10", log10, 1},
    {"log2", log2,   1}, {"exp",  exp,     1},
    {"sin",  sin,    1}, {"cos",  cos,     1}, {"tan",  tan,  1},
    {"asin", asin,   1}, {"acos", acos,    1}, {"atan", atan, 1},
    {"sinh", sinh,   1}, {"cosh", cosh,    1}, {"tanh", tanh, 1},
    {"round", round, 1}, {"floor", floor,  1}, {"ceil", ceil, 1},
    {"fact", fn_fact,1},
    {NULL, NULL, 0}
};

/* ---------------- grammar ---------------- */

static double parse_expr(parser_t *p);

static double parse_atom(parser_t *p) {
    token_t t = p->cur;
    if (t.kind == T_NUM) { next_token(p); return t.num; }
    if (t.kind == T_LPAR) {
        next_token(p);
        double v = parse_expr(p);
        if (p->failed) return NAN;
        if (p->cur.kind != T_RPAR) { fail(p, p->cur.col, "expected ')'"); return NAN; }
        next_token(p);
        return v;
    }
    if (t.kind == T_IDENT) {
        /* constant? */
        for (const const_entry_t *c = kConstants; c->name; c++)
            if (!strcasecmp(t.ident, c->name)) { next_token(p); return c->value; }
        /* function call? */
        for (const func_entry_t *f = kFuncs; f->name; f++) {
            if (!strcasecmp(t.ident, f->name)) {
                next_token(p);
                if (p->cur.kind != T_LPAR) {
                    fail(p, p->cur.col, "expected '(' after %s()", f->name);
                    return NAN;
                }
                next_token(p);
                double arg = parse_expr(p);
                if (p->failed) return NAN;
                if (p->cur.kind != T_RPAR) {
                    fail(p, p->cur.col, "expected ')' to close %s(", f->name);
                    return NAN;
                }
                next_token(p);
                errno = 0;
                double r = f->fn(arg);
                if (errno == EDOM) { fail(p, t.col, "%s(): domain error (bad argument)", f->name); return NAN; }
                if (errno == ERANGE) { fail(p, t.col, "%s(): result out of range", f->name); return NAN; }
                return r;
            }
        }
        fail(p, t.col, "unknown identifier '%s'", t.ident);
        return NAN;
    }
    fail(p, t.col, t.kind == T_END ? "unexpected end of expression"
                                   : "expected a number or '('");
    return NAN;
}

static double parse_unary(parser_t *p) {
    if (p->cur.kind == T_OP && (p->cur.op == '-' || p->cur.op == '+')) {
        char op = p->cur.op;
        next_token(p);
        double v = parse_unary(p);
        if (p->failed) return NAN;
        return op == '-' ? -v : v;
    }
    return parse_atom(p);
}

static double parse_factor(parser_t *p) {
    double base = parse_unary(p);
    if (p->failed) return NAN;
    if (p->cur.kind == T_OP && p->cur.op == '^') {
        int opcol = p->cur.col;
        next_token(p);
        double expo = parse_factor(p);   /* right-associative */
        if (p->failed) return NAN;
        errno = 0;
        double r = pow(base, expo);
        if (errno == EDOM && !(base < 0 && floor(expo) != expo))
            { fail(p, opcol, "pow(): domain error"); return NAN; }
        if (errno == ERANGE) { fail(p, opcol, "pow(): result out of range"); return NAN; }
        return r;
    }
    return base;
}

static double parse_term(parser_t *p) {
    double v = parse_factor(p);
    while (!p->failed && p->cur.kind == T_OP &&
           (p->cur.op == '*' || p->cur.op == '/' || p->cur.op == '%')) {
        char op = p->cur.op; int opcol = p->cur.col;
        next_token(p);
        double rhs = parse_factor(p);
        if (p->failed) return NAN;
        if (op == '*') v *= rhs;
        else if (op == '/') {
            if (rhs == 0.0) { fail(p, opcol, "division by zero"); return NAN; }
            v /= rhs;
        } else {
            if (rhs == 0.0) { fail(p, opcol, "modulo by zero"); return NAN; }
            v = fmod(v, rhs);
        }
    }
    return v;
}

static double parse_expr(parser_t *p) {
    double v = parse_term(p);
    while (!p->failed && p->cur.kind == T_OP &&
           (p->cur.op == '+' || p->cur.op == '-')) {
        char op = p->cur.op;
        next_token(p);
        double rhs = parse_term(p);
        if (p->failed) return NAN;
        v = (op == '+') ? v + rhs : v - rhs;
    }
    return v;
}

/* ---------------- public API ---------------- */

calc_result_t calc_evaluate(const char *expr) {
    calc_result_t res;
    memset(&res, 0, sizeof res);
    if (!expr) { snprintf(res.err, sizeof res.err, "empty input"); res.err_col = 0; return res; }

    parser_t p;
    memset(&p, 0, sizeof p);
    p.src = expr;
    p.out = res;
    next_token(&p);
    double v = parse_expr(&p);
    if (!p.failed && p.cur.kind != T_END)
        fail(&p, p.cur.col, "unexpected trailing input");
    if (p.failed) {
        res = p.out;
        res.value = NAN;
        return res;
    }
    res.value = v;
    return res;
}

int calc_status(const calc_result_t *r) {
    if (r->err[0] == '\0') return 0;
    if (strstr(r->err, "domain") || strstr(r->err, "range") ||
        strstr(r->err, "zero")) return 2;
    return 1;
}
