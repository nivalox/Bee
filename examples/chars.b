/* char arrays: C-style declarations, byte indexing, passing to functions */
char var[] = "hello, world!";

upper(char s[]) {
  auto i, c;
  i = 0;
  while ((c = s[i]) != '*e') {
    if (c >= 'a' & c <= 'z') s[i] = c - 32;
    i++;
  }
}

main() {
  char name[] = "bee";
  printf("%s*n", var);
  var[0] = 'H';
  upper(name);
  printf("%s %s*n", var, name);
}
