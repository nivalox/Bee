/* Kernighan's hello world from the B tutorial (1973).
   B has no strings: text is packed into words, one word per 'hell' chunk. */
main() {
  extrn a, b, c;
  putchar(a); putchar(b); putchar(c); putchar('!*n');
}

a 'hell';
b 'o, w';
c 'orld';
