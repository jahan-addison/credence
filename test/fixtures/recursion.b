main() {
  // x exp exp (2:int:4) (5:int:4) PUSH PUSH CALL PUSH CALL (2:int:4) PUSH =
  // x exp exp (2:int:4) (5:int:4) PUSH PUSH CALL PUSH (2:int:4) PUSH CALL =
  // callee1 exp (2:int:4) (5:int:4) PUSH PUSH CALL = x exp callee1 (2:int:4) PUSH PUSH CALL =
  auto x, y;
  //auto callee1;
  //callee1 = exp(2, 5);
  x = exp(exp(2,5), sub(1,2));
  //x = exp(callee1, 2);
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