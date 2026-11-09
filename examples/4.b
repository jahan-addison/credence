
union data_t {
    ab "",
    cd 0,
    km 0.0,
    ll 0.0f,
    list1 (0, "") // comma is optional
    list2 ("", "", 0.0f, 0, false, '\0')
};


main () {
  auto x[50], w, *y;
  extrn mess;
  union data_t m, k;
  // k.ll = 5.555f;
  // m.ab = "hello world";
  // y = m.ab;
  // if (m is data_t.ab) {
  //   printf("%s\n%s\n", m.ab, mess[5]);
  // }
  // if (k is data_t.ll) {
  //   printf("%f\n"m k.ll);
  // }
}

//data_t k;
