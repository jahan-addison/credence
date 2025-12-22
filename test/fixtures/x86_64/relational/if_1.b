main() {
  auto x;
  x = 10;
  if (x <= 5) {
    x = 1;
    print("less than or equal to 5\n");
  }
  if (x == 10) {
    print("equal to 10\n");
  }
  if (x >= 5) {
    print("greater than or equal to 5\n");
  }
  if (x != 5) {
    print("not equal to 5\n");
  }
  if (x > 8) {
    print("greater than 8\n");
  }
  if (x < 20) {
    print("less than 20\n");
  }
  x = 5 || 2;
}
