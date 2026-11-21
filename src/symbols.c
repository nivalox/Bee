/* symbols.c - global (function/variable) and per-function local symbol tables */
#include "bee.h"

Sym *gsyms;
int ngs, capgs;
int nfuncs = 1; /* ftab[0] stays empty so 0 is never a valid function */

Sym *findg(const char *n) {
    for (int i = 0; i < ngs; i++)
        if (!strcmp(gsyms[i].name, n)) return &gsyms[i];
    return NULL;
}
Sym *addg(const char *n, int kind) {
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
const char *builtins[] = {"putchar", "getchar", "char", "lchar", "printf", "printn", "putnumb", "exit", 0};

Loc locs[512];
int nloc;
int nslots; /* words in the current function's frame */

int findloc(const char *n) {
    for (int i = nloc - 1; i >= 0; i--)
        if (!strcmp(locs[i].name, n)) return i;
    return -1;
}
void addloc(const char *n, int off, int line) {
    if (nloc == (int)(sizeof locs / sizeof *locs)) die(line, "too many local names");
    locs[nloc].name = dupn(n, strlen(n));
    locs[nloc].off = off;
    nloc++;
}
