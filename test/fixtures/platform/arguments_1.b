main() {
  extrn counter, mess;
  printf("%d %s %d\n", counter, mess[1], swap(10, 3));
  show("a b", "a b" == "a b");
}

swap(a, b) {
  return(sub(b, a));
}

sub(x, y) {
  return(x - y);
}

show(s, equal) {
  printf("%s %d\n", s, equal);
}

counter 5;
mess [2] "too bad", "tough luck";
