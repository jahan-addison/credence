main() {
  extrn mess;
  auto v[3], x, i;
  v[0] = 10;
  v[1] = 20;
  v[2] = 30;
  x = v[1] + 1;
  printf("%d %d %d\n", v[0], x, v[2]);
  i = 1;
  printf("%s\n", mess[i]);
}

mess [3] "too bad", "tough luck", "sorry";
