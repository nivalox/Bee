/* character constants hold a full word: up to 8 chars on every target */
main() {
  extrn a, b, c;
  putchar(a); putchar(b); putchar(c); putchar('!*n');
  putchar('12345678'); putchar('*n');
  putchar('abcdefg\n');
  printf("%c|%-9c|%9c|*n", 'abcdefgh', 'xy', 'xy');
  printf("%x*n", 'abcdefgh');
}
a 'hello, ';
b 'world, h';
c 'ello 8!!';
