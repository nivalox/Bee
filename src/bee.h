/* bee.h - shared declarations for the Bee compiler.
 *
 *   util.c     buffers, string building, errors
 *   lexer.c    source -> tokens
 *   symbols.c  global and local symbol tables
 *   parser.c   parser state and token/literal helpers
 *   expr.c     expressions
 *   stmt.c     statements
 *   decl.c     top level (functions, globals)
 *   main.c     driver
 *   runtime/   bee_rt.h, the C runtime embedded in every output file
 */
#ifndef BEE_H
#define BEE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- util.c ---- */
typedef struct { char *p; size_t n, cap; } Buf;

extern const char *srcname;
void oom(void);
#if defined(__GNUC__)
__attribute__((noreturn, format(printf, 2, 3)))
#endif
void die(int line, const char *f, ...);
void bputn(Buf *b, const char *s, size_t n);
void bput(Buf *b, const char *s);
const char *bstr(Buf *b);
char *fmt(const char *f, ...);
void bprintf(Buf *b, const char *f, ...);
char *dupn(const char *s, size_t n);

/* ---- lexer.c ---- */
enum { T_EOF, T_NUM, T_CHR, T_STR, T_ID, T_OP };

typedef struct {
    int k;
    char *s; /* identifier, operator, or the decoded bytes of a char/string literal */
    int len; /* byte length of a char/string literal */
    unsigned long long v;
    int line;
} Tok;

extern Tok *toks;
extern int ntoks;
extern int modern; /* -m: do not read "=+" style assignment operators */
extern int strict; /* -s: B escapes only; a backslash is an ordinary character */
void lex(const char *s);

/* ---- symbols.c ---- */
enum { K_VAR, K_FN };

typedef struct {
    char *name;
    int kind;
    int fidx; /* function-table index, for K_FN */
    int builtin;
} Sym;

typedef struct { char *name; int off; } Loc;

extern Sym *gsyms;
extern int ngs;
extern int nfuncs;
extern const char *builtins[];
extern Loc locs[];
extern int nloc;
extern int nslots; /* words in the current function's frame */

Sym *findg(const char *n);
Sym *addg(const char *n, int kind);
int findloc(const char *n);
void addloc(const char *n, int off, int line);

/* ---- parser.c ---- */
extern int pass; /* 1 = collect names, 2 = emit */
extern int pos;
extern Buf defs;   /* #define G_x / S_n addresses */
extern Buf protos; /* function prototypes */
extern Buf funcs;  /* function bodies */
extern Buf init;   /* global/string initialisation, run before main */
extern Buf vinit;  /* local vector setup for the function being parsed */
extern char *gend; /* C expression for the next free global word */
extern int nstr;

int isop(const char *o);
int isid(const char *k);
int accept(const char *o);
void expect(const char *o);
int is_kw(const char *s);
unsigned long long chr_val(Tok *t);
long long const_val(void);
char *chr_expr(Tok *t);
char *str_expr(Tok *t);

/* ---- expr.c ---- */
/* rv:   C expression for the value (for an lvalue, the C lvalue itself)
 * addr: C expression for the word address, non-NULL only for lvalues
 * fn:   name, if this is a bare function name (so a call can be direct) */
typedef struct { char *rv, *addr, *fn; } Ex;

Ex expr(void);

/* ---- stmt.c / decl.c ---- */
void stmt(Buf *o);
char *ival(void);
void toplevel(void);

#endif
