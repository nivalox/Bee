/* bee.h - shared declarations for the Bee compiler.
 *
 *   util.c     buffers, string building, errors
 *   lexer.c    source -> tokens
 *   symbols.c  global and local symbol tables
 *   types.c    Bee's value types and the assignment-compatibility rules
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
void warn(int line, const char *f, ...);
void bputn(Buf *b, const char *s, size_t n);
void bput(Buf *b, const char *s);
const char *bstr(Buf *b);
char *fmt(const char *f, ...);
void bprintf(Buf *b, const char *f, ...);
char *dupn(const char *s, size_t n);

/* ---- lexer.c ---- */
enum { T_EOF, T_NUM, T_FLT, T_CHR, T_STR, T_ID, T_OP };

typedef struct {
    int k;
    char *s; /* identifier, operator, or the decoded bytes of a char/string literal */
    int len; /* byte length of a char/string literal */
    unsigned long long v;
    double d; /* value of a T_FLT literal */
    int line;
} Tok;

extern Tok *toks;
extern int ntoks;
extern int modern; /* -m: do not read "=+" style assignment operators */
extern int strict; /* -s: B escapes only; a backslash is an ordinary character */
void lex(const char *s);

/* ---- types.c ---- */
/* Bee has one machine word. Traditional B names (auto, extrn, parameters, globals) are
 * untyped words, TY_WORD, and are never checked. Names declared the modern way (int, char,
 * float, char x[]) or created by inference have a stable type that is checked at compile
 * time. A float is a double stored bit-for-bit in the word. */
enum { TY_WORD, TY_INT, TY_CHAR, TY_FLOAT, TY_STR };

const char *tyname(int ty);
/* NULL if a value of type src may be stored in a variable of type dst, else why not */
const char *assign_err(int dst, int src);

/* ---- symbols.c ---- */
enum { K_VAR, K_FN };

typedef struct {
    char *name;
    int kind;
    int fidx;     /* function-table index, for K_FN */
    int builtin;
    int ty;       /* K_VAR: the variable's type; K_FN: the return type */
    int arr;      /* declared char name[...]: fixed storage, cannot be reassigned */
    int implicit; /* created by 'extrn name;' with no definition: a zeroed global word */
    int typed;    /* K_FN: written with a return type or typed parameters, so calls are checked */
    int np;       /* K_FN: number of parameters */
    int pty[8];   /* K_FN: parameter types */
} Sym;

typedef struct { char *name; int off; int ty; int arr; int line; } Loc;

extern Sym *gsyms;
extern int ngs;
extern int nfuncs;
extern const char *builtins[];
extern Loc locs[];
extern int nloc;
extern int nslots; /* words in the current function's frame */
extern int scope_start; /* first local of the innermost block, for duplicate checks */

Sym *findg(const char *n);
Sym *addg(const char *n, int kind);
int findloc(const char *n);
void addloc(const char *n, int off, int line);
void note_extrn(const char *name);
void register_implicit(void);

/* A character constant is one machine word; Bee words are 64-bit on every target. */
#define MAXCHR 8

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
extern int cur_ret;         /* declared return type of the function being parsed */
extern int cur_ret_declared; /* ... and whether it was written (int f() rather than f()) */
extern const char *cur_fn;
extern int flat_body;        /* the next block is a function body: it shares scope with the parameters */

int isop(const char *o);
int isid(const char *k);
int accept(const char *o);
void expect(const char *o);
const char *tokdesc(Tok *t);
int is_kw(const char *s);
unsigned long long chr_val(Tok *t);
long long const_val(void);
char *chr_expr(Tok *t);
char *str_expr(Tok *t);
const char *unsupported(const char *id);

/* ---- expr.c ---- */
/* rv:   C expression for the value (for an lvalue, the C lvalue itself)
 * addr: C expression for the word address, non-NULL only for lvalues
 * fn:   name, if this is a bare function name (so a call can be direct) */
typedef struct {
    char *rv, *addr, *fn;
    int ty;      /* TY_* of the value */
    int isbyte;  /* a byte element of a string/char array (rv is an unsigned char lvalue) */
    int isarr;   /* the name of a char array declaration */
    const char *name; /* variable name, for diagnostics */
    Tok *lit;    /* the literal token, if this expression is exactly a string/char literal */
} Ex;

Ex expr(void);
char *cond_rv(Ex e);                 /* C condition for an expression (floats test != 0.0) */
char *convert(int line, int dst, Ex e, const char *what); /* check assign dst <- e, convert int->float */

/* ---- stmt.c / decl.c ---- */
void stmt(Buf *o);
char *ival(void);
int at_type_decl(void); /* TY_INT/TY_CHAR/TY_FLOAT if a declaration "type name" starts here, else -1 */
void var_decl(Buf *o, int global);
void infer_stmt(Buf *o);
void toplevel(void);

#endif
