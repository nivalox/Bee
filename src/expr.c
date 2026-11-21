/* expr.c - expression parser: B expressions -> C expressions */
#include "bee.h"

/* rv:   C expression for the value (for an lvalue, the C lvalue itself)
 * addr: C expression for the word address, non-NULL only for lvalues
 * fn:   name, if this is a bare function name (so a call can be direct) */

static Ex mk(char *rv) { Ex e = {rv, NULL, NULL}; return e; }
static Ex lval(char *rv, char *addr) { Ex e = {rv, addr, NULL}; return e; }

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

Ex expr(void) {
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
