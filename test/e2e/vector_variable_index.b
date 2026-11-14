main() {
  auto v[5], i, s;
  i = 0;
  while (i < 5) {
    v[i] = i * i;
    i = i + 1;
  }
  s = 0;
  i = 0;
  while (i < 5) {
    s = s + v[i];
    i = i + 1;
  }
  printf("%d\n", s);
}
