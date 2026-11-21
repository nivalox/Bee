main() {
    auto i, j;
    printf("[%4d][%-4d][%04d][%4d]*n", 7, 7, 7, 12345);
    printf("[%04d][%4d][%-4d]*n", -7, -7, -7);
    printf("[%6s][%-6s][%2s]*n", "ab", "ab", "abcd");
    printf("[%3c][%-3c]*n", 'x', 'y');
    printf("[%4x][%04o]*n", 255, 8);
    i = 1;
    while (i <= 3) {
        j = 1;
        while (j <= 3) { printf("%4d", i * j); j++; }
        printf("\n");
        i++;
    }
}
