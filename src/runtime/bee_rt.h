/* bee_rt.h - Bee runtime. Embedded verbatim into every generated C file.
 *
 * Memory model: all of B's memory is one array of machine words (mem[]).
 * A B "pointer" is a word index into that array, so v[1] really is the
 * next word and a[b] == b[a] == *(a+b), just like on the original
 * word-addressed machines. Characters are packed CPW per word, first
 * character in the low byte.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef intptr_t W; /* one B machine word */
#define CPW ((int)sizeof(W)) /* characters packed per word: 8 on arm64, 4 on arm32 */

#ifndef BEE_MEMWORDS
#define BEE_MEMWORDS (1 << 20)
#endif

/* words needed for a string of n chars plus its *e terminator */
#define BEE_WORDS(n) (((n) + 1 + CPW - 1) / CPW)

/* byte i of a character constant, or 0 if it does not fit in a word */
#define BCH(i, c) ((i) < CPW ? ((uintptr_t)(c) << (8 * ((i) < CPW ? (i) : 0))) : (uintptr_t)0)

/* every B function takes up to 8 words, so they all fit one pointer type */
#define BEE_PARAMS W a0, W a1, W a2, W a3, W a4, W a5, W a6, W a7
#define BEE_FN(name) static W F_##name(BEE_PARAMS)
typedef W (*Fn)(BEE_PARAMS);

static W mem[BEE_MEMWORDS];
static W sp; /* stack pointer, a word index into mem[] */
static Fn ftab[BEE_NFUNCS];

static void bee_fatal(const char *msg, W v) {
    fflush(stdout);
    fprintf(stderr, "bee: runtime error: %s (%ld)\n", msg, (long)v);
    exit(1);
}

static W *mp(W a) {
    if ((uintptr_t)a >= (uintptr_t)BEE_MEMWORDS) bee_fatal("memory access out of range", a);
    return &mem[a];
}

static W bdiv(W a, W b) {
    if (b == 0) bee_fatal("division by zero", 0);
    if (b == -1) return (W)(0 - (uintptr_t)a);
    return a / b;
}
static W bmod(W a, W b) {
    if (b == 0) bee_fatal("division by zero", 0);
    if (b == -1) return 0;
    return a % b;
}
static W shl(W a, W b) { return (W)((uintptr_t)a << ((uintptr_t)b & (8 * CPW - 1))); }
static W shr(W a, W b) { return a >> ((uintptr_t)b & (8 * CPW - 1)); }
static W asg_div(W *p, W b) { return *p = bdiv(*p, b); }
static W asg_mod(W *p, W b) { return *p = bmod(*p, b); }
static W asg_shl(W *p, W b) { return *p = shl(*p, b); }
static W asg_shr(W *p, W b) { return *p = shr(*p, b); }

static W callf(W f, BEE_PARAMS) {
    if (f <= 0 || f >= BEE_NFUNCS || !ftab[f]) bee_fatal("call of a non-function", f);
    return ftab[f](a0, a1, a2, a3, a4, a5, a6, a7);
}

/* ---- packed character access ---- */

static int getbyte(W s, W i) {
    if (i < 0) bee_fatal("negative character index", i);
    return (int)(((uintptr_t)*mp(s + i / CPW) >> (8 * (i % CPW))) & 0xff);
}
static void putbyte(W s, W i, int c) {
    if (i < 0) bee_fatal("negative character index", i);
    W *p = mp(s + i / CPW);
    int sh = 8 * (int)(i % CPW);
    *p = (W)(((uintptr_t)*p & ~((uintptr_t)0xff << sh)) | ((uintptr_t)(c & 0xff) << sh));
}
static void bee_str(W addr, const char *s, int n) {
    for (int i = 0; i <= n; i++) putbyte(addr, i, i < n ? (unsigned char)s[i] : 4);
}

/* ---- the B standard library (small subset) ---- */

/* putchar(c): writes the characters packed in word c, low byte first, up to a zero byte */
static void put_packed(W c) {
    for (int i = 0; i < CPW; i++) {
        int ch = (int)(((uintptr_t)c >> (8 * i)) & 0xff);
        if (!ch) break;
        putchar(ch);
    }
}
static void put_str(W s) {
    for (W i = 0;; i++) {
        int c = getbyte(s, i);
        if (c == 4) break; /* *e */
        putchar(c);
    }
}
static void print_base(W n, W base) {
    char buf[72];
    int k = 0;
    uintptr_t u = (uintptr_t)n;
    if (base < 2 || base > 16) base = 10;
    if (n < 0) {
        putchar('-');
        u = 0 - u;
    }
    do {
        buf[k++] = "0123456789abcdef"[u % (uintptr_t)base];
        u /= (uintptr_t)base;
    } while (u);
    while (k) putchar(buf[--k]);
}

BEE_FN(putchar) { put_packed(a0); return a0; }
BEE_FN(getchar) { int c = getchar(); return c == EOF ? 4 : c; } /* *e at end of file */
BEE_FN(char) { return getbyte(a0, a1); }
BEE_FN(lchar) { putbyte(a0, a1, (int)a2); return a2; }
BEE_FN(printn) { print_base(a0, a1); return 0; }
BEE_FN(putnumb) { print_base(a0, 10); return 0; }
BEE_FN(exit) { fflush(stdout); exit((int)a0); return 0; }

/* write n chars of s padded to width w (left-justified if left) */
static void put_padded(const char *s, int n, int w, int left, int zero) {
    int pad = w > n ? w - n : 0;
    if (!left && zero && n > 0 && (s[0] == '-' || s[0] == '+')) { putchar(s[0]); s++; n--; }
    if (!left) for (int i = 0; i < pad; i++) putchar(zero ? '0' : ' ');
    for (int i = 0; i < n; i++) putchar(s[i]);
    if (left) for (int i = 0; i < pad; i++) putchar(' ');
}

/* printf(fmt, ...): %d %o %x %c %s %% with optional '-' / '0' flags and a field width,
 * e.g. %4d %-6s %04x */
BEE_FN(printf) {
    W args[7] = {a1, a2, a3, a4, a5, a6, a7};
    int n = 0;
    for (W i = 0;; i++) {
        int c = getbyte(a0, i);
        if (c == 4) break;
        if (c != '%') { putchar(c); continue; }
        W start = i; /* index of the '%', to echo an unknown spec as written */
        int left = 0, zero = 0, width = 0;
        c = getbyte(a0, ++i);
        for (;; c = getbyte(a0, ++i)) {
            if (c == '-') left = 1;
            else if (c == '0' && width == 0) zero = 1;
            else break;
        }
        while (c >= '0' && c <= '9') {
            if (width < 10000) width = width * 10 + (c - '0');
            c = getbyte(a0, ++i);
        }
        if (c == 4) break;
        char buf[72];
        int len;
        switch (c) {
        case 'd': case 'o': case 'x': {
            W v = n < 7 ? args[n++] : 0;
            if (c == 'd') len = snprintf(buf, sizeof buf, "%ld", (long)v);
            else if (c == 'o') len = snprintf(buf, sizeof buf, "%lo", (unsigned long)v);
            else len = snprintf(buf, sizeof buf, "%lx", (unsigned long)v);
            put_padded(buf, len, width, left, zero);
            break;
        }
        case 'c': {
            W v = n < 7 ? args[n++] : 0;
            char cb[8];
            int k = 0;
            for (int j = 0; j < CPW; j++) {
                int ch = (int)(((uintptr_t)v >> (8 * j)) & 0xff);
                if (!ch) break;
                cb[k++] = (char)ch;
            }
            put_padded(cb, k, width, left, 0);
            break;
        }
        case 's': {
            W v = n < 7 ? args[n++] : 0;
            W l = 0;
            while (getbyte(v, l) != 4) l++;
            int pad = width > l ? (int)(width - l) : 0;
            if (!left) for (int j = 0; j < pad; j++) putchar(' ');
            put_str(v);
            if (left) for (int j = 0; j < pad; j++) putchar(' ');
            break;
        }
        case '%': putchar('%'); break;
        default: /* unknown spec: print it as written, consume no argument */
            for (W j = start; j <= i; j++) putchar(getbyte(a0, j));
            break;
        }
    }
    return 0;
}
