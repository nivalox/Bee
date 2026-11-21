/* decl.c - declarations: top-level functions and globals, plus "int x", "char s[]" anywhere */
#include "bee.h"

static unsigned long long fbits(double d) {
    unsigned long long u;
    memcpy(&u, &d, sizeof u);
    return u;
}

/* traditional B initialiser: a constant or a name (the address of that name) */
char *ival(void) {
    Tok *t = &toks[pos];
    if (t->k == T_NUM) { pos++; return fmt("((W)%lluULL)", t->v); }
    if (t->k == T_FLT) { pos++; return fmt("((W)0x%llxULL)", fbits(t->d)); }
    if (t->k == T_CHR) { pos++; return chr_expr(t); }
    if (t->k == T_STR) { pos++; return str_expr(t); }
    if (isop("-") && toks[pos + 1].k == T_NUM) {
        pos += 2;
        return fmt("(-(W)%lluULL)", toks[pos - 1].v);
    }
    if (isop("-") && toks[pos + 1].k == T_FLT) {
        pos += 2;
        return fmt("((W)0x%llxULL)", fbits(-toks[pos - 1].d));
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
    die(t->line, "bad initialiser: expected a constant or a name but found %s", tokdesc(t));
    return NULL;
}

/* A global's initialiser, with its type. `want` is the declared type, or -1 to take the
 * literal's own type (inferred declaration). Global initialisers must be constants. */
static char *ival_const(int want, int *ty) {
    Tok *t = &toks[pos];
    int line = t->line;
    int neg = 0;
    if (isop("-") && (toks[pos + 1].k == T_NUM || toks[pos + 1].k == T_FLT)) {
        neg = 1;
        pos++;
        t = &toks[pos];
    }
    int lty;
    double num = 0;
    char *rv;
    switch (t->k) {
    case T_NUM:
        lty = TY_INT;
        num = neg ? -(double)t->v : (double)t->v;
        rv = neg ? fmt("(-(W)%lluULL)", t->v) : fmt("((W)%lluULL)", t->v);
        pos++;
        break;
    case T_FLT:
        lty = TY_FLOAT;
        num = neg ? -t->d : t->d;
        rv = fmt("((W)0x%llxULL)", fbits(num));
        pos++;
        break;
    case T_CHR:
        lty = TY_CHAR;
        num = (double)chr_val(t);
        if (want == TY_CHAR && t->len > 1 && pass == 2)
            die(line, "a char holds one character, but this constant has %d (use an int, or a char array)", t->len);
        rv = chr_expr(t);
        pos++;
        break;
    case T_STR:
        lty = TY_STR;
        rv = str_expr(t);
        pos++;
        break;
    default:
        if (want < 0 || want == TY_WORD) { /* inferred or untyped: a name is the address of that name */
            *ty = TY_WORD;
            return ival();
        }
        die(line, "a global initialiser must be a constant (number, character or string), found %s", tokdesc(t));
        return NULL;
    }
    *ty = lty;
    if (want >= 0 && pass == 2) {
        const char *err = assign_err(want, lty);
        if (err) die(line, "incompatible types: the variable is %s but the value is %s (%s)", tyname(want), tyname(lty), err);
    }
    if (want == TY_FLOAT && lty != TY_FLOAT) rv = fmt("((W)0x%llxULL)", fbits(num)); /* int constant widened at compile time */
    return rv;
}

/* "int", "char" or "float" starts a declaration only when a name follows: char is also the
 * builtin char(s, i), and an old B program may use these words as ordinary names. */
int at_type_decl(void) {
    if (toks[pos].k != T_ID) return -1;
    if (toks[pos + 1].k != T_ID || is_kw(toks[pos + 1].s)) return -1;
    const char *s = toks[pos].s;
    if (!strcmp(s, "int")) return TY_INT;
    if (!strcmp(s, "char")) return TY_CHAR;
    if (!strcmp(s, "float")) return TY_FLOAT;
    return -1;
}

static void emit_global(const char *name, long words) {
    bprintf(&defs, "#define G_%s %s\n", name, gend);
    gend = fmt("(G_%s + %ld)", name, words);
}

static void dup_check_local(const char *nm, int line) {
    int li = findloc(nm);
    if (pass == 2 && li >= 0 && li >= scope_start)
        die(line, "'%s' is already declared in this scope (first declared on line %d)", nm, locs[li].line);
}

/* int x;  int x = expr;  char c = 'B';  float f = 3.14;   (global or local) */
static void scalar_decl(Buf *o, int global, int ty, const char *nm, int line) {
    if (global) {
        char *rv = fmt("((W)0)");
        if (pass == 1) {
            if (findg(nm)) die(line, "'%s' is already defined", nm);
            Sym *s = addg(nm, K_VAR);
            s->ty = ty;
        }
        if (accept("=")) {
            int lty;
            rv = ival_const(ty, &lty);
        }
        if (pass == 2) {
            emit_global(nm, 1);
            bprintf(&init, "  mem[G_%s] = %s;\n", nm, rv);
        }
        return;
    }
    dup_check_local(nm, line);
    char *rv = fmt("((W)0)"); /* modern declarations start zeroed */
    if (accept("=")) {
        Ex r = expr();
        rv = convert(line, ty, r, fmt("'%s'", nm)); /* checked before 'nm' is in scope */
    }
    int slot = nslots++;
    addloc(nm, slot, line);
    locs[nloc - 1].ty = ty;
    bprintf(o, "mem[fp+%d] = %s;\n", slot, rv);
}

/* char name[] = "text";   char name[N];   char name[N] = "text";
 * A byte array: name[i] is the i-th character, and name itself is the address of the
 * packed string, so it works with printf("%s", name), char() and lchar(). Strings keep
 * Bee's *e (EOT) terminator rather than C's NUL. Storage is fixed: the name cannot be
 * reassigned. Global, or local inside a function (zeroed, then filled in each time the
 * declaration is reached). */
static void char_array(Buf *o, int global, const char *name, int line) {
    expect("[");
    long long size = -1; /* -1: take the size from the initialiser */
    if (!isop("]")) {
        size = const_val();
        if (size < 0 || size > (1 << 24)) die(line, "bad array size");
    }
    expect("]");
    Tok *in = NULL;
    if (accept("=")) {
        in = &toks[pos];
        if (in->k != T_STR) die(in->line, "a char array is initialised with a string literal, found %s", tokdesc(in));
        pos++;
    }
    if (size < 0 && !in) die(line, "size needed: write char %s[N] or char %s[] = \"...\"", name, name);
    long long need = in ? in->len + 1 : 0; /* text plus the *e terminator */
    if (size >= 0 && in && size < need)
        die(line, "string too long for char %s[%lld]: needs %lld with the *e terminator", name, size, need);
    long long nbytes = size >= 0 ? size : need;
    long words = (long)((nbytes + MAXCHR - 1) / MAXCHR);
    if (words < 1) words = 1;

    if (global) {
        if (pass == 1) {
            if (findg(name)) die(line, "'%s' is already defined", name);
            Sym *s = addg(name, K_VAR);
            s->ty = TY_STR;
            s->arr = 1;
        } else {
            emit_global(name, words + 1);
            bprintf(&init, "  mem[G_%s] = G_%s + 1;\n", name, name);
            if (in) {
                bprintf(&init, "  bee_str(G_%s + 1, \"", name);
                for (int i = 0; i < in->len; i++) bprintf(&init, "\\%03o", (unsigned char)in->s[i]);
                bprintf(&init, "\", %d);\n", in->len);
            }
        }
        return;
    }
    dup_check_local(name, line);
    int slot = nslots++;
    int base = nslots;
    nslots += (int)words;
    addloc(name, slot, line);
    locs[nloc - 1].ty = TY_STR;
    locs[nloc - 1].arr = 1;
    bprintf(&vinit, "  mem[fp+%d] = fp+%d;\n", slot, base);
    bprintf(o, "bee_zero(fp+%d, %ld);\n", base, words);
    if (in) {
        bprintf(o, "bee_str(fp+%d, \"", base);
        for (int i = 0; i < in->len; i++) bprintf(o, "\\%03o", (unsigned char)in->s[i]);
        bprintf(o, "\", %d);\n", in->len);
    }
}

/* int a = 1, b;   char c = 'x', s[] = "text";   float f = 2.5;   (the type word is current) */
void var_decl(Buf *o, int global) {
    int ty = at_type_decl();
    const char *tword = toks[pos].s;
    pos++;
    for (;;) {
        Tok *n = &toks[pos];
        if (n->k != T_ID || is_kw(n->s)) die(n->line, "expected a name after '%s' but found %s", tword, tokdesc(n));
        const char *nm = n->s;
        int line = n->line;
        pos++;
        if (isop("[")) {
            if (ty != TY_CHAR)
                die(line, "arrays of %s are not supported (use 'auto %s[N]' for a B word vector, or 'char %s[]' for a string)", tword, nm, nm);
            char_array(o, global, nm, line);
        } else {
            scalar_decl(o, global, ty, nm, line);
        }
        if (!accept(",")) break;
    }
    expect(";");
}

/* name = expr;  where name does not exist yet: creates a local with the type of the value.
 * (Historical B treated the first use of an undeclared name as an "internal" variable.) */
void infer_stmt(Buf *o) {
    Tok *n = &toks[pos];
    int line = n->line;
    const char *nm = n->s;
    pos += 2; /* name and '=' */
    Ex r = expr(); /* parsed before the name exists: x = x + 1 does not see the new x */
    expect(";");
    int slot = nslots++;
    addloc(nm, slot, line);
    locs[nloc - 1].ty = r.ty;
    bprintf(o, "mem[fp+%d] = %s;\n", slot, r.rv);
}

/* ---- top level --------------------------------------------------------------------- */

static int is_type_word(const char *s) { return !strcmp(s, "int") || !strcmp(s, "char") || !strcmp(s, "float"); }

void toplevel(void) {
    int rty = TY_WORD, rdecl = 0;
    int tdecl = at_type_decl();
    if (tdecl >= 0) {
        if (toks[pos + 2].k == T_OP && !strcmp(toks[pos + 2].s, "(")) { /* int add(int a, int b) ... */
            rty = tdecl;
            rdecl = 1;
            pos++;
        } else {
            var_decl(NULL, 1); /* int x = 5;  char s[] = "...";  float f; */
            return;
        }
    }

    Tok *t = &toks[pos];
    if (t->k == T_ID && !is_kw(t->s) && toks[pos + 1].k == T_ID) {
        const char *u = unsupported(t->s);
        if (u) die(t->line, "unsupported syntax: %s", u);
    }
    if (t->k != T_ID || is_kw(t->s)) die(t->line, "expected a name at top level but found %s", tokdesc(t));
    char *name = t->s;
    int line = t->line;
    pos++;
    Sym *s = findg(name);

    if (isop("(")) {
        /* function definition: [type] name(params) statement */
        if (pass == 1) {
            if (s) die(line, "'%s' is already defined", name);
            s = addg(name, K_FN);
            s->fidx = nfuncs++;
            s->ty = rty;
            s->typed = rdecl;
        }
        pos++;
        nloc = 0;
        nslots = 0;
        scope_start = 0;
        vinit.n = 0;
        if (vinit.p) vinit.p[0] = 0;
        int np = 0;
        int ptys[8];
        int ptyped = 0;
        if (!isop(")")) {
            for (;;) {
                int pty = TY_WORD;
                int tw = at_type_decl();
                if (tw >= 0) {
                    pos++;
                    pty = tw;
                    ptyped = 1;
                } else if (toks[pos].k == T_ID && is_type_word(toks[pos].s) &&
                           toks[pos + 1].k == T_OP && (!strcmp(toks[pos + 1].s, ",") || !strcmp(toks[pos + 1].s, ")"))) {
                    die(toks[pos].line, "malformed parameter list in '%s': '%s' needs a parameter name after it", name, toks[pos].s);
                }
                Tok *p = &toks[pos];
                if (p->k == T_ID && !is_kw(p->s) && toks[pos + 1].k == T_ID && unsupported(p->s))
                    die(p->line, "unsupported syntax: %s", unsupported(p->s));
                if (p->k != T_ID || is_kw(p->s))
                    die(p->line, "malformed parameter list in '%s': expected a parameter name but found %s", name, tokdesc(p));
                if (np == 8) die(p->line, "too many parameters (max 8)");
                if (findloc(p->s) >= 0) die(p->line, "duplicate parameter '%s' in '%s'", p->s, name);
                pos++;
                addloc(p->s, nslots++, p->line);
                if (isop("[")) { /* char s[]: a string parameter */
                    if (pty != TY_CHAR) die(p->line, "parameter '%s': only 'char %s[]' can be an array parameter", p->s, p->s);
                    pos++;
                    expect("]");
                    pty = TY_STR;
                }
                locs[nloc - 1].ty = pty;
                ptys[np] = pty;
                np++;
                if (!accept(",")) break;
            }
        }
        if (!isop(")"))
            die(toks[pos].line, "malformed parameter list in '%s': expected ',' or ')' but found %s", name, tokdesc(&toks[pos]));
        pos++;
        if (pass == 1) {
            s->np = np;
            memcpy(s->pty, ptys, sizeof ptys);
            if (ptyped) s->typed = 1;
        }
        if (toks[pos].k == T_EOF) die(line, "function '%s' has no body", name);
        if (isop(";") && (rdecl || ptyped))
            die(line, "'%s' looks like a declaration without a body; Bee needs no prototypes (a function can be called before it is defined)", name);
        cur_ret = rty;
        cur_ret_declared = rdecl;
        cur_fn = name;
        flat_body = isop("{"); /* the body block shares scope with the parameters */
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

    if (rdecl) die(line, "expected '(' after the function name '%s'", name);

    /* name = constant;   modern inferred global: the type comes from the value */
    if (isop("=")) {
        pos++;
        int lty;
        char *rv = ival_const(-1, &lty);
        expect(";");
        if (pass == 1) {
            if (s) die(line, "'%s' is already defined", name);
            s = addg(name, K_VAR);
            s->ty = lty;
        } else {
            emit_global(name, 1);
            bprintf(&init, "  mem[G_%s] = %s;\n", name, rv);
        }
        return;
    }

    /* traditional global variable or vector: name [ [size] ] [ival {, ival}] ; */
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
        emit_global(name, words);
        if (vec) bprintf(&init, "  mem[G_%s] = G_%s + 1;\n", name, name);
        for (int i = 0; i < nin; i++) bprintf(&init, "  mem[G_%s + %d] = %s;\n", name, vec ? i + 1 : i, iv[i]);
    }
    free(iv);
}
