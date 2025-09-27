main() {
  auto x;
  x = exp(2,5);
}

exp(x,y) {
  if (x == 1 || y == 1)
    return(x * y);
  return(exp(x - 1, y - 1));
}
