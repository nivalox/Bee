// all_features.b - one self-checking program that exercises every Bee feature.
//
// Each check() compares a value with what it must be. A broken check prints a FAIL line
// (so the output stops matching all_features.exp); the last line is a summary.
// Traditional B and modern syntax are deliberately mixed throughout.

/* ---- check helpers (typed globals, traditional-looking functions) ---- */
int passed = 0;
int failed = 0;

check(name, got, want) {
    if (got == want) passed++;
    else {
        failed++;
        printf("FAIL %s: got %d, wanted %d*n", name, got, want);
    }
}

checkf(char name[], float got, float want) {
    float d = got - want;
    if (d < 0) d = -d;
    if (d < 0.000001) passed++;
    else {
        failed++;
        printf("FAIL %s: got %f, wanted %f*n", name, got, want);
    }
}

checks(name, got, want) { /* strings, compared character by character */
    auto i;
    i = 0;
    while (char(got, i) == char(want, i)) {
        if (char(got, i) == '*e') { passed++; return; }
        i++;
    }
    failed++;
    printf("FAIL %s: got \"%s\", wanted \"%s\"*n", name, got, want);
}

section(name) { printf("== %s ==*n", name); }

/* ---- traditional globals, defined after their use ---- */
tg1 42;
tg2;
tgs "hello";
tgv[3] 10, 20, 30;
fnglobal 0;

/* ---- modern and inferred globals ---- */
int gint = 7;
char gchar = 'Q';
float gfloat = 2.5;
char garr[] = "global";
int gzero;
ginf = 67;
gname = "Gilbert";
gpi = 3.14;

main() {
    section("traditional B");   t_traditional();
    section("historical ops");  t_histops();
    section("control flow");    t_control();
    section("functions");       t_funcs();
    section("declarations");    t_decls();
    section("inference");       t_infer();
    section("compound");        t_compound();
    section("floats");          t_floats();
    section("strings");         t_strings();
    section("escapes");         t_escapes();
    section("printf");          t_printf();
    section("mixed syntax");    t_mixed();
    section("comments");        t_comments();
    printf("%d checks, %d passed, %d failed*n", passed + failed, passed, failed);
    if (failed == 0) printf("ALL PASSED*n");
}

/* ------------------------------------------------------------------------------- */
t_traditional() {
    extrn tg1, tg2, tgs, tgv, implicitg;
    auto a, b, p, v[4];

    a = 10;
    b = 3;
    check("add", a + b, 13);
    check("sub", a - b, 7);
    check("mul", a * b, 30);
    check("div", a / b, 3);
    check("mod", a % b, 1);
    check("neg", -a, -10);
    check("not0", !0, 1);
    check("not5", !5, 0);
    check("shl", 1 << 4, 16);
    check("shr", 256 >> 4, 16);
    check("and", 12 & 10, 8);
    check("or", 12 | 10, 14);
    check("lt", 1 < 2, 1);
    check("le", 2 <= 2, 1);
    check("gt", 1 > 2, 0);
    check("ge", 3 >= 4, 0);
    check("eq", 5 == 5, 1);
    check("ne", 5 != 5, 0);
    check("land", 1 && 0, 0);
    check("lor", 1 || 0, 1);
    check("cond", a > b ? 100 : 200, 100);
    check("cond nested", a < b ? 100 : a == 10 ? 7 : 8, 7);
    check("hex", 0x1f, 31);
    check("octal", 010, 8);
    check("char const", 'A', 65);
    check("packed const", 'ab', 97 + 98 * 256);

    check("global", tg1, 42);
    check("global zero", tg2, 0);
    check("global vector", tgv[2], 30);
    check("global string", char(tgs, 1), 'e');
    check("implicit extrn zero", implicitg, 0);
    implicitg = 5;
    check("implicit extrn set", implicitg, 5);

    check("pre-inc", ++a, 11);
    check("post-inc", a++, 11);
    check("after post-inc", a, 12);
    check("pre-dec", --a, 11);
    check("post-dec", a--, 11);
    check("after post-dec", a, 10);

    v[0] = 1; v[1] = 2; v[3] = 4;
    check("vector", v[0] + v[1] + v[3], 7);
    check("a[b] == b[a]", v[1], 1[v]);

    p = &a;
    *p = 77;
    check("address-of / store through", a, 77);
    check("deref", *p, 77);
    a = b = 9;
    check("chained assignment", a + b, 18);
}

