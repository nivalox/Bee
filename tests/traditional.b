/* traditional B, as in the spec: extrn globals, assignment, printf */
main() {
    extrn a, b, c;

    a = 10;
    b = 20;
    c = a + b;

    printf("%d*n", c);
    printf("%d %d*n", a, b);
}

a;
b;
c;
