main() {
  auto x, y;
  x = (1 + 1) * (2 + 2);
  y = add(x, 5);
  if (x > y) {
    x = 100;
  }
  //y = x;
}

add(x, y) {
  return(x + y);
}