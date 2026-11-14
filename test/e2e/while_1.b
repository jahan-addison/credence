main() {
  auto x, y;
  x = 100;
  y = 4;
  if (x > 50) {
    while(x >= 50) {
      x--;
      y = x - 1;
    }
  }

  if (x == 48) {
    print("no\n");
  } else {
    print("yes!\n");
  }

  printf("x, y: %d %d\n", x, y);
}