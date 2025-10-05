main() {
  auto x, y;
  x = 10;
  if (x >= 5) {
    switch (x) {
      case 5:
        while (x > 1) {
          x--;
        }
        y = 8;
        x = 2;
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
