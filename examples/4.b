union data_t {
    ab "",
    cd 0,
    km 0.0,
    ll 0.0f,
    list (0, ""),
    list2 ("", "")
};

main () {
  auto x[50], w, *y;
  extrn mess, k;
  data_t m;
  k.ll = 5.555f;
  m.ab = "hello world";
  y = m.ab;
  if (m is data_t.ab) {
    printf("%s\n%s\n", m.ab, mess[5]);
  }
  if (k is data_t.ll) {
    printf("%f\n"m k.ll);
  }
}

data_t k;

unit 10;


mess [6] "too bad", "tough luck", "sorry, Charlie", "that's the breaks",
       "what a shame", "some days you can't win";