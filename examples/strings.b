/* strings, char(), switch, old-style "=+" assignment */
main() {
  auto s, i, c, vowels;
  s = "hello, world*n";
  i = 0;
  vowels = 0;
  while ((c = char(s, i++)) != '*e') {
    putchar(c);
    switch (c) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
      vowels =+ 1;
    }
  }
  printf("vowels: %d*n", vowels);
}
