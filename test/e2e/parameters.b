convert(s, n) {
  s = s + n;
  nothing();
  n = 1;
  return(s + n);
}

nothing() {
  return(0);
}

main() {
  printf("%d\n", convert(10, 5));
}
