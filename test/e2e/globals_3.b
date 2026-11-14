main() {
  auto x, y;
  extrn unit, mess;
  x = unit;
  y = mess[x];
  print(y);
}

unit 1;

mess [3] "too bad", "tough luck", "that sucks";