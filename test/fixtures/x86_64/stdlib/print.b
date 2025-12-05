main() {
  auto x, y;
  extrn unit, mess;
  x = unit;
  y = mess[x];
  print(y);
  print(mess[1]);
}

unit 0;

mess [2] "hello", "world";
