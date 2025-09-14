main() {
  auto x, y;
  x = 100;
  y = 100;
  if (x > 5) {
    while(x >= 0) {
      x--;
      x = y--;
    }
  }
  x++;
  y = 5;
  x = (x + y) * (x + x);
}