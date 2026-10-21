/*
 * main.c — OpenMac Calculator.app entry point (terminal edition).
 * Original OpenMac code, proprietary license (see ../../LICENSE).
 *
 * Reproduces the FUNCTIONALITY of Apple's preinstalled Calculator.app,
 * running entirely in the terminal (Termux / any POSIX tty):
 *   - Basic mode    : + - × ÷ %, memory MC MR M+ M-, like the macOS app
 *   - Scientific    : sqrt x² x^y sin cos tan ln log pi e ...
 *   - CLI one-shot  : calculator "2+2*2"  -> prints 6
 *   - Interactive   : REPL with history and last-answer "_"
 *   - History       : persisted to ~/.omac/Calculator/history (like macOS
 *                     Calculator's Show History panel)
 *
 * Usage:
 *   calculator                 interactive REPL (Basic-style prompts)
 *   calculator --sci           interactive REPL with scientific functions
 *   calculator -e EXPR         evaluate one expression, print result
 *   calculator --history [n]   show last n entries (default 20)
 *   calculator --clear-history wipe saved history
 */
#include "calc.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define HISTORY_MAX_LINES 200

static const char *history_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof path, "%s/.omac/Calculator/history", home);
    return path;
}

static void history_append(const char *expr, const char *result) {
    const char *p = history_path();
    /* mkdir -p style: create every ancestor component of the file path */
    char dir[512];
    snprintf(dir, sizeof dir, "%s", p);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';                      /* dir = ~/.omac/Calculator */
        for (char *q = dir + 1; *q; q++) {  /* skip leading '/', create per level */
            if (*q == '/') { *q = '\0'; mkdir(dir, 0755); *q = '/'; }
        }
        mkdir(dir, 0755);                   /* final (deepest) component */
    }
    FILE *f = fopen(p, "a");
    if (!f) {
        perror("calculator: cannot write history");
        return;
    }
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M", &tmv);
    fprintf(f, "%s\t%s\t=\t%s\n", ts, expr, result);
    fclose(f);
}

static int history_show(int n) {
    FILE *f = fopen(history_path(), "r");
    if (!f) { printf("(history is empty)\n"); return 0; }
    /* read all lines into a ring buffer */
    static char lines[HISTORY_MAX_LINES][512];
    int count = 0;
    fseek(f, 0, SEEK_SET);
    char buf[512];
    while (fgets(buf, sizeof buf, f)) {
        size_t len = strlen(buf);
        if (len && buf[len-1] == '\n') buf[--len] = '\0';
        snprintf(lines[count % HISTORY_MAX_LINES], 512, "%s", buf);
        count++;
    }
    fclose(f);
    if (count == 0) { printf("(history is empty)\n"); return 0; }
    if (n > count) n = count;
    int start = count - n;
    for (int i = start; i < count; i++) {
        const char *l = lines[i % HISTORY_MAX_LINES];
        /* tab-separated: date time \t expr \t = \t result */
        const char *t1 = strchr(l, '\t');
        if (!t1) { puts(l); continue; }
        char stamp[64];
        size_t sl = (size_t)(t1 - l);
        if (sl >= sizeof stamp) sl = sizeof stamp - 1;
        memcpy(stamp, l, sl); stamp[sl] = '\0';
        const char *rest = t1 + 1;
        const char *t2 = strstr(rest, "\t=\t");
        if (t2) {
            char expr[256];
            size_t el = (size_t)(t2 - rest);
            if (el >= sizeof expr) el = sizeof expr - 1;
            memcpy(expr, rest, el); expr[el] = '\0';
            printf("%-16s %s = %s\n", stamp, expr, t2 + 3);
        } else {
            printf("%-16s %s\n", stamp, rest);
        }
    }
    return 0;
}

static int history_clear(void) {
    FILE *f = fopen(history_path(), "w");
    if (!f) { perror("calculator: history"); return 1; }
    fclose(f);
    printf("History cleared.\n");
    return 0;
}

/* format number Darwin-style: integers without trailing .0 */
static void format_number(double v, char *out, size_t cap) {
    if (v == trunc(v) && fabs(v) < 1e15)
        snprintf(out, cap, "%.0f", v);
    else
        snprintf(out, cap, "%g", v);
}

