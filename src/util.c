/* util.c - growable buffers, printf-style string building, error reporting */
#include "bee.h"

#include <stdarg.h>

const char *srcname = "input";

void oom(void) { fputs("bee: out of memory\n", stderr); exit(1); }

void die(int line, const char *f, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", srcname, line);
    va_start(ap, f);
    vfprintf(stderr, f, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void warn(int line, const char *f, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: warning: ", srcname, line);
    va_start(ap, f);
    vfprintf(stderr, f, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void bgrow(Buf *b, size_t extra) {
    if (b->n + extra + 1 > b->cap) {
        b->cap = (b->n + extra + 1) * 2;
        b->p = realloc(b->p, b->cap);
        if (!b->p) oom();
    }
}
void bputn(Buf *b, const char *s, size_t n) {
    bgrow(b, n);
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
void bput(Buf *b, const char *s) { bputn(b, s, strlen(s)); }
const char *bstr(Buf *b) { return b->p ? b->p : ""; }

static char *vfmt(const char *f, va_list ap) {
    va_list c;
    va_copy(c, ap);
    int n = vsnprintf(NULL, 0, f, c);
    va_end(c);
    char *s = malloc((size_t)n + 1);
    if (!s) oom();
    vsnprintf(s, (size_t)n + 1, f, ap);
    return s;
}
char *fmt(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    char *s = vfmt(f, ap);
    va_end(ap);
    return s;
}
void bprintf(Buf *b, const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    char *s = vfmt(f, ap);
    va_end(ap);
    bput(b, s);
    free(s);
}
char *dupn(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) oom();
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}
