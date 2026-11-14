main() {
  auto x, y;
  x = 10;
  if (x >= 5) {
    switch (x) {
      case 10:
        while (x > 1) {
          x--;
        }
        printf("should say 1: %d\n", x);
      break;
      case 6:
        y = 2;
      case 7:
        x = 5;
      break;
    }
  }
  y = 10;
}
