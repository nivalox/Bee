char gname[] = "Gilbert";
char ginit = 'G';
main() {
    char name[] = "Gilbert";
    char initial = 'G';
    message = "Hello, world!";
    printf("%s %c %s\n", name, initial, message);
    printf("%d %d\n", name[0] == initial, ginit == initial);
    message[0] = 'J';
    printf("%s\n", message);
    printf("a\tb\\c\"q\"\n");
    printf("%s|%5s|%-5s|%.3s*n", "ab", "ab", "ab", "abcdef");
    printf("%s %s\n", gname, name);
    char c = name[1];
    printf("%c %d\n", c, c);
}
