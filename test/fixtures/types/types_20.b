main(argc, argv) {
  // should pass
  // "m" is never assigned before the call, which credence does not
  // report, since it does not track whether a name has been written to
  auto k, *m, *z;
  test(m);
}

test(*x) {
  return(x);
}