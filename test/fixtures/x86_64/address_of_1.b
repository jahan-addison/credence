main() {
  auto *x, *z, k;
  extrn strings;
  k = 5;
  x = &k;
  z = &strings[1];
  print(z);
}


strings [3] "one", "two", "three";