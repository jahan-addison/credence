main() {
  auto *x;
  extrn strings;
  x = "hello, how are you";
  print(identity(identity(identity(x))));
  print(strings[0]);
}

identity(*y) {
  return(y);
}

strings [3] "these are strings", "in an array", "for the readme";