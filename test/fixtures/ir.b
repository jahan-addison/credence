main() {
  auto x, y, z;
  x = 5;
  y = 1;
  z = add(x, sub(x, y)) - 2;
  if (x > y) {
    while(z > x) {
      // z--; bugged (if no temp stack just push)
      z = z - 1;
    }
  }
  x = 0;
}

add(x,y) {
  return(x + y);
}

sub(x,y) {
  return(x - y);
}
