main(argc, argv) {
  auto x;
  x = argc;
  printf("%d\n", x);
  show(argv[1]);
}

show(*s) {
  printf("%s\n", s);
}
