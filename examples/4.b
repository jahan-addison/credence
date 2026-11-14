union data_t {
  ab "",
  cd 0,
  km 0.0,
  ll 0.0f,
  list1 (0, "") // comma is optional
  list2 ("", "", 0.0f, 0, false, '\0')
};

union data_t k;

snide(errno) {
  auto t;
  extrn unit, mess;
  auto u;

  u = unit;
  unit = 1;
  t = mess[errno];
  printf("error number %d, %s\n", errno, t);

  unit = u;
}

str "puts";

unit 10;

main () {
  auto x[50], w, *y;
  extrn mess, k;
  union data_t m;
  k.ll = 5.555f;
  m.ab = "hello world";
  if (m is data_t.ab) {
    printf("%s\n%s\n", m.ab, mess[5]);
  }
  if (k is data_t.ll) {
    printf("%f\n", k.ll);
  }
}


mess [6] "too bad", "tough luck", "sorry, Charlie", "that's the breaks",
       "what a shame", "some days you can't win";