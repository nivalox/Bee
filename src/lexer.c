/* lexer.c - source text -> tokens (B "*" escapes plus Bee backslash escapes) */
#include "bee.h"

#include <ctype.h>

Tok *toks;
int ntoks;
static int captoks;
int modern; /* -m: do not read "=+" style assignment operators */
int strict; /* -s: B escapes only; a backslash is an ordinary character */

static void addtok(Tok t) {
    if (ntoks == captoks) {
        captoks = captoks ? captoks * 2 : 256;
        toks = realloc(toks, (size_t)captoks * sizeof *toks);
        if (!toks) oom();
    }
    toks[ntoks++] = t;
}

/* B escapes start with '*', not '\' */
static int escape(const char *s, int *i, int line) {
    int c = (unsigned char)s[*i];
    if (!c) die(line, "unterminated literal");
    (*i)++;
    switch (c) {
    case 'n': return 10;
    case 't': return 9;
    case 'r': return 13;
    case '0': return 0;
    case 'e': return 4; /* EOT, the string terminator */
    case '(': return '{';
    case ')': return '}';
    default: return c; /* ** *' *" */
    }
}

/* Bee extension: C-style backslash escapes (\n \t ...) alongside B's '*' escapes.
 * An unrecognised sequence keeps its backslash, so old B programs that used a
 * literal '\' still mean what they did. */
static int bs_escape(const char *s, int *i) {
    switch (s[*i]) {
    case 'n': (*i)++; return 10;
    case 't': (*i)++; return 9;
    case 'r': (*i)++; return 13;
    case '0': (*i)++; return 0;
    case 'e': (*i)++; return 4;
    case '\\': (*i)++; return '\\';
    case '\'': (*i)++; return '\'';
    case '"': (*i)++; return '"';
    default: return '\\';
    }
}

