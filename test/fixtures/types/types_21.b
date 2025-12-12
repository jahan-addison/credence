main(argc, argv) {
  // should pass
  auto *m;
  *m = 5;
  test(m);
}

test(*x) {
  return(x);
}