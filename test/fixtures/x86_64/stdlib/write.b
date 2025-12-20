main() {
  auto x, *y;
  extrn unit, mess;
  x = unit;
  y = mess[x];
  write(1, y, 6);
  write(1, mess[1], 6);
  write(1, "how cool is this man\n", 21);
}

unit 0;

mess [2] "hello ", "world\n";
