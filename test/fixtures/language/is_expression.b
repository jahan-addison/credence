union data_t { ab "", list (0, "") };

main() {
  auto n, *s;
  union data_t m;
  if (m is data_t.ab) {
    n = 1;
  }
  if (m is data_t.list(n, s) && n > 0) {
    n = 2;
  }
}
