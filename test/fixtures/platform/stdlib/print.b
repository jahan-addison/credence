main() {
  auto x, y;
  extrn unit, mess;
  x = unit;
  y = mess[x];

  print("hello world\n");
  print(y);
  print(mess[1]);
  // syscall
  write(1, "how cool is this man\n", 21);
}

unit 0;

mess [2] "hello ", "world\n";

// /print() {}
//write() {}