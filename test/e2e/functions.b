fact(n) {
  if (n <= 1)
    return(1);
  return(n * fact(n - 1));
}

sum6(a, b, c, d, e, f) {
  return(a + b + c + d + e + f);
}

main() {
  printf("%d\n", fact(5));
  printf("%d\n", sum6(1, 2, 3, 4, 5, 6));
}
