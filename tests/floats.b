main() {
    float a = 7.5, b = 2.0;
    printf("%.2f %.2f %.2f %.2f\n", a + b, a - b, a * b, a / b);
    printf("%d %d %d %d\n", a > b, a < b, a == 7.5, a != b);
    printf("%.1f %.3f\n", -a, 1e3 / 8);
    printf("%8.3f|%-8.1f|%08.2f\n", 3.14159, 2.5, -1.5);
    printf("%e\n", 12345.678);
    printf("%g %g\n", 0.5, 100000.0);
    x = 3 * 1.5;
    printf("%.1f\n", x);
    if (a) printf("true\n");
    z = 0.0;
    if (!z) printf("zero\n");
    printf("%.2f\n", 1 + 2.5);
    printf("%.1f\n", a ? 1.5 : 2);
    printf("%f\n", 1.0 / 3);
}
