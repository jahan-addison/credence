main(argc, argv) {
  auto *x;
  extrn strings;
  x = "hello, how are you, %s\n";
  // using the provided standard library printf function in stdlib/
  if (argc > 1) {
    printf(identity(identity(identity(x))), argv[1]);
    // stdlib print in stdlib/
    print(strings[0]);
  }
}

identity(*y) {
  return(y);
}

strings [3] "good afternoon", "good morning", "good evening";