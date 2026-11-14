main() {
  auto a, b;
  a = 3;
  b = 5;
  printf("%d %d %d\n", a < b, a > b, a <= 3);
  printf("%d %d %d\n", a >= 4, a == 3, a != 3);
  printf("%d %d\n", a < b && b < 10, a > b || b == 5);
}
