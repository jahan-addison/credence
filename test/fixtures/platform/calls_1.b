main() {
  auto fd, n;
  fd = dup(1);
  n = write(fd, "dup\n", 4);
  close(fd);
  printf("%d %d %s\n", n, fd > 2, plural(2));
  printf("%d\n", keep(41, "kept"));
}

keep(a, *s) {
  nothing();
  a = a + 1;
  printf("%d %s\n", a, s);
  return(a);
}

nothing() {
  return(0);
}

plural(count) {
  if (count == 1)
    return("");
  return("s");
}