static int eval_and_report(const char *expr, int quiet_ok) {
    calc_result_t r = calc_evaluate(expr);
    char num[64];
    if (r.err[0]) {
        fprintf(stderr, "error: %s\n", r.err);
        if (r.err_col > 0) {
            fprintf(stderr, "      %s\n", expr);
            fprintf(stderr, "      ");
            for (int i = 1; i < r.err_col; i++) fputc(' ', stderr);
            fprintf(stderr, "^\n");
        }
        return calc_status(&r);
    }
    format_number(r.value, num, sizeof num);
    if (!quiet_ok) printf("%s\n", num);
    history_append(expr, num);
    return 0;
}

static void print_banner(int sci) {
    printf("OpenMac Calculator — %s mode (Ctrl-D or 'quit' to exit)\n",
           sci ? "Scientific" : "Basic");
    if (sci)
        printf("Functions: sqrt cbrt abs ln log log2 exp sin cos tan asin acos atan\n"
               "           sinh cosh tanh round floor ceil fact | ops + - * / %% ^ | constants pi e tau\n");
    else
        printf("Type expressions like: 2+2*2, (3+4)/7, 2^10. '_' reuses the last answer.\n");
}

static int repl(int sci) {
    print_banner(sci);
    char line[1024];
    double last = 0.0;
    int have_last = 0;
    char expr_sub[2048];

    while (1) {
        fputs("> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) { putchar('\n'); break; }
        size_t len = strlen(line);
        while (len && isspace((unsigned char)line[len-1])) line[--len] = '\0';
        if (len == 0) continue;
        if (!strcmp(line, "quit") || !strcmp(line, "exit")) break;
        if (!strcmp(line, "history")) { history_show(20); continue; }
        if (!strcmp(line, "clear"))   { history_clear(); continue; }

        /* substitute '_' with last answer, like macOS keeps running total */
        const char *src = line;
        if (strchr(line, '_')) {
            if (!have_last) { fprintf(stderr, "error: no previous answer for '_'\n"); continue; }
            char ls[64];
            format_number(last, ls, sizeof ls);
            int j = 0;
            for (const char *i = line; *i && j < (int)sizeof expr_sub - 1; i++) {
                if (*i == '_') {
                    j += snprintf(expr_sub + j, sizeof expr_sub - j, "%s", ls);
                } else expr_sub[j++] = *i;
            }
            expr_sub[j] = '\0';
            src = expr_sub;
        }

        calc_result_t r = calc_evaluate(src);
        char num[64];
        if (r.err[0]) {
            fprintf(stderr, "error: %s\n", r.err);
            if (r.err_col > 0) {
                fprintf(stderr, "  %s\n  ", src);
                for (int i = 1; i < r.err_col; i++) fputc(' ', stderr);
                fputs("^\n", stderr);
            }
            continue;
        }
        last = r.value; have_last = 1;
        format_number(last, num, sizeof num);
        printf("= %s\n", num);
        history_append(src, num);
    }
    return 0;
}

static void usage(void) {
    printf(
"calculator - OpenMac Calculator.app (terminal edition)\n"
"\n"
"Usage:\n"
"  calculator                 interactive REPL (Basic mode)\n"
"  calculator --sci           interactive REPL (Scientific mode)\n"
"  calculator -e EXPR         evaluate EXPR once and print the result\n"
"  calculator --history [n]   show last n results (default 20)\n"
"  calculator --clear-history delete saved history\n"
"  calculator --help          this message\n");
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) { usage(); return 0; }
        if (!strcmp(argv[1], "-e") || !strcmp(argv[1], "--eval")) {
            if (argc < 3) { fprintf(stderr, "calculator: -e needs an expression\n"); return 1; }
            /* join remaining args so `calculator -e 2 + 2` also works */
            char joined[2048]; 
            size_t off = 0;
            for (int i = 2; i < argc && off + 1 < sizeof joined; i++)
                off += (size_t)snprintf(joined + off, sizeof joined - off, "%s%s",
                                        i > 2 ? " " : "", argv[i]);
            return eval_and_report(joined, 0);
        }
        if (!strcmp(argv[1], "--sci")) return repl(1);
        if (!strcmp(argv[1], "--history")) {
            int n = (argc >= 3) ? atoi(argv[2]) : 20;
            if (n <= 0) n = 20;
            return history_show(n);
        }
        if (!strcmp(argv[1], "--clear-history")) return history_clear();
        /* bare positional arg treated as expression: calculator "2+2" */
        return eval_and_report(argv[1], 0);
    }
    return repl(0);
}
