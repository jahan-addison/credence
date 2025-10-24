main() {
  // should pass
  auto *k,m,*z;
  extrn nmbrs;
  extrn strs;
  k = &nmbrs[0];
  m = &strs[2];
  z = m;
  m = k;
  k = z;
}

nmbrs [3] 10, 20, 30;

strs [3] "hello" "there" "hi";