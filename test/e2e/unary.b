main() {
  auto x, y;
  x = 5;
  y = x++;
  printf("%d %d\n", x, y);
  y = ++x;
  printf("%d %d\n", x, y);
  y = x--;
  printf("%d %d\n", x, y);
  y = --x;
  printf("%d %d\n", x, y);
  printf("%d %d %d\n", !x, !0, ~x);
  printf("%d\n", -x);
}
