main() {
  auto x, y;
  x = 5;
  if (x) {
    y = x;
    if (!y) {
      if (x > y) {
        x = y;
      }
    }
  }
}
