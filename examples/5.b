///////////////////////////////////////////////////////////////////////////
// Example with syscalls that are available on Linux and Darwin, and on
// both arm64 and x86-64.
//
// Example:
//
//   ./bin/credence -t arm64 -o syscalls examples/5.b
//   ./syscalls world
//
// The first argument after the program name is printed by greet, and the
// output is the process ids, the umask, and a write to a duplicate of
// stdout.
///////////////////////////////////////////////////////////////////////////

main(argc, argv) {
  auto pid, mask, fd, n;
  extrn banner, report;

  write(1, banner, 32);
  print("\n");
  rule(32);

  // the process and its parent
  pid = getpid();
  printf("pid %d, parent %d, uid %d\n", pid, getppid(), getuid());

  // umask returns the mask it replaces, so a second call restores it
  mask = umask(63);
  printf("umask was %d, then %d\n", mask, umask(mask));

  // a duplicate of stdout
  fd = dup(1);
  n = write(fd, report, 33);
  close(fd);
  printf("wrote %d bytes to descriptor %d\n", n, fd);

  if (argc > 1)
    greet(argv[1], argc - 1);

  rule(32);
}

// a line of dashes
rule(width) {
  auto i;
  i = 0;
  while (i < width) {
    write(1, "-", 1);
    i++;
  }
  write(1, "\n", 1);
}

greet(name, count) {
  printf("hello, %s, you passed %d argument%s\n", name, count, plural(count));
}

plural(count) {
  if (count == 1)
    return("");
  return("s");
}

banner "### credence syscall example ###\n";

report "written to a duplicate of stdout\n";
