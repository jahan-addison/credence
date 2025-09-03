main() {
  auto x, y;
  x = 5;
  switch (x) {
    case 0:
      y = 1;
      break;
    case 1:
      y = 2;
    case 2:
      x = 5;
    break;
  }
}
