main() {
  auto x,y;
ADD:
  x = add(2,5);
  y = 10;
  goto ADD;
}

add(a,b) {
  // return(a + b);
  return(a + b + b);
}