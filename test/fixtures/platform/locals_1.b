main() {
  auto m, s, c;
  m = 10 % 3;
  s = "ab" == "ab";
  c = ' ';
  printf("%d %d %d %d %d\n", address(), many(), m, s, c);
}

address() {
  auto x, *p, y;
  x = 1;
  p = &y;
  y = 3;
  return(*p + x);
}

many() {
  auto a, b, c, d, e, g, h, i, j;
  a = 1;
  b = 2;
  c = 3;
  d = 4;
  e = 5;
  g = 6;
  h = 7;
  i = 8;
  j = 9;
  return(a + b + c + d + e + g + h + i + j);
}
