main() {
  auto x, y;
  x = (5 + 5) * (3 + 3);
  y = 100;
  if (x > 0) {
    while(x >= 0) {
      x--;
      x = y--;
    }
  } else {
    x++;
    y = 5;
    x = (x + y) * (x + x);
  }
  x = 2;
}
