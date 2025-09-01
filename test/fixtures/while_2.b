main() {
  auto x, y;
  x = (5 + 5) * (3 + 3);
  y = 100;
  while(x >= 0) {
    x--;
    x = y--;
  }
  while (x <= 100) {
    x++;
  }
  x = 2;
}