void lex(const char *s) {
    int line = 1;
    int i = 0;
    for (;;) {
        int c = (unsigned char)s[i];
        if (!c) break;
        if (c == '\n') { line++; i++; continue; }
        if (isspace(c)) { i++; continue; }
        if (c == '/' && s[i + 1] == '/') { /* modern single-line comment */
            while (s[i] && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && s[i + 1] == '*') {
            i += 2;
            while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) {
                if (s[i] == '\n') line++;
                i++;
            }
            if (!s[i]) die(line, "unterminated comment");
            i += 2;
            continue;
        }
        Tok t;
        memset(&t, 0, sizeof t);
        t.line = line;
        if (isalpha(c) || c == '_') {
            int j = i;
            while (isalnum((unsigned char)s[j]) || s[j] == '_') j++;
            t.k = T_ID;
            t.s = dupn(s + i, (size_t)(j - i));
            i = j;
        } else if (isdigit(c) || (c == '.' && isdigit((unsigned char)s[i + 1]))) {
            int j = i;
            int hex = c == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X');
            if (!hex) { /* 3.14  .5  2e3  1.5e-3 are floats; plain digits stay integers */
                int k = i, isf = 0;
                while (isdigit((unsigned char)s[k])) k++;
                if (s[k] == '.' && isdigit((unsigned char)s[k + 1])) {
                    isf = 1;
                    k++;
                    while (isdigit((unsigned char)s[k])) k++;
                }
                if ((s[k] == 'e' || s[k] == 'E') &&
                    (isdigit((unsigned char)s[k + 1]) ||
                     ((s[k + 1] == '+' || s[k + 1] == '-') && isdigit((unsigned char)s[k + 2])))) {
                    isf = 1;
                    k += 2;
                    while (isdigit((unsigned char)s[k])) k++;
                }
                if (isf) {
                    if (isalpha((unsigned char)s[k]) || s[k] == '_') die(line, "bad number '%s'", dupn(s + i, (size_t)(k - i + 1)));
                    char *txt = dupn(s + i, (size_t)(k - i));
                    t.k = T_FLT;
                    t.d = strtod(txt, NULL);
                    t.s = txt;
                    i = k;
                    addtok(t);
                    continue;
                }
            }
            while (isalnum((unsigned char)s[j])) j++;
            char *txt = dupn(s + i, (size_t)(j - i)), *end;
            t.k = T_NUM;
            t.v = strtoull(txt, &end, 0); /* 0755 octal, 0x1f hex, else decimal */
            if (*end) die(line, "bad number '%s'", txt);
            i = j;
        } else if (c == '\'' || c == '"') {
            int q = c;
            Buf b = {0};
            i++;
            for (;;) {
                int d = (unsigned char)s[i];
                if (!d || d == '\n') die(line, "unterminated %s", q == '"' ? "string" : "character constant");
                i++;
                if (d == q) break;
                if (d == '*') d = escape(s, &i, line);
                else if (d == '\\' && !strict) d = bs_escape(s, &i);
                char ch = (char)d;
                bputn(&b, &ch, 1);
            }
            t.k = q == '"' ? T_STR : T_CHR;
            t.s = b.p;
            t.len = (int)b.n;
        } else {
            char op[4] = {0};
            if (c == '=' && !modern) {
                /* Original B spells compound assignment "=+", "=-", "=<<" ... and has six
                 * relational ones: "===", "=!=", "=<", "=<=", "=>", "=>=" (x =< y means
                 * x = x < y). Those six keep their historical spelling as tokens. */
                if (s[i + 1] == '=' && s[i + 2] == '=') { strcpy(op, "==="); i += 3; }
                else if (s[i + 1] == '!' && s[i + 2] == '=') { strcpy(op, "=!="); i += 3; }
                else if (s[i + 1] == '<' && s[i + 2] == '=') { strcpy(op, "=<="); i += 3; }
                else if (s[i + 1] == '>' && s[i + 2] == '=') { strcpy(op, "=>="); i += 3; }
                else if (s[i + 1] == '<' && s[i + 2] == '<') { strcpy(op, "<<="); i += 3; }
                else if (s[i + 1] == '>' && s[i + 2] == '>') { strcpy(op, ">>="); i += 3; }
                else if (s[i + 1] == '<') { strcpy(op, "=<"); i += 2; }
                else if (s[i + 1] == '>') { strcpy(op, "=>"); i += 2; }
                else if (s[i + 1] && s[i + 1] != '=' && strchr("+-*/%&|", s[i + 1])) {
                    op[0] = s[i + 1];
                    op[1] = '=';
                    /* x=-1 is x =- 1 (x = x - 1) in B. Almost certainly not what a modern
                     * programmer meant, but it is the historical meaning, so only warn. */
                    int nx = (unsigned char)s[i + 2];
                    if ((op[0] == '-' || op[0] == '+') && (isalnum(nx) || nx == '_' || nx == '('))
                        warn(line, "'=%c' is the historical B operator (x =%c y means x = x %c y); write 'x = %c1' with a space to assign a signed number", op[0], op[0], op[0], op[0]);
                    i += 2;
                }
            }
            if (!op[0]) {
                static const char *m3[] = {"<<=", ">>=", 0};
                static const char *m2[] = {"==", "!=", "<=", ">=", "<<", ">>", "++", "--", "&&", "||",
                                           "+=", "-=", "*=", "/=", "%=", "&=", "|=", 0};
                int found = 0;
                for (int k = 0; m3[k] && !found; k++)
                    if (!strncmp(s + i, m3[k], 3)) { strcpy(op, m3[k]); i += 3; found = 1; }
                for (int k = 0; m2[k] && !found; k++)
                    if (!strncmp(s + i, m2[k], 2)) { strcpy(op, m2[k]); i += 2; found = 1; }
                if (!found) {
                    if (!strchr("(){}[];,:?+-*/%&|<>!=", c)) {
                        if (c == '.') die(line, "unexpected '.': Bee has no structures or member access");
                        die(line, "unexpected character '%c'", c);
                    }
                    op[0] = (char)c;
                    i++;
                }
            }
            t.k = T_OP;
            t.s = dupn(op, strlen(op));
        }
        addtok(t);
    }
    Tok e;
    memset(&e, 0, sizeof e);
    e.k = T_EOF;
    e.line = line;
    addtok(e);
    addtok(e); /* second EOF so one-token lookahead is always in range */
}
