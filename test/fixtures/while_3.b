main() {
  auto x, y;
  x = (5 + 5) * (3 + 3);
  y = 100;
  while(x >= 0) {
    x--;
    x = y--;
  }
  x++;
  y = 5;
  x = (x + y) * (x + x);
}