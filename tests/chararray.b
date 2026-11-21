/* char arrays: byte indexing, %s, initialisers, locals, char parameters */
char g[] = "hello, world!";
char pad[8];
char big[20] = "hi";
char a[] = "ab", b[3] = "xy";

shout(char s[]) {
  auto i;
  i = 0;
  while (s[i] != '*e') { s[i] = s[i] - 32; i++; }
}

count(s) { /* a plain parameter: use char()/lchar() */
  auto n;
  n = 0;
  while (char(s, n) != '*e') n++;
  return(n);
}

main() {
  char loc[] = "local", buf[16];
  auto i;

  printf("%s|%s|%s|%s*n", g, big, a, b);
  g[0] = 'H';
  g[7] = 'W';
  printf("%s*n", g);

  i = 0;
  while (g[i] != '*e') { putchar(g[i]); i++; }
  putchar('*n');

  g[1] += 1;          /* e -> f */
  g[2]++;             /* l -> m */
  --g[3];             /* l -> k */
  printf("%s*n", g);

  buf[0] = 'o'; buf[1] = 'k'; buf[2] = '*e';
  printf("%s %d*n", buf, count(buf));
  pad[0] = 300;       /* a byte keeps the low 8 bits */
  printf("%d*n", pad[0]);
  printf("%d %d*n", g[12], big[2]);   /* '!' and the *e terminator */

  shout(loc);
  printf("%s %d*n", loc, count(loc));
  big[1] = '*e';
  printf("[%s] %d*n", big, count(big));
  while (i > 0) { i--; if (i == 5) { char t[] = "in a loop"; printf("%s*n", t); } }
}
