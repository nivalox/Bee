hello = "Gilbert";     // inferred globals
age = 67;
number = 3.14;

main() {
    greeting = "Hi";   // inferred locals
    n = 5;
    ratio = 0.5;
    ch = 'x';
    n = n + 1;
    ratio = ratio * 4;
    ratio += 1;
    {
        inner = 9;
        printf("%d\n", inner);
    }
    printf("%s %s %d %.2f\n", hello, greeting, age, number);
    printf("%d %.1f %c\n", n, ratio, ch);
    age = age + 1;
    number = number * 2;
    printf("%d %.2f\n", age, number);
    hello = "Other";
    printf("%s %c\n", hello, hello[0]);
    inner = 100;       // the block's 'inner' is gone, so this is a new variable
    printf("%d\n", inner);
}
