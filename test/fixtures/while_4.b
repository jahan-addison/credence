main() {
  auto x, y;
  x = 100;
  y = 100;
  if (x == 100) {
    if (x > 5) {
      while(x >= 50) {
        x--;
        y = x - 1;
      }
    }
  }
  x++;
  y = 5;
  printf("%d", x);
  printf("%d", y);
}