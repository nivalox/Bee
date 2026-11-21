/* parser.c - parser state, token helpers, constant and string/char literal emission */
#include "bee.h"

int pass; /* 1 = collect names, 2 = emit */
int pos;
Buf defs;   /* #define G_x / S_n addresses */
Buf protos; /* function prototypes */
Buf funcs;  /* function bodies */
Buf init;   /* global/string initialisation, run before main */
Buf vinit;  /* local vector setup for the function being parsed */
char *gend; /* C expression for the next free global word */
int nstr;

int isop(const char *o) { return toks[pos].k == T_OP && !strcmp(toks[pos].s, o); }
int isid(const char *k) { return toks[pos].k == T_ID && !strcmp(toks[pos].s, k); }
int accept(const char *o) {
    if (isop(o)) { pos++; return 1; }
    return 0;
}
void expect(const char *o) {
    if (!isop(o)) die(toks[pos].line, "expected '%s'", o);
    pos++;
}

int is_kw(const char *s) {
    static const char *kw[] = {"auto", "extrn", "if", "else", "while", "switch", "case", "default",
                               "break", "continue", "goto", "return", 0};
    for (int i = 0; kw[i]; i++)
        if (!strcmp(s, kw[i])) return 1;
    return 0;
}

/* value of a character constant, first char in the low byte */
unsigned long long chr_val(Tok *t) {
    unsigned long long v = 0;
    if (t->len > 8) die(t->line, "character constant too long");
    for (int i = 0; i < t->len; i++) v |= (unsigned long long)(unsigned char)t->s[i] << (8 * i);
    return v;
}

long long const_val(void) {
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
char *chr_expr(Tok *t) {
    Buf b = {0};
    bput(&b, "((W)(");
    if (t->len == 0) bput(&b, "0");
    for (int i = 0; i < t->len; i++) bprintf(&b, "%sBCH(%d,%d)", i ? "|" : "", i, (unsigned char)t->s[i]);
    bput(&b, "))");
    return b.p;
}

/* C expression for a string literal: allocates its words and queues its initialisation */
char *str_expr(Tok *t) {
    if (pass == 1) return fmt("0");
    int k = nstr++;
    bprintf(&defs, "#define S_%d %s\n", k, gend);
    gend = fmt("(S_%d + BEE_WORDS(%d))", k, t->len);
    bprintf(&init, "  bee_str(S_%d, \"", k);
    for (int i = 0; i < t->len; i++) bprintf(&init, "\\%03o", (unsigned char)t->s[i]);
    bprintf(&init, "\", %d);\n", t->len);
    return fmt("((W)S_%d)", k);
}
