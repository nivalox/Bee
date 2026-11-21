/* stmt.c - statement parser: B statements -> C statements */
#include "bee.h"

void stmt(Buf *o) {
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
