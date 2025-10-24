main() {
  // should pass
  auto *k, *m, z;
  extrn array;
  k = &array[2];
  z = *k;
  m = k;
}

array [3] 10, 20, 30;