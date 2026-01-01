main() {
  // note that arrays are treated like tuples, and
  // may hold any data type, including pointers
  auto m[5];
  m[0] = 2;
  m[1] = "hello world";
  print(m[1]);
}