/*
 * bee.c - Bee, a B-to-C transpiler.
 *
 *   prog.b --> [lexer] --> [parser + C emitter, two passes] --> prog.c --> cc
 *
 * Pass 1 only learns the names of top-level functions and variables so that
 * forward references work. Pass 2 emits the C. The runtime (bee_rt.h) is
 * embedded into the generated file, so the output builds with any C compiler.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char rt_src[] =
#include "rt_inc.h"
;

/* ------------------------------------------------------------------ */
/* utilities                                                           */
/* ------------------------------------------------------------------ */

typedef struct { char *p; size_t n, cap; } Buf;

static const char *srcname = "input";

static void oom(void) { fputs("bee: out of memory\n", stderr); exit(1); }

static void die(int line, const char *f, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", srcname, line);
    va_start(ap, f);
    vfprintf(stderr, f, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void bgrow(Buf *b, size_t extra) {
    if (b->n + extra + 1 > b->cap) {
        b->cap = (b->n + extra + 1) * 2;
        b->p = realloc(b->p, b->cap);
        if (!b->p) oom();
    }
}
static void bputn(Buf *b, const char *s, size_t n) {
    bgrow(b, n);
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void bput(Buf *b, const char *s) { bputn(b, s, strlen(s)); }
static const char *bstr(Buf *b) { return b->p ? b->p : ""; }

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
static char *fmt(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    char *s = vfmt(f, ap);
    va_end(ap);
    return s;
}
static void bprintf(Buf *b, const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    char *s = vfmt(f, ap);
    va_end(ap);
    bput(b, s);
    free(s);
}
static char *dupn(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) oom();
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

/* ------------------------------------------------------------------ */
/* lexer                                                               */
/* ------------------------------------------------------------------ */

enum { T_EOF, T_NUM, T_CHR, T_STR, T_ID, T_OP };

typedef struct {
    int k;
    char *s; /* identifier, operator, or the decoded bytes of a char/string literal */
    int len; /* byte length of a char/string literal */
    unsigned long long v;
    int line;
} Tok;

static Tok *toks;
static int ntoks, captoks;
static int modern; /* -m: do not read "=+" style assignment operators */

static void addtok(Tok t) {
    if (ntoks == captoks) {
        captoks = captoks ? captoks * 2 : 256;
        toks = realloc(toks, (size_t)captoks * sizeof *toks);
        if (!toks) oom();
    }
    toks[ntoks++] = t;
}

/* B escapes start with '*', not '\' */
static int escape(const char *s, int *i, int line) {
    int c = (unsigned char)s[*i];
    if (!c) die(line, "unterminated literal");
    (*i)++;
    switch (c) {
    case 'n': return 10;
    case 't': return 9;
    case 'r': return 13;
    case '0': return 0;
    case 'e': return 4; /* EOT, the string terminator */
    case '(': return '{';
    case ')': return '}';
    default: return c; /* ** *' *" */
    }
}

static void lex(const char *s) {
    int line = 1;
    int i = 0;
    for (;;) {
        int c = (unsigned char)s[i];
        if (!c) break;
        if (c == '\n') { line++; i++; continue; }
        if (isspace(c)) { i++; continue; }
        if (c == '/' && s[i + 1] == '*') {
            i += 2;
            while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) {
                if (s[i] == '\n') line++;
                i++;
            }
            if (!s[i]) die(line, "unterminated comment");
            i += 2;
            continue;
        }
        Tok t;
        memset(&t, 0, sizeof t);
        t.line = line;
        if (isalpha(c) || c == '_') {
            int j = i;
            while (isalnum((unsigned char)s[j]) || s[j] == '_') j++;
            t.k = T_ID;
            t.s = dupn(s + i, (size_t)(j - i));
            i = j;
        } else if (isdigit(c)) {
            int j = i;
            while (isalnum((unsigned char)s[j])) j++;
            char *txt = dupn(s + i, (size_t)(j - i)), *end;
            t.k = T_NUM;
            t.v = strtoull(txt, &end, 0); /* 0755 octal, 0x1f hex, else decimal */
            if (*end) die(line, "bad number '%s'", txt);
            i = j;
        } else if (c == '\'' || c == '"') {
            int q = c;
            Buf b = {0};
            i++;
            for (;;) {
                int d = (unsigned char)s[i];
                if (!d || d == '\n') die(line, "unterminated %s", q == '"' ? "string" : "character constant");
                i++;
                if (d == q) break;
                if (d == '*') d = escape(s, &i, line);
                char ch = (char)d;
                bputn(&b, &ch, 1);
            }
            t.k = q == '"' ? T_STR : T_CHR;
            t.s = b.p;
            t.len = (int)b.n;
        } else {
            char op[4] = {0};
            if (c == '=' && !modern && s[i + 1] != '=') {
                /* original B spells compound assignment "=+", "=-", "=<<" ... */
                if (s[i + 1] == '<' && s[i + 2] == '<') { strcpy(op, "<<="); i += 3; }
                else if (s[i + 1] == '>' && s[i + 2] == '>') { strcpy(op, ">>="); i += 3; }
                else if (s[i + 1] && strchr("+-*/%&|", s[i + 1])) { op[0] = s[i + 1]; op[1] = '='; i += 2; }
            }
            if (!op[0]) {
                static const char *m3[] = {"<<=", ">>=", 0};
                static const char *m2[] = {"==", "!=", "<=", ">=", "<<", ">>", "++", "--", "&&", "||",
                                           "+=", "-=", "*=", "/=", "%=", "&=", "|=", 0};
                int found = 0;
                for (int k = 0; m3[k] && !found; k++)
                    if (!strncmp(s + i, m3[k], 3)) { strcpy(op, m3[k]); i += 3; found = 1; }
                for (int k = 0; m2[k] && !found; k++)
                    if (!strncmp(s + i, m2[k], 2)) { strcpy(op, m2[k]); i += 2; found = 1; }
                if (!found) {
                    if (!strchr("(){}[];,:?+-*/%&|<>!=", c)) die(line, "unexpected character '%c'", c);
                    op[0] = (char)c;
                    i++;
                }
            }
            t.k = T_OP;
            t.s = dupn(op, strlen(op));
        }
        addtok(t);
    }
    Tok e;
    memset(&e, 0, sizeof e);
    e.k = T_EOF;
    e.line = line;
    addtok(e);
    addtok(e); /* second EOF so one-token lookahead is always in range */
}

/* ------------------------------------------------------------------ */
/* symbols                                                             */
/* ------------------------------------------------------------------ */

enum { K_VAR, K_FN };

typedef struct {
    char *name;
    int kind;
    int fidx; /* function-table index, for K_FN */
    int builtin;
} Sym;

static Sym *gsyms;
static int ngs, capgs;
static int nfuncs = 1; /* ftab[0] stays empty so 0 is never a valid function */

static Sym *findg(const char *n) {
    for (int i = 0; i < ngs; i++)
        if (!strcmp(gsyms[i].name, n)) return &gsyms[i];
    return NULL;
}
static Sym *addg(const char *n, int kind) {
    if (ngs == capgs) {
        capgs = capgs ? capgs * 2 : 64;
        gsyms = realloc(gsyms, (size_t)capgs * sizeof *gsyms);
        if (!gsyms) oom();
    }
    Sym *s = &gsyms[ngs++];
    memset(s, 0, sizeof *s);
    s->name = dupn(n, strlen(n));
    s->kind = kind;
    return s;
}

/* must match the BEE_FN(...) definitions in bee_rt.h */
static const char *builtins[] = {"putchar", "getchar", "char", "lchar", "printf", "printn", "putnumb", "exit", 0};

typedef struct { char *name; int off; } Loc;
static Loc locs[512];
static int nloc;
static int nslots; /* words in the current function's frame */

static int findloc(const char *n) {
    for (int i = nloc - 1; i >= 0; i--)
        if (!strcmp(locs[i].name, n)) return i;
    return -1;
}
static void addloc(const char *n, int off, int line) {
    if (nloc == (int)(sizeof locs / sizeof *locs)) die(line, "too many local names");
    locs[nloc].name = dupn(n, strlen(n));
    locs[nloc].off = off;
    nloc++;
}

/* ------------------------------------------------------------------ */
/* parser state and output buffers                                     */
/* ------------------------------------------------------------------ */

static int pass; /* 1 = collect names, 2 = emit */
static int pos;
static Buf defs;   /* #define G_x / S_n addresses */
static Buf protos; /* function prototypes */
static Buf funcs;  /* function bodies */
static Buf init;   /* global/string initialisation, run before main */
static Buf vinit;  /* local vector setup for the function being parsed */
static char *gend; /* C expression for the next free global word */
static int nstr;

static int isop(const char *o) { return toks[pos].k == T_OP && !strcmp(toks[pos].s, o); }
static int isid(const char *k) { return toks[pos].k == T_ID && !strcmp(toks[pos].s, k); }
static int accept(const char *o) {
    if (isop(o)) { pos++; return 1; }
    return 0;
}
static void expect(const char *o) {
    if (!isop(o)) die(toks[pos].line, "expected '%s'", o);
    pos++;
}

static int is_kw(const char *s) {
    static const char *kw[] = {"auto", "extrn", "if", "else", "while", "switch", "case", "default",
                               "break", "continue", "goto", "return", 0};
    for (int i = 0; kw[i]; i++)
        if (!strcmp(s, kw[i])) return 1;
    return 0;
}

/* value of a character constant, first char in the low byte */
static unsigned long long chr_val(Tok *t) {
    unsigned long long v = 0;
    if (t->len > 8) die(t->line, "character constant too long");
    for (int i = 0; i < t->len; i++) v |= (unsigned long long)(unsigned char)t->s[i] << (8 * i);
    return v;
}

static long long const_val(void) {
    int neg = accept("-");
    Tok *t = &toks[pos];
    long long v;
    if (t->k == T_NUM) v = (long long)t->v;
    else if (t->k == T_CHR) v = (long long)chr_val(t);
    else die(t->line, "constant expected");
    pos++;
    return neg ? -v : v;
}

/* C expression for a character constant; packed per the target's word size */
static char *chr_expr(Tok *t) {
    Buf b = {0};
    bput(&b, "((W)(");
    if (t->len == 0) bput(&b, "0");
    for (int i = 0; i < t->len; i++) bprintf(&b, "%sBCH(%d,%d)", i ? "|" : "", i, (unsigned char)t->s[i]);
    bput(&b, "))");
    return b.p;
}

/* C expression for a string literal: allocates its words and queues its initialisation */
static char *str_expr(Tok *t) {
    if (pass == 1) return fmt("0");
    int k = nstr++;
    bprintf(&defs, "#define S_%d %s\n", k, gend);
    gend = fmt("(S_%d + BEE_WORDS(%d))", k, t->len);
    bprintf(&init, "  bee_str(S_%d, \"", k);
    for (int i = 0; i < t->len; i++) bprintf(&init, "\\%03o", (unsigned char)t->s[i]);
    bprintf(&init, "\", %d);\n", t->len);
    return fmt("((W)S_%d)", k);
}

/* ------------------------------------------------------------------ */
/* expressions                                                         */
/* ------------------------------------------------------------------ */

/* rv:   C expression for the value (for an lvalue, the C lvalue itself)
 * addr: C expression for the word address, non-NULL only for lvalues
 * fn:   name, if this is a bare function name (so a call can be direct) */
typedef struct { char *rv, *addr, *fn; } Ex;

static Ex mk(char *rv) { Ex e = {rv, NULL, NULL}; return e; }
static Ex lval(char *rv, char *addr) { Ex e = {rv, addr, NULL}; return e; }

static Ex expr(void);

static Ex primary(void) {
    Tok *t = &toks[pos];
    switch (t->k) {
    case T_NUM:
        pos++;
        return mk(fmt("((W)%lluULL)", t->v));
    case T_CHR:
        pos++;
        return mk(chr_expr(t));
    case T_STR:
        pos++;
        return mk(str_expr(t));
    case T_ID: {
        if (is_kw(t->s)) die(t->line, "unexpected '%s'", t->s);
        pos++;
        int li = findloc(t->s);
        if (li >= 0) return lval(fmt("mem[fp+%d]", locs[li].off), fmt("(fp+%d)", locs[li].off));
        Sym *s = findg(t->s);
        if (s && s->kind == K_VAR) return lval(fmt("mem[G_%s]", t->s), fmt("((W)G_%s)", t->s));
        if (s && s->kind == K_FN) {
            Ex e = mk(fmt("((W)%d)", s->fidx));
            e.fn = t->s;
            return e;
        }
        if (pass == 2) die(t->line, "undefined name '%s'", t->s);
        return lval(fmt("mem[0]"), fmt("0")); /* pass 1: may be defined later, output is discarded */
    }
    case T_OP:
        if (isop("(")) {
            pos++;
            Ex e = expr();
            expect(")");
            return e;
        }
        /* fall through */
    default:
        die(t->line, "expected an expression");
    }
    return mk(fmt("0"));
}

static Ex postfix(void) {
    Ex e = primary();
    for (;;) {
        if (isop("(")) {
            int line = toks[pos].line;
            pos++;
            char *args[8];
            int n = 0;
            if (!isop(")")) {
                for (;;) {
                    Ex a = expr();
                    if (n == 8) die(line, "too many arguments (max 8)");
                    args[n++] = a.rv;
                    if (!accept(",")) break;
                }
            }
            expect(")");
            Buf b = {0};
            if (e.fn) bprintf(&b, "F_%s(", e.fn);
            else bprintf(&b, "callf(%s, ", e.rv);
            for (int i = 0; i < 8; i++) bprintf(&b, "%s%s", i ? ", " : "", i < n ? args[i] : "0");
            bput(&b, ")");
            e = mk(b.p);
        } else if (isop("[")) {
            pos++;
            Ex i = expr();
            expect("]");
            char *addr = fmt("(%s+%s)", e.rv, i.rv);
            e = lval(fmt("(*mp(%s))", addr), addr);
        } else if (isop("++") || isop("--")) {
            if (!e.addr) die(toks[pos].line, "lvalue required for '%s'", toks[pos].s);
            e = mk(fmt("(%s%s)", e.rv, toks[pos].s));
            pos++;
        } else {
            break;
        }
    }
    return e;
}

static Ex unary(void) {
    int line = toks[pos].line;
    if (isop("-")) { pos++; Ex e = unary(); return mk(fmt("(-%s)", e.rv)); }
    if (isop("!")) { pos++; Ex e = unary(); return mk(fmt("((W)(!%s))", e.rv)); }
    if (isop("*")) {
        pos++;
        Ex e = unary();
        return lval(fmt("(*mp(%s))", e.rv), e.rv);
    }
    if (isop("&")) {
        pos++;
        Ex e = unary();
        if (e.fn) return mk(e.rv);
        if (!e.addr) die(line, "lvalue required for '&'");
        return mk(fmt("((W)%s)", e.addr));
    }
    if (isop("++") || isop("--")) {
        char *op = toks[pos].s;
        pos++;
        Ex e = unary();
        if (!e.addr) die(line, "lvalue required for '%s'", op);
        return mk(fmt("(%s%s)", op, e.rv));
    }
    return postfix();
}

static int binprec(const char *op) {
    static const struct { const char *op; int p; } T[] = {
        {"||", 0}, {"&&", 1}, {"|", 2}, {"&", 3}, {"==", 4}, {"!=", 4}, {"<", 5}, {"<=", 5}, {">", 5},
        {">=", 5}, {"<<", 6}, {">>", 6}, {"+", 7}, {"-", 7}, {"*", 8}, {"/", 8}, {"%", 8}, {0, 0}};
    for (int i = 0; T[i].op; i++)
        if (!strcmp(T[i].op, op)) return T[i].p;
    return -1;
}

static char *binop(const char *op, const char *a, const char *b) {
    if (!strcmp(op, "/")) return fmt("bdiv(%s, %s)", a, b);
    if (!strcmp(op, "%")) return fmt("bmod(%s, %s)", a, b);
    if (!strcmp(op, "<<")) return fmt("shl(%s, %s)", a, b);
    if (!strcmp(op, ">>")) return fmt("shr(%s, %s)", a, b);
    if (strchr("=!<>", op[0]) || !strcmp(op, "&&") || !strcmp(op, "||")) return fmt("((W)(%s %s %s))", a, op, b);
    return fmt("(%s %s %s)", a, op, b); /* + - * & | */
}

static Ex binary(int minp) {
    Ex l = unary();
    for (;;) {
        if (toks[pos].k != T_OP) break;
        int p = binprec(toks[pos].s);
        if (p < minp) break;
        char *op = toks[pos].s;
        pos++;
        Ex r = binary(p + 1);
        l = mk(binop(op, l.rv, r.rv));
    }
    return l;
}

static Ex cond(void) {
    Ex c = binary(0);
    if (accept("?")) {
        Ex a = expr();
        expect(":");
        Ex b = cond();
        return mk(fmt("((%s) ? (%s) : (%s))", c.rv, a.rv, b.rv));
    }
    return c;
}

static Ex expr(void) {
    Ex l = cond();
    if (toks[pos].k == T_OP) {
        const char *op = toks[pos].s;
        static const char *aops[] = {"+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", 0};
        int compound = 0;
        for (int i = 0; aops[i]; i++)
            if (!strcmp(op, aops[i])) compound = 1;
        if (!strcmp(op, "=") || compound) {
            int line = toks[pos].line;
            if (!l.addr) die(line, "lvalue required on the left of '%s'", op);
            pos++;
            Ex r = expr();
            if (!compound) return mk(fmt("(%s = %s)", l.rv, r.rv));
            if (!strcmp(op, "/=")) return mk(fmt("asg_div(&%s, %s)", l.rv, r.rv));
            if (!strcmp(op, "%=")) return mk(fmt("asg_mod(&%s, %s)", l.rv, r.rv));
            if (!strcmp(op, "<<=")) return mk(fmt("asg_shl(&%s, %s)", l.rv, r.rv));
            if (!strcmp(op, ">>=")) return mk(fmt("asg_shr(&%s, %s)", l.rv, r.rv));
            return mk(fmt("(%s %s %s)", l.rv, op, r.rv));
        }
    }
    return l;
}

/* ------------------------------------------------------------------ */
/* statements                                                          */
/* ------------------------------------------------------------------ */

static void stmt(Buf *o) {
    Tok *t = &toks[pos];
    int line = t->line;

    if (isop("{")) {
        pos++;
        int save = nloc;
        bput(o, "{\n");
        while (!isop("}")) {
            if (toks[pos].k == T_EOF) die(toks[pos].line, "missing '}'");
            stmt(o);
        }
        pos++;
        nloc = save;
        bput(o, "}\n");
        return;
    }
    if (isop(";")) { pos++; bput(o, ";\n"); return; }

    if (t->k == T_ID) {
        const char *s = t->s;
        if (!strcmp(s, "auto")) {
            pos++;
            for (;;) {
                Tok *n = &toks[pos];
                if (n->k != T_ID || is_kw(n->s)) die(n->line, "expected a name after 'auto'");
                pos++;
                if (accept("[")) {
                    long long sz = const_val();
                    expect("]");
                    if (sz < 0 || sz > (1 << 20)) die(n->line, "bad vector size");
                    int slot = nslots++;
                    int base = nslots;
                    nslots += (int)sz + 1;
                    addloc(n->s, slot, n->line);
                    bprintf(&vinit, "  mem[fp+%d] = fp+%d;\n", slot, base);
                } else {
                    addloc(n->s, nslots++, n->line);
                }
                if (!accept(",")) break;
            }
            expect(";");
            return;
        }
        if (!strcmp(s, "extrn")) {
            pos++;
            for (;;) {
                if (toks[pos].k != T_ID) die(toks[pos].line, "expected a name after 'extrn'");
                pos++;
                if (!accept(",")) break;
            }
            expect(";");
            return;
        }
        if (!strcmp(s, "if")) {
            pos++;
            expect("(");
            Ex c = expr();
            expect(")");
            bprintf(o, "if (%s) {\n", c.rv);
            stmt(o);
            bput(o, "}\n");
            if (isid("else")) {
                pos++;
                bput(o, "else {\n");
                stmt(o);
                bput(o, "}\n");
            }
            return;
        }
        if (!strcmp(s, "while")) {
            pos++;
            expect("(");
            Ex c = expr();
            expect(")");
            bprintf(o, "while (%s) {\n", c.rv);
            stmt(o);
            bput(o, "}\n");
            return;
        }
        if (!strcmp(s, "switch")) {
            pos++;
            expect("(");
            Ex c = expr();
            expect(")");
            bprintf(o, "switch (%s) {\n", c.rv);
            stmt(o);
            bput(o, "}\n");
            return;
        }
        if (!strcmp(s, "case")) {
            pos++;
            long long v = const_val();
            expect(":");
            bprintf(o, "case %lld:;\n", v);
            return;
        }
        if (!strcmp(s, "default")) {
            pos++;
            expect(":");
            bput(o, "default:;\n");
            return;
        }
        if (!strcmp(s, "break") || !strcmp(s, "continue")) {
            pos++;
            expect(";");
            bprintf(o, "%s;\n", s);
            return;
        }
        if (!strcmp(s, "goto")) {
            pos++;
            if (toks[pos].k != T_ID) die(toks[pos].line, "expected a label after 'goto'");
            bprintf(o, "goto L_%s;\n", toks[pos].s);
            pos++;
            expect(";");
            return;
        }
        if (!strcmp(s, "return")) {
            pos++;
            char *v = fmt("0");
            if (!isop(";")) v = expr().rv;
            expect(";");
            bprintf(o, "{ W r_ = %s; sp = fp; return r_; }\n", v);
            return;
        }
        if (!is_kw(s) && toks[pos + 1].k == T_OP && !strcmp(toks[pos + 1].s, ":")) {
            bprintf(o, "L_%s:;\n", s);
            pos += 2;
            return;
        }
    }

    Ex e = expr();
    (void)line;
    expect(";");
    bprintf(o, "(void)(%s);\n", e.rv);
}

/* ------------------------------------------------------------------ */
/* top level                                                           */
/* ------------------------------------------------------------------ */

static char *ival(void) {
    Tok *t = &toks[pos];
    if (t->k == T_NUM) { pos++; return fmt("((W)%lluULL)", t->v); }
    if (t->k == T_CHR) { pos++; return chr_expr(t); }
    if (t->k == T_STR) { pos++; return str_expr(t); }
    if (isop("-") && toks[pos + 1].k == T_NUM) {
        pos += 2;
        return fmt("(-(W)%lluULL)", toks[pos - 1].v);
    }
    if (t->k == T_ID && !is_kw(t->s)) {
        pos++;
        Sym *s = findg(t->s);
        if (!s) {
            if (pass == 2) die(t->line, "undefined name '%s'", t->s);
            return fmt("0");
        }
        if (s->kind == K_FN) return fmt("((W)%d)", s->fidx);
        return fmt("((W)G_%s)", t->s);
    }
    die(t->line, "bad initialiser");
    return NULL;
}

static void toplevel(void) {
    Tok *t = &toks[pos];
    if (t->k != T_ID || is_kw(t->s)) die(t->line, "expected a name at top level");
    char *name = t->s;
    int line = t->line;
    pos++;
    Sym *s = findg(name);

    if (isop("(")) {
        /* function definition: name(params) statement */
        if (pass == 1) {
            if (s) die(line, "'%s' is already defined", name);
            s = addg(name, K_FN);
            s->fidx = nfuncs++;
        }
        pos++;
        nloc = 0;
        nslots = 0;
        vinit.n = 0;
        if (vinit.p) vinit.p[0] = 0;
        int np = 0;
        if (!isop(")")) {
            for (;;) {
                Tok *p = &toks[pos];
                if (p->k != T_ID || is_kw(p->s)) die(p->line, "expected a parameter name");
                if (np == 8) die(p->line, "too many parameters (max 8)");
                pos++;
                addloc(p->s, nslots++, p->line);
                np++;
                if (!accept(",")) break;
            }
        }
        expect(")");
        Buf body = {0};
        stmt(&body);
        if (pass == 2) {
            bprintf(&protos, "static W F_%s(BEE_PARAMS);\n", name);
            bprintf(&funcs,
                    "static W F_%s(BEE_PARAMS) {\n"
                    "  W fp = sp;\n"
                    "  sp += %d;\n"
                    "  if (sp > BEE_MEMWORDS) bee_fatal(\"stack overflow\", 0);\n",
                    name, nslots);
            for (int i = 0; i < np; i++) bprintf(&funcs, "  mem[fp+%d] = a%d;\n", i, i);
            bput(&funcs, bstr(&vinit));
            bput(&funcs, bstr(&body));
            bput(&funcs, "  sp = fp;\n  return 0;\n}\n\n");
        }
        return;
    }

    /* global variable or vector: name [ [size] ] [ival {, ival}] ; */
    if (pass == 1) {
        if (s) die(line, "'%s' is already defined", name);
        addg(name, K_VAR);
    }
    long long vsize = -1; /* -1 means scalar */
    if (accept("[")) {
        vsize = isop("]") ? 0 : const_val();
        expect("]");
        if (vsize < 0 || vsize > (1 << 24)) die(line, "bad vector size");
    }
    char **iv = NULL;
    int nin = 0, capin = 0;
    if (!isop(";")) {
        for (;;) {
            if (nin == capin) {
                capin = capin ? capin * 2 : 8;
                iv = realloc(iv, (size_t)capin * sizeof *iv);
                if (!iv) oom();
            }
            iv[nin++] = ival();
            if (!accept(",")) break;
        }
    }
    expect(";");
    if (pass == 2) {
        int vec = vsize >= 0;
        long words;
        if (!vec) {
            words = nin > 1 ? nin : 1;
        } else {
            long cnt = (long)vsize + 1;
            if (nin > cnt) cnt = nin;
            words = 1 + cnt;
        }
        bprintf(&defs, "#define G_%s %s\n", name, gend);
        gend = fmt("(G_%s + %ld)", name, words);
        if (vec) bprintf(&init, "  mem[G_%s] = G_%s + 1;\n", name, name);
        for (int i = 0; i < nin; i++) bprintf(&init, "  mem[G_%s + %d] = %s;\n", name, vec ? i + 1 : i, iv[i]);
    }
    free(iv);
}

/* ------------------------------------------------------------------ */
/* driver                                                              */
/* ------------------------------------------------------------------ */

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "bee: cannot open %s\n", path); exit(1); }
    Buf b = {0};
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) bputn(&b, tmp, n);
    fclose(f);
    return b.p ? b.p : dupn("", 0);
}

static void usage(FILE *f) {
    fputs("usage: bee [options] file.b\n"
          "  -o FILE   output file (default: file.b without .b, or a.out)\n"
          "  -C        only write the generated C (to -o, or file.c)\n"
          "  -k        keep the generated C file next to the output\n"
          "  -m        modern assignment only: '=-' means '= -', and x -= 1 style is used\n"
          "  -h        this help\n"
          "env: CC picks the C compiler (default: cc)\n",
          f);
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL;
    int emit_only = 0, keep = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-m")) modern = 1;
        else if (!strcmp(argv[i], "-C")) emit_only = 1;
        else if (!strcmp(argv[i], "-k")) keep = 1;
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(stdout); return 0; }
        else if (argv[i][0] == '-' && argv[i][1]) { fprintf(stderr, "bee: unknown option %s\n", argv[i]); usage(stderr); return 2; }
        else if (!in) in = argv[i];
        else { fprintf(stderr, "bee: only one input file is supported\n"); return 2; }
    }
    if (!in) { usage(stderr); return 2; }

    srcname = in;
    lex(slurp(in));

    for (int i = 0; builtins[i]; i++) {
        Sym *s = addg(builtins[i], K_FN);
        s->fidx = nfuncs++;
        s->builtin = 1;
    }

    for (pass = 1; pass <= 2; pass++) {
        memset(&defs, 0, sizeof defs);
        memset(&protos, 0, sizeof protos);
        memset(&funcs, 0, sizeof funcs);
        memset(&init, 0, sizeof init);
        gend = "1"; /* word 0 is reserved so 0 is never a valid address */
        nstr = 0;
        pos = 0;
        while (toks[pos].k != T_EOF) toplevel();
    }

    Sym *m = findg("main");
    if (!m || m->kind != K_FN || m->builtin) {
        fprintf(stderr, "%s: error: no 'main' function defined\n", in);
        return 1;
    }

    Buf c = {0};
    bprintf(&c, "/* generated by bee from %s */\n#define BEE_NFUNCS %d\n", in, nfuncs);
    bput(&c, bstr(&defs));
    bprintf(&c, "#define GEND %s\n", gend);
    bput(&c, rt_src);
    bput(&c, bstr(&protos));
    bput(&c, "\n");
    bput(&c, bstr(&funcs));
    bput(&c, "static void bee_init(void) {\n");
    for (int i = 0; i < ngs; i++)
        if (gsyms[i].kind == K_FN) bprintf(&c, "  ftab[%d] = F_%s;\n", gsyms[i].fidx, gsyms[i].name);
    bput(&c, bstr(&init));
    bput(&c, "}\n\n"
             "int main(void) {\n"
             "  bee_init();\n"
             "  sp = GEND;\n"
             "  F_main(0, 0, 0, 0, 0, 0, 0, 0);\n"
             "  fflush(stdout);\n"
             "  return 0;\n"
             "}\n");

    /* default names */
    size_t il = strlen(in);
    int has_b = il > 2 && !strcmp(in + il - 2, ".b");
    char *base = has_b ? dupn(in, il - 2) : NULL;

    if (emit_only) {
        const char *path = out ? out : (base ? fmt("%s.c", base) : "a.c");
        FILE *f = fopen(path, "wb");
        if (!f || fwrite(c.p, 1, c.n, f) != c.n) { fprintf(stderr, "bee: cannot write %s\n", path); return 1; }
        fclose(f);
        fprintf(stderr, "bee: wrote %s\n", path);
        return 0;
    }

    const char *exe = out ? out : (base ? base : "a.out");
    char *cpath = fmt("%s.bee.c", exe);
    FILE *f = fopen(cpath, "wb");
    if (!f || fwrite(c.p, 1, c.n, f) != c.n) { fprintf(stderr, "bee: cannot write %s\n", cpath); return 1; }
    fclose(f);

    const char *cc = getenv("CC");
    if (!cc || !*cc) cc = "cc";
    char *cmd = fmt("%s -O2 -fwrapv -w -o '%s' '%s'", cc, exe, cpath);
    int rc = system(cmd);
    if (!keep) remove(cpath);
    if (rc != 0) {
        fprintf(stderr, "bee: C compiler failed (%s)\n", cmd);
        return 1;
    }
    return 0;
}
