main() {
  auto x, y, z;
  x = ~10;
  y = 5;
  z = (x ^ y) | y >> 5;
  z = ~x & ~y;
}