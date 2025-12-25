main() {
  auto x, y;
  x = 5;
  while (x) {
    y = x;
    while (!y) {
      while (x > y) {
        x = y;
          while (!x) {
            x = y;
            while(y) {
              y = x;
            }
          }
      }
    }
  }
}
