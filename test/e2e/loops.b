main() {
  auto i, j, n;
  i = 0;
  n = 0;
  while (i < 3) {
    j = 0;
    while (j < 4) {
      n = n + 1;
      j = j + 1;
    }
    i = i + 1;
  }
  printf("%d\n", n);
}
