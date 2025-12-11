main() {
  auto *x;
  x = "hello, how are you";
  print(identity(identity(identity(x))));
}

identity(*y) {
  return(y);
}
