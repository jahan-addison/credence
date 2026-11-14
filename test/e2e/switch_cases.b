main() {
  auto x, n;
  n = 0;
  x = 2;
  switch (x) {
    case 1:
      n = 1;
      break;
    case 2:
      n = 2;
    case 3:
      n = n + 30;
      break;
    case 4:
      n = 4;
  }
  printf("%d\n", n);
  switch ((n = 4)) {
    case 4:
      printf("four\n");
      break;
  }
}