t_histops() {
    auto x, y, v[2];
    x = 10;
    x =+ 5;   check("=+", x, 15);
    x =- 3;   check("=-", x, 12);
    x =* 2;   check("=**", x, 24);
    x =/ 4;   check("=/", x, 6);
    x =% 4;   check("=%", x, 2);
    x =<< 3;  check("=<<", x, 16);
    x =>> 1;  check("=>>", x, 8);
    x =| 3;   check("=|", x, 11);
    x =& 6;   check("=&", x, 2);
    y = 5; y =< 3;   check("=<", y, 0);
    y = 5; y =<= 5;  check("=<=", y, 1);
    y = 5; y => 3;   check("=>", y, 1);
    y = 5; y =>= 6;  check("=>=", y, 0);
    y = 5; y === 5;  check("===", y, 1);
    y = 5; y =!= 5;  check("=!=", y, 0);
    v[1] = 4; v[1] =* 3; v[1] =- 2;
    check("=op on element", v[1], 10);
}

sw(n) {
    switch (n) {
    case 1: return(10);
    case 2: return(20);
    default: return(99);
    }
    return(0);
}

sw2(n) {
    auto s;
    s = 0;
    switch (n) {
    case 5: s++;      /* falls through */
    case 6: s++; break;
    case 7: s = 100;
    }
    return(s);
}

fact(n) { if (n <= 1) return(1); return(n * fact(n - 1)); }

t_control() {
    auto i, s, r;

    i = 0; s = 0;
    while (i < 5) { s =+ i; i++; }
    check("while", s, 10);

    i = 0; s = 0;
    while (1) {
        i++;
        if (i > 10) break;
        if (i % 2) continue;
        s =+ i;
    }
    check("break / continue", s, 30);

    if (s == 30) r = 1; else r = 2;
    check("if", r, 1);
    if (s == 31) r = 1; else if (s == 30) r = 3; else r = 4;
    check("else if", r, 3);

    check("switch case 1", sw(1), 10);
    check("switch case 2", sw(2), 20);
    check("switch default", sw(7), 99);
    check("switch fallthrough", sw2(5), 2);
    check("switch break", sw2(6), 1);
    check("switch last case", sw2(7), 100);
    check("switch no match", sw2(8), 0);

    i = 0;
top:
    i++;
    if (i < 3) goto top;
    check("goto / label", i, 3);

    check("recursion", fact(6), 720);

    i = 0; s = 0;
    while (i < 3) {                 /* a block local is recreated each pass */
        cube = i * i;
        s =+ cube;
        i++;
    }
    check("inferred inside loop", s, 5);
}

/* ------------------------------------------------------------------------------- */
add(a, b) { return a + b; }                        // traditional
int mul(int a, int b) { return a * b; }            // modern
int sq(int n) { return n * n; }
float half(int n) { return n / 2.0; }
float avg(float a, float b) { return (a + b) / 2; }
char first(char s[]) { return s[0]; }
int len(char s[]) {
    int n = 0;
    while (s[n] != '*e') n++;
    return n;
}
mixedargs(int a, b) { return a * 100 + b; }        // one typed, one untyped parameter
noreturn(a) { return; }

t_funcs() {
    auto f, g;

    check("traditional fn", add(2, 3), 5);
    check("modern fn", mul(4, 5), 20);
    check("nested calls", sq(add(1, 2)), 9);
    checkf("float return", half(5), 2.5);
    checkf("int args widen", avg(1, 4), 2.5);
    check("char return", first("xyz"), 'x');
    check("string param", len("hello"), 5);
    check("mixed typed/untyped params", mixedargs(3, 4), 304);
    check("bare return", noreturn(5), 0);
    check("forward reference", late(6), 36);

    f = add;                        /* function values */
    check("function value", f(7, 8), 15);
    g = mul;
    check("typed function value", g(7, 8), 56);
    check("function value in vector", callthrough(add, 1, 2), 3);
}

callthrough(fn, a, b) { return(fn(a, b)); }
late(n) { return n * n; }

/* ------------------------------------------------------------------------------- */
t_decls() {
    int i1 = 16;
    char c1 = 'B';
    float f1 = 3.5;
    int z;
    float zf;
    char zc;
    int a = 1, b = 2, c;
    int n = 'A';
    char d = 66;
    float fi = 3;
    int x = 1;

    check("int", i1, 16);
    check("char", c1, 'B');
    checkf("float", f1, 3.5);
    check("zeroed int", z, 0);
    checkf("zeroed float", zf, 0.0);
    check("zeroed char", zc, 0);
    check("several at once", a * 10 + b * 100 + c, 210);
    check("char into int", n, 65);
    check("int into char", d, 'B');
    checkf("int widens to float", fi, 3.0);

    {
        int x = 2;                  /* shadows the outer x */
        check("shadow inner", x, 2);
    }
    check("shadow outer", x, 1);

    check("global int", gint, 7);
    check("global char", gchar, 'Q');
    checkf("global float", gfloat, 2.5);
    checks("global char array", garr, "global");
    check("global zero", gzero, 0);
    gint = gint + 1;
    check("global changes", gint, 8);
}

