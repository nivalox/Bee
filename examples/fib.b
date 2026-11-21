/* recursion, printf, and a vector */
main() {
  auto i, v[9];
  i = 0;
  while (i < 10) {
    v[i] = fib(i);
    printf("fib(%d) = %d*n", i, v[i]);
    i++;
  }
}

fib(n) {
  if (n < 2) return(n);
  return(fib(n-1) + fib(n-2));
}
