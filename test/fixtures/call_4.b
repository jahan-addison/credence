main() {
  auto x,y,z;
  x = 1;
  y = 2;
  if (y < x) {
    z = add(x, sub(x, y)) - 2;
  }
}


add(x,y) {
  return(x + y);
}

sub(x,y) {
  return(x - y);
}

