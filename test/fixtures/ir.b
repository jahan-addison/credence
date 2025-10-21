main() {
  auto x, y, z;
  extrn unit;
  x = unit;
  y = 1;
  z = add(x, sub(x, y)) - 2;
  if (x > y) {
    while(z > x) {
      z--;
    }
  }
  x = 0;
}

str(i) {
  extrn mess;
  return(mess[i]);
}

add(x,y) {
  return(x + y);
}

sub(x,y) {
  return(x - y);
}

unit 10;

mess [3] "too bad", "tough luck", "that's the breaks";
