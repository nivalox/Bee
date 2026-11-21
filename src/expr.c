/* expr.c - expression parser: B expressions -> C expressions, with Bee's value types
 *
 * Every Ex carries a type (TY_*). Untyped B words (TY_WORD) pass everything unchecked,
 * exactly as in B. Typed values (int, char, float, string) are checked, and ints widen to
 * floats. Floats are doubles stored bit-for-bit in the machine word, so a float flows
 * through memory, calls and returns like any other word; only arithmetic knows the
 * difference (it goes through the f*() helpers in the runtime). */
#include <ctype.h>
#include "bee.h"

static Ex mkt(char *rv, int ty) {
    Ex e;
    memset(&e, 0, sizeof e);
    e.rv = rv;
    e.ty = ty;
    return e;
}
static Ex mk(char *rv) { return mkt(rv, TY_WORD); }
static Ex lval(char *rv, char *addr, int ty) {
    Ex e = mkt(rv, ty);
    e.addr = addr;
    return e;
}

static unsigned long long fbits(double d) {
    unsigned long long u;
    memcpy(&u, &d, sizeof u);
    return u;
}

/* the value as a float (an int is converted, a float is already one) */
static char *tof(Ex e) { return e.ty == TY_FLOAT ? e.rv : fmt("i2f(%s)", e.rv); }

char *cond_rv(Ex e) { return e.ty == TY_FLOAT ? fmt("ftruth(%s)", e.rv) : e.rv; }

/* Check that e can be stored where a dst-typed value is expected, and return the C
 * expression to store (an int stored in a float is converted). `what` names the target
 * for the message, e.g. 'age' or argument 2 of 'add'. Type errors are only reported in
 * pass 2, when every global and function signature is known. */
char *convert(int line, int dst, Ex e, const char *what) {
    if (pass == 2) {
        const char *err = assign_err(dst, e.ty);
        if (err)
            die(line, "incompatible types: %s is %s but the value is %s (%s)", what, tyname(dst), tyname(e.ty), err);
        if (dst == TY_CHAR && e.lit && e.lit->k == T_CHR && e.lit->len > 1)
            die(line, "a char holds one character, but this constant has %d (use an int for packed characters, or a char array)", e.lit->len);
    }
    if (dst == TY_FLOAT && e.ty != TY_FLOAT) return fmt("i2f(%s)", e.rv);
    return e.rv;
}

static const char *target_name(Ex l) {
    if (l.name) return fmt("'%s'", l.name);
    return l.isbyte ? "a character element" : "the assignment target";
}

/* ---- printf format checking -------------------------------------------------------- */

/* When printf's format is a literal, check each conversion against the type of its
 * argument. Mismatches that would print garbage (a float given to %d, an int to %f, ...)
 * are errors; a wrong argument count is only a warning, as in B. */
static void check_printf(int line, Tok *fl, Ex *av, int n) {
    const char *f = fl->s;
    int len = fl->len, need = 0;
    for (int i = 0; i < len; i++) {
        if (f[i] != '%') continue;
        i++;
        while (i < len && (f[i] == '-' || f[i] == '0')) i++;
        while (i < len && isdigit((unsigned char)f[i])) i++;
        if (i < len && f[i] == '.') {
            i++;
            while (i < len && isdigit((unsigned char)f[i])) i++;
        }
        if (i >= len) break;
        char c = f[i];
        if (c == '%') continue;
        if (!strchr("doxcsfeg", c)) {
            warn(line, "printf: unknown conversion '%%%c' is printed as written", c);
            continue;
        }
        need++;
        if (need >= n) continue; /* missing argument: reported below */
        int ty = av[need].ty;
        if (strchr("doxc", c) && (ty == TY_FLOAT || ty == TY_STR))
            die(line, "printf: '%%%c' expects an integer but argument %d is %s (use %s)", c, need, tyname(ty), ty == TY_FLOAT ? "%f" : "%s");
        if (c == 's' && (ty == TY_INT || ty == TY_CHAR || ty == TY_FLOAT))
            die(line, "printf: '%%s' expects a string but argument %d is %s", need, tyname(ty));
        if (strchr("feg", c) && (ty == TY_INT || ty == TY_CHAR || ty == TY_STR))
            die(line, "printf: '%%%c' expects a float but argument %d is %s (write 3.0 rather than 3, or multiply by 1.0)", c, need, tyname(ty));
    }
    if (need > n - 1) warn(line, "printf: the format needs %d argument(s) but only %d given", need, n - 1);
    else if (need < n - 1) warn(line, "printf: the format uses %d argument(s) but %d given; the extra ones are ignored", need, n - 1);
}

/* ---- primary / postfix / unary ----------------------------------------------------- */

static Ex primary(void) {
    Tok *t = &toks[pos];
    switch (t->k) {
    case T_NUM:
        pos++;
        return mkt(fmt("((W)%lluULL)", t->v), TY_INT);
    case T_FLT:
        pos++;
        return mkt(fmt("((W)0x%llxULL)", fbits(t->d)), TY_FLOAT);
    case T_CHR: {
        pos++;
        Ex e = mkt(chr_expr(t), TY_CHAR);
        e.lit = t;
        return e;
    }
    case T_STR: {
        pos++;
        Ex e = mkt(str_expr(t), TY_STR);
        e.lit = t;
        return e;
    }
    case T_ID: {
        if (is_kw(t->s)) die(t->line, "unexpected '%s'", t->s);
        pos++;
        int li = findloc(t->s);
        if (li >= 0) {
            Ex e = lval(fmt("mem[fp+%d]", locs[li].off), fmt("(fp+%d)", locs[li].off), locs[li].ty);
            e.isarr = locs[li].arr;
            e.name = locs[li].name;
            return e;
        }
        Sym *s = findg(t->s);
        if (s && s->kind == K_VAR) {
            Ex e = lval(fmt("mem[G_%s]", t->s), fmt("((W)G_%s)", t->s), s->ty);
            e.isarr = s->arr;
            e.name = s->name;
            return e;
        }
        if (s && s->kind == K_FN) {
            Ex e = mk(fmt("((W)%d)", s->fidx));
            e.fn = t->s;
            return e;
        }
        const char *u = unsupported(t->s);
        if (u) die(t->line, "unsupported syntax: %s", u);
        if (pass == 2) {
            if (isop("("))
                die(t->line, "undefined function '%s'", t->s);
            die(t->line, "undefined variable '%s' (declare it with int/auto, or create it by assigning to it: %s = ...;)", t->s, t->s);
        }
        return lval(fmt("mem[0]"), fmt("0"), TY_WORD); /* pass 1: may be defined later, output is discarded */
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
        die(t->line, "expected an expression but found %s", tokdesc(t));
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
            Ex av[8];
            int n = 0;
            if (!isop(")")) {
                for (;;) {
                    Ex a = expr();
                    if (n == 8) die(line, "too many arguments (max 8)");
                    av[n] = a;
                    args[n++] = a.rv;
                    if (!accept(",")) break;
                }
            }
            expect(")");
            int rty = TY_WORD;
            Sym *fs = e.fn ? findg(e.fn) : NULL;
            if (fs && fs->kind == K_FN) {
                rty = fs->ty;
                if (pass == 2 && !fs->builtin && fs->typed) {
                    /* calls to functions written with types are checked and converted */
                    if (n != fs->np) die(line, "function '%s' takes %d argument(s) but %d given", fs->name, fs->np, n);
                    for (int i = 0; i < n; i++)
                        args[i] = convert(line, fs->pty[i], av[i], fmt("argument %d of '%s'", i + 1, fs->name));
                }
                if (pass == 2 && fs->builtin && !strcmp(fs->name, "printf") && n >= 1 && av[0].lit && av[0].lit->k == T_STR)
                    check_printf(line, av[0].lit, av, n);
            }
            Buf b = {0};
            if (e.fn) bprintf(&b, "F_%s(", e.fn);
            else bprintf(&b, "callf(%s, ", e.rv);
            for (int i = 0; i < 8; i++) bprintf(&b, "%s%s", i ? ", " : "", i < n ? args[i] : "0");
            bput(&b, ")");
            e = mkt(b.p, rty);
        } else if (isop("[")) {
            int line = toks[pos].line;
            pos++;
            Ex i = expr();
            expect("]");
            if (pass == 2 && i.ty == TY_FLOAT) die(line, "an index must be an integer, not a float");
            if (e.ty == TY_STR) { /* byte element of a string or char array */
                e = lval(fmt("(*bytep(%s, %s))", e.rv, i.rv), fmt("0"), TY_CHAR);
                e.isbyte = 1;
                continue;
            }
            if (pass == 2 && e.ty == TY_FLOAT) die(line, "a float cannot be indexed");
            char *addr = fmt("(%s+%s)", e.rv, i.rv);
            e = lval(fmt("(*mp(%s))", addr), addr, TY_WORD);
        } else if (isop("++") || isop("--")) {
            if (!e.addr) die(toks[pos].line, "lvalue required for '%s'", toks[pos].s);
            if (pass == 2 && (e.ty == TY_FLOAT || e.ty == TY_STR))
                die(toks[pos].line, "'%s' is not defined for %s", toks[pos].s, e.ty == TY_FLOAT ? "floats" : "strings");
            e = mkt(fmt("(%s%s)", e.rv, toks[pos].s), e.ty);
            pos++;
        } else {
            break;
        }
    }
    return e;
}

static Ex unary(void) {
    int line = toks[pos].line;
    if (isop("-")) {
        pos++;
        Ex e = unary();
        if (e.ty == TY_FLOAT) return mkt(fmt("fneg(%s)", e.rv), TY_FLOAT);
        if (pass == 2 && e.ty == TY_STR) die(line, "unary '-' cannot be applied to a string");
        return mkt(fmt("(-%s)", e.rv), e.ty == TY_WORD ? TY_WORD : TY_INT);
    }
    if (isop("!")) {
        pos++;
        Ex e = unary();
        if (e.ty == TY_FLOAT) return mkt(fmt("((W)fnot(%s))", e.rv), TY_INT);
        return mkt(fmt("((W)(!%s))", e.rv), TY_INT);
    }
    if (isop("*")) {
        pos++;
        Ex e = unary();
        return lval(fmt("(*mp(%s))", e.rv), e.rv, TY_WORD);
    }
    if (isop("&")) {
        pos++;
        Ex e = unary();
        if (e.fn) return mk(e.rv);
        if (e.isbyte) die(line, "cannot take the address of a char element");
        if (!e.addr) die(line, "lvalue required for '&'");
        return mk(fmt("((W)%s)", e.addr));
    }
    if (isop("++") || isop("--")) {
        char *op = toks[pos].s;
        pos++;
        Ex e = unary();
        if (!e.addr) die(line, "lvalue required for '%s'", op);
        if (pass == 2 && (e.ty == TY_FLOAT || e.ty == TY_STR))
            die(line, "'%s' is not defined for %s", op, e.ty == TY_FLOAT ? "floats" : "strings");
        return mkt(fmt("(%s%s)", op, e.rv), e.ty);
    }
    return postfix();
}

/* ---- binary operators -------------------------------------------------------------- */

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

static int is_cmp(const char *op) {
    return !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") ||
           !strcmp(op, ">=");
}
static int is_logic(const char *op) { return !strcmp(op, "&&") || !strcmp(op, "||"); }

static Ex arith(int line, const char *op, Ex l, Ex r) {
    if (l.ty == TY_STR || r.ty == TY_STR) {
        /* a string is its address: == and != compare addresses, nothing else makes sense */
        if (strcmp(op, "==") && strcmp(op, "!="))
            die(line, "operator '%s' cannot be applied to a string (work with its characters: s[i], char(s, i))", op);
        return mkt(binop(op, l.rv, r.rv), TY_INT);
    }
    if (l.ty == TY_FLOAT || r.ty == TY_FLOAT) {
        static const struct { const char *op, *fn; } F[] = {
            {"+", "fadd"}, {"-", "fsub"}, {"*", "fmul"}, {"/", "fdiv"},
            {"<", "flt"}, {"<=", "fle"}, {">", "fgt"}, {">=", "fge"}, {"==", "feq"}, {"!=", "fne"}, {0, 0}};
        for (int i = 0; F[i].op; i++)
            if (!strcmp(op, F[i].op))
                return mkt(fmt("%s(%s, %s)", F[i].fn, tof(l), tof(r)), is_cmp(op) ? TY_INT : TY_FLOAT);
        if (is_logic(op))
            return mkt(fmt("((W)(%s %s %s))", cond_rv(l), op, cond_rv(r)), TY_INT);
        die(line, "operator '%s' is not defined for floats", op);
    }
    int ty = (is_cmp(op) || is_logic(op)) ? TY_INT : (l.ty == TY_WORD && r.ty == TY_WORD ? TY_WORD : TY_INT);
    return mkt(binop(op, l.rv, r.rv), ty);
}

static Ex binary(int minp) {
    Ex l = unary();
    for (;;) {
        if (toks[pos].k != T_OP) break;
        int p = binprec(toks[pos].s);
        if (p < minp) break;
        char *op = toks[pos].s;
        int line = toks[pos].line;
        pos++;
        Ex r = binary(p + 1);
        l = arith(line, op, l, r);
    }
    return l;
}

static Ex cond(void) {
    Ex c = binary(0);
    if (accept("?")) {
        int line = toks[pos].line;
        Ex a = expr();
        expect(":");
        Ex b = cond();
        int ty;
        char *ar = a.rv, *br = b.rv;
        if (a.ty == b.ty) {
            ty = a.ty;
        } else if (a.ty == TY_FLOAT || b.ty == TY_FLOAT) {
            if (a.ty == TY_STR || b.ty == TY_STR) die(line, "the two branches of '?:' have incompatible types (a float and a string)");
            ty = TY_FLOAT;
            ar = tof(a);
            br = tof(b);
        } else if (a.ty == TY_STR || b.ty == TY_STR) {
            ty = TY_WORD;
        } else {
            ty = TY_INT;
        }
        return mkt(fmt("((%s) ? (%s) : (%s))", cond_rv(c), ar, br), ty);
    }
    return c;
}

/* ---- assignment -------------------------------------------------------------------- */

/* B's six relational assignment operators, as the lexer spells them, and the operator
 * each one applies: "=<" is x = x < y. */
static const char *rel_assign(const char *tok) {
    static const struct { const char *tok, *op; } T[] = {
        {"=<", "<"}, {"=<=", "<="}, {"=>", ">"}, {"=>=", ">="}, {"===", "=="}, {"=!=", "!="}, {0, 0}};
    for (int i = 0; T[i].tok; i++)
        if (!strcmp(tok, T[i].tok)) return T[i].op;
    return NULL;
}

Ex expr(void) {
    Ex l = cond();
    if (toks[pos].k == T_OP) {
        const char *op = toks[pos].s;
        static const char *aops[] = {"+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", 0};
        const char *rel = rel_assign(op);
        int compound = rel != NULL;
        for (int i = 0; aops[i]; i++)
            if (!strcmp(op, aops[i])) compound = 1;
        if (!strcmp(op, "=") || compound) {
            int line = toks[pos].line;
            if (!l.addr && l.fn) die(line, "cannot assign to '%s': it is a function, not a variable", l.fn);
            if (!l.addr) die(line, "lvalue required on the left of '%s'", op);
            pos++;
            Ex r = expr();
            const char *what = target_name(l);

            if (!compound) {
                if (pass == 2 && l.isarr)
                    die(line, "cannot assign to the char array '%s': its storage is fixed (set characters with %s[i] = ..., or keep a string in a plain variable: s = \"...\")", l.name, l.name);
                char *rv = convert(line, l.ty, r, what);
                return mkt(fmt("(%s = %s)", l.rv, rv), l.ty);
            }

            /* compound: x op= y */
            char bop[4] = {0};
            if (rel) strcpy(bop, rel);
            else { strcpy(bop, op); bop[strlen(bop) - 1] = 0; } /* "+=" -> "+" */
            if (pass == 2) {
                if (l.isarr) die(line, "cannot assign to the char array '%s'", l.name);
                if (l.ty == TY_STR) die(line, "operator '%s' cannot be applied to a string variable (%s)", op, what);
                if (r.ty == TY_STR) die(line, "operator '%s' cannot be applied to a string", op);
                if (r.ty == TY_FLOAT && l.ty != TY_FLOAT)
                    die(line, "incompatible types: %s is %s but the value is a float (%s)", what, tyname(l.ty),
                        l.ty == TY_WORD ? "an untyped word cannot hold the result of float arithmetic; use a float variable"
                                        : "a float is never converted to an integer implicitly");
            }
            if (l.ty == TY_FLOAT) {
                static const struct { const char *op, *fn; } F[] = {{"+", "fadd"}, {"-", "fsub"}, {"*", "fmul"}, {"/", "fdiv"}, {0, 0}};
                for (int i = 0; F[i].op; i++)
                    if (!strcmp(bop, F[i].op))
                        return mkt(fmt("(%s = %s(%s, %s))", l.rv, F[i].fn, l.rv, tof(r)), TY_FLOAT);
                die(line, "operator '%s' is not defined for floats", op);
            }
            if (rel) /* integer relational assignment: x = x OP y (x is evaluated twice) */
                return mkt(fmt("(%s = ((W)(%s %s %s)))", l.rv, l.rv, bop, r.rv), l.ty);
            if (!strcmp(op, "/=")) return mkt(fmt(l.isbyte ? "asg_div_b(&%s, %s)" : "asg_div(&%s, %s)", l.rv, r.rv), l.ty);
            if (!strcmp(op, "%=")) return mkt(fmt(l.isbyte ? "asg_mod_b(&%s, %s)" : "asg_mod(&%s, %s)", l.rv, r.rv), l.ty);
            if (!strcmp(op, "<<=")) return mkt(fmt(l.isbyte ? "asg_shl_b(&%s, %s)" : "asg_shl(&%s, %s)", l.rv, r.rv), l.ty);
            if (!strcmp(op, ">>=")) return mkt(fmt(l.isbyte ? "asg_shr_b(&%s, %s)" : "asg_shr(&%s, %s)", l.rv, r.rv), l.ty);
            return mkt(fmt("(%s %s %s)", l.rv, op, r.rv), l.ty);
        }
    }
    return l;
}
