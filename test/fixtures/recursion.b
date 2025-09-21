main() {
  // x exp exp (2:int:4) (5:int:4) PUSH PUSH CALL PUSH CALL (2:int:4) PUSH =
  auto x, y;
  x = 1;
  x = exp(exp(x - 1), 2);
}

exp(x,y) {
  auto x,y;
  x * y;
}

sub(x) {
  if (x == 0) {
    return(x);
  }
  return(sub(x - 1));
}