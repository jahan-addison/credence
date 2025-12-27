main() {
  auto x, y;
  extrn unit, mess;
  x = unit;
  // note: pointer decay
  y = mess[x];
}

unit 5;

mess [3] "too bad", "tough luck", "that sucks";