set(*p, v) {
  *p = v;
}

main() {
  auto x, *p, v[3];
  x = 1;
  p = &x;
  *p = 5;
  printf("%d\n", x);
  set(&x, 9);
  printf("%d\n", x);
  v[0] = 10;
  v[1] = 20;
  v[2] = 30;
  p = &v[1];
  printf("%d\n", *p);
  p++;
  printf("%d\n", *p);
}
