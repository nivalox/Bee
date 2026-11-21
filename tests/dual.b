// Bee: traditional B and modern syntax in one file.

main() {
    extrn greeting;            /* traditional: declared extrn, defined below */
    auto i;                    /* traditional: untyped word */

    printf("%s*n", greeting);

    i = 1;
    i =+ 4;                    /* traditional compound assignment */
    int j = i * 2;             // modern typed declaration
    j += 1;                    // modern compound assignment
    name = "Gilbert";          // inferred string
    ratio = j / 4.0;           // inferred float
    printf("%s: i=%d j=%d ratio=%.2f\n", name, i, j, ratio);

    printf("%d squared is %d, half of it is %.1f\n", 7, square(7), half(square(7)));
}

greeting "hello from the old world";

square(n) { return n * n; }                  // traditional function
float half(int n) { return n / 2.0; }        // modern function