t_infer() {
    istr = "Gilbert";
    iint = 67;
    iflt = 3.14;
    ichr = 'x';
    checks("inferred string", istr, "Gilbert");
    check("inferred int", iint, 67);
    checkf("inferred float", iflt, 3.14);
    check("inferred char", ichr, 'x');

    iint = iint + 1;
    iflt = iflt * 2;
    istr = "Other";
    check("inferred int updates", iint, 68);
    checkf("inferred float updates", iflt, 6.28);
    checks("inferred string repoints", istr, "Other");

    wv = add(1, 2);                 /* untyped value: an untyped variable */
    check("inferred word", wv, 3);
    wv = 99;
    check("word takes anything", wv, 99);

    {
        inner = 9;
        check("block variable", inner, 9);
    }
    inner = "reused";               /* the block's inner is gone: a new variable */
    checks("name reuse after block", inner, "reused");

    check("inferred global int", ginf, 67);
    checks("inferred global string", gname, "Gilbert");
    checkf("inferred global float", gpi, 3.14);
}

t_compound() {
    int x = 10;
    float f = 1.5;
    char s[] = "abc";
    auto v[2];

    x += 5;   check("+=", x, 15);
    x -= 3;   check("-=", x, 12);
    x *= 2;   check("**=", x, 24);
    x /= 5;   check("/=", x, 4);
    x %= 3;   check("%=", x, 1);
    x <<= 4;  check("<<=", x, 16);
    x >>= 2;  check(">>=", x, 4);
    x |= 3;   check("|=", x, 7);
    x &= 5;   check("&=", x, 5);
    x = -7;   check("negative literal", x, -7);

    f += 1; f -= 0.25; f *= 4; f /= 2;
    checkf("float compound", f, 4.5);

    s[0] += 1; s[1]++; --s[2];
    checks("char element compound", s, "bcb");
    s[0] <<= 1;  check("<<= on element", s[0], 196);
    s[0] /= 4;   check("/= on element", s[0], 49);

    v[1] = 3; v[1] += 4;
    check("vector element +=", v[1], 7);

    k = 3;
    k += 4;
    check("compound on inferred", k, 7);
    x = k = 2;
    check("chained modern", x + k, 4);
}

t_floats() {
    float a = 7.5, b = 2.0;
    checkf("add", a + b, 9.5);
    checkf("sub", a - b, 5.5);
    checkf("mul", a * b, 15.0);
    checkf("div", a / b, 3.75);
    check("gt", a > b, 1);
    check("lt", a < b, 0);
    check("ge", a >= 7.5, 1);
    check("le", a <= 7.4, 0);
    check("eq", a == 7.5, 1);
    check("ne", a != b, 1);
    checkf("unary minus", -a, -7.5);
    checkf("int + float", 1 + 2.5, 3.5);
    checkf("float * int", a * 2, 15.0);
    checkf("scientific", 1e3 / 8, 125.0);
    checkf("small exponent", 1.5e-3 * 2, 0.003);
    checkf("leading dot", .5 + .25, 0.75);
    checkf("ternary mix", a ? 1.5 : 2, 1.5);
    check("float truth", a ? 1 : 0, 1);
    z = 0.0;
    check("float zero is false", !z, 1);
    check("float &&", a && b, 1);
    check("float ||", z || 0, 0);
    x = 3 * 1.5;                    /* inferred float */
    checkf("inferred float arithmetic", x, 4.5);
    checkf("division is IEEE", 1.0 / 4, 0.25);
    big = 1e300 * 1e300;            /* inf, not an error */
    check("overflow is inf", big > 1e308, 1);
}

