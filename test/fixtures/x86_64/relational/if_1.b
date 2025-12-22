main() {
  auto x;
  x = 10;
  if (x <= 5) {
    x = 1;
    print("less than or equal to 5");
  }
  if (x >= 5) {
    x = 10;
    print("greater than or equal to 5");
  }
  x = 5 || 2;
}
