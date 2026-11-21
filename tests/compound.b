main() {
  int x = 10;
  x += 5;  printf("%d\n", x);
  x -= 3;  printf("%d\n", x);
  x *= 2;  printf("%d\n", x);
  x /= 5;  printf("%d\n", x);
  x %= 3;  printf("%d\n", x);
  x <<= 4; printf("%d\n", x);
  x >>= 2; printf("%d\n", x);
  x |= 3;  printf("%d\n", x);
  x &= 5;  printf("%d\n", x);
  x = -7;  printf("%d\n", x);
  float f = 1.5;
  f += 1; f -= 0.25; f *= 4; f /= 2;
  printf("%.2f\n", f);
  char s[] = "abc";
  s[0] += 1; s[1]++; --s[2];
  printf("%s\n", s);
  y = 3;
  y += 4;
  printf("%d\n", y);
  x = y = 2;
  printf("%d %d\n", x, y);
}
