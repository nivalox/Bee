main() {
    auto i, j, n, sum, product, fib, temp;

    n = 12;
    sum = 0;
    product = 1;

    printf("=== BEE COMPILER STRESS TEST ===\n");

    printf("\n--- Multiplication Table ---\n");

    i = 1;
    while (i <= 10) {
        j = 1;
        while (j <= 10) {
            printf("%4d", i * j);
            j = j + 1;
        }
        printf("\n");
        i = i + 1;
    }

    printf("\n--- Factorials ---\n");

    i = 1;
    while (i <= n) {
        product = product * i;
        printf("%d! = %d\n", i, product);
        i = i + 1;
    }

    printf("\n--- Fibonacci ---\n");

    i = 0;
    j = 1;

    while (i <= 1000) {
        printf("%d ", i);
        temp = i + j;
        i = j;
        j = temp;
    }

    printf("\n\n--- Arithmetic ---\n");

    i = 1;
    while (i <= 100) {
        sum = sum + i;
        i = i + 1;
    }

    printf("Sum of 1-100: %d\n", sum);

    printf("\n--- Conditional Tests ---\n");

    i = 42;

    if (i > 100)
        printf("Large\n");
    else
        if (i > 50)
            printf("Medium\n");
        else
            printf("Small\n");

    printf("\n=== TEST COMPLETE ===\n");
}
