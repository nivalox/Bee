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
int cur_ret = TY_WORD;
int cur_ret_declared;
const char *cur_fn = "";
int flat_body;

int isop(const char *o) { return toks[pos].k == T_OP && !strcmp(toks[pos].s, o); }
int isid(const char *k) { return toks[pos].k == T_ID && !strcmp(toks[pos].s, k); }
int accept(const char *o) {
    if (isop(o)) { pos++; return 1; }
    return 0;
}
/* the token as the user wrote it, for "expected X but found Y" messages */
const char *tokdesc(Tok *t) {
    switch (t->k) {
    case T_EOF: return "end of file";
    case T_NUM: return "a number";
    case T_FLT: return "a number";
    case T_CHR: return "a character constant";
    case T_STR: return "a string";
    default: return fmt("'%s'", t->s);
    }
}

void expect(const char *o) {
    if (!isop(o)) die(toks[pos].line, "expected '%s' but found %s", o, tokdesc(&toks[pos]));
    pos++;
}

/* C and C++ words Bee does not have, with what to use instead. NULL for anything else. */
const char *unsupported(const char *id) {
    static const struct { const char *w, *msg; } T[] = {
        {"for", "'for' loops are not part of Bee yet; use while"},
        {"do", "'do ... while' is not part of Bee yet; use while"},
        {"void", "'void' is not a Bee type; leave the return type off, or use int"},
        {"double", "there is no 'double'; use float (Bee floats are 64-bit)"},
        {"long", "Bee has one integer type, int (a 64-bit word)"},
        {"short", "Bee has one integer type, int (a 64-bit word)"},
        {"unsigned", "Bee has one integer type, int (a 64-bit word)"},
        {"signed", "Bee has one integer type, int (a 64-bit word)"},
        {"struct", "'struct' is not supported (B has no structures; use vectors)"},
        {"union", "'union' is not supported"},
        {"enum", "'enum' is not supported"},
        {"typedef", "'typedef' is not supported"},
        {"static", "'static' is not supported"},
        {"const", "'const' is not supported"},
        {"register", "'register' is not supported"},
        {"volatile", "'volatile' is not supported"},
        {"sizeof", "'sizeof' is not supported"},
        {0, 0}};
    for (int i = 0; T[i].w; i++)
        if (!strcmp(id, T[i].w)) return T[i].msg;
    return NULL;
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
    if (t->len > MAXCHR) die(t->line, "character constant too long (%d characters, max %d)", t->len, MAXCHR);
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
    if (t->len > MAXCHR) die(t->line, "character constant too long (%d characters, max %d)", t->len, MAXCHR);
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
