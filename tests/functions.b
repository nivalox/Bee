// main comes first: functions can be used before they are defined
main() {
    printf("%d %d\n", add(2, 3), mul(4, 5));
    printf("%.2f\n", half(5));
    printf("%.1f\n", avg(1, 4));
    printf("%d %d\n", sq(add(1, 2)), mul(add(1, 1), 3));
    printf("%c %d\n", first("xyz"), len("hello"));
    printf("%d\n", fact(5));
    printf("%d\n", oldstyle(6));
}

add(a, b) { return a + b; }                       // traditional
int mul(int a, int b) { return a * b; }           // modern
int sq(int n) { return n * n; }
float half(int n) { return n / 2.0; }
float avg(float a, float b) { return (a + b) / 2; }
char first(char s[]) { return s[0]; }
int len(char s[]) {
    int n = 0;
    while (s[n] != '*e') n++;
    return n;
}
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
oldstyle(n) {
    auto r;
    r = 1;
    while (n > 0) { r =* n; n--; }
    return(r);
}
