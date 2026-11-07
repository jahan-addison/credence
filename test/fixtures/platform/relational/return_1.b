main() {
  auto x, y, z;
  y = 2;
  z = 3;
  x = y + z;
  x = z;
  if (x == 3) {
    printf("returned early %d", x);
    return(0);
  }
  printf("did not return");
}