t_strings() {
    char name[] = "Gilbert";
    char buf[8];
    char line[16] = "hi";
    char initial = 'G';
    message = "Hello, world!";
    auto i;

    checks("char array", name, "Gilbert");
    check("index", name[0], 'G');
    check("compare to char", name[0] == initial, 1);
    check("terminator", name[7], 4);
    check("sized array", line[1], 'i');
    check("sized array terminator", line[2], 4);
    check("local array zeroed", buf[0] + buf[7], 0);
    name[0] = 'g';
    checks("assign element", name, "gilbert");
    buf[0] = 'o'; buf[1] = 'k'; buf[2] = '*e';
    checks("build in buffer", buf, "ok");
    buf[0] = 300;
    check("byte keeps low 8 bits", buf[0], 44);
    buf[0] = -1;
    check("negative becomes 255", buf[0], 255);

    checks("inferred string", message, "Hello, world!");
    message[0] = 'J';
    checks("string literal writable", message, "Jello, world!");
    message = "Other";
    checks("string variable repoints", message, "Other");

    check("char()", char(name, 1), 'i');
    lchar(name, 1, 'o');
    checks("lchar()", name, "golbert");

    check("len of literal", len("twelve chars"), 12);
    check("len of array", len(name), 7);
    check("string param to typed fn", first(name), 'g');

    i = 0;
    while (name[i] != '*e') { name[i] = name[i] - 32; i++; }
    checks("upper-case in a loop", name, "GOLBERT");

    check("global array element", garr[0], 'g');
    garr[0] = 'G';
    checks("global array writable", garr, "Global");
    check("string == itself", message == message, 1);
}

t_escapes() {
    check("star-n", char("a*nb", 1), 10);
    check("star-t", char("a*tb", 1), 9);
    check("star-0", char("a*0b", 1), 0);
    check("star-e", char("a*eb", 1), 4);
    check("star-star", char("a**b", 1), '**');
    check("star-apostrophe", char("a*'b", 1), 39);
    check("star-quote", char("a*"b", 1), 34);
    check("star-open-brace", char("a*(b", 1), 123);
    check("star-close-brace", char("a*)b", 1), 125);
    check("backslash-n", char("a\nb", 1), 10);
    check("backslash-t", char("a\tb", 1), 9);
    check("backslash-backslash", char("a\\b", 1), 92);
    check("backslash-quote", char("a\"b", 1), 34);
    check("backslash-apostrophe", char("a\'b", 1), 39);
    check("backslash-0", char("a\0b", 1), 0);
    check("unknown backslash keeps it", char("a\qb", 1), 92);
    check("escape in char const", '*n', 10);
    check("backslash escape in char const", '\n', 10);
    check("packed escape", 'a\n', 97 + 10 * 256);
}

t_printf() {
    putchar('>');
    printf("%d|%5d|%-5d|%05d*n", 42, 42, 42, 42);
    printf("%d %d*n", -7, 0);
    printf("%o %x*n", 64, 255);
    printf("%c%c%c*n", 'a', 'b', 'c');
    printf("%s|%6s|%-6s|%.2s*n", "bee", "bee", "bee", "bee");
    printf("%f %.2f %8.3f|%-8.1f|*n", 1.5, 2.0 / 3, 3.14159, 2.5);
    printf("%e %g %g*n", 1234.5, 0.25, 100000.0);
    printf("100%% sure*n");
    printf("%4d%4d%4d*n", 1, 22, 333);
    printf("%s has %d items costing %.2f*n", "cart", 3, 9.5);
    printn(255, 10); putchar(' ');
    printn(255, 8); putchar(' ');
    printn(7, 2); putchar(' ');
    putnumb(12345);
    putchar('*n');
    putchar('!'); putchar('*n');
    printf("%c*n", 'xy');
    printf("no args*n");
}

/* ------------------------------------------------------------------------------- */
t_mixed() {
    extrn oldValue;                 /* never defined at top level: a zeroed global */
    auto trad;

    oldValue = 10;
    modernValue = "Bee";
    count = 20;
    int result = oldValue + count;  // typed local initialised from untyped + typed
    trad = result;
    trad =+ 5;                      /* historical operator on an untyped word */
    result += 5;                    // modern operator on a typed int

    check("mixed arithmetic", result, 35);
    check("both styles agree", trad, result);
    checks("mixed string", modernValue, "Bee");
    check("typed fn from untyped caller", mul(oldValue, 2), 20);
    check("untyped fn from typed code", add(result, 5), 40);
    checkf("float fn on mixed values", half(result), 17.5);
}

t_comments() {
    int x = 1; // line comment
    /* block comment */ x = 2;
    /* multi
       line
       comment */
    check("comments are skipped", x, 2);
    check("// inside a string", char("a//b", 1), '/');
    check("slash-star inside a string", char("a/**b", 1), '/');
    check("// at end of statement", 5 // + 100
                                    , 5);
}
