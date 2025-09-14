main() {
  auto x, y;
  x = 1;
  y = 10;
  while(x >= 0) {
    x--;
    x = y--;
  }
  while (x <= 100) {
    x++;
  }
  x = 2;
}
