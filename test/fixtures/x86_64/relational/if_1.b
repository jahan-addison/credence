main() {
  auto x;
  x = 10;
  if (x <= 5) {
    x = 1;
    printf("less than or equal to %d\n", 5);
  }
  if (x == 10) {
    printf("equal to %d\n", 10);
  }
  if (x >= 5) {
    printf("greater than or equal to %d\n", 5);
  }
  if (x != 5) {
    printf("not equal to %d\n", 5);
  }
  if (x > 8) {
    printf("greater than %d\n", 8);
  }
  if (x < 20) {
    printf("less than %d\n", 20);
  }

  print("done!");

  x = 5 || 2;

}
