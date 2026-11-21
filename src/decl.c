/* decl.c - top level: function definitions and global variables/vectors */
#include "bee.h"

char *ival(void) {
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

void toplevel(void) {
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
