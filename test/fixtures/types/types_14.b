main() {
  // should fail
  auto *k, *m, z;
  extrn array;
  k = &array[2];
  m = *k;
}

array [3] 10, 20, 30;