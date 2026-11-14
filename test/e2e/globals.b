main() {
  extrn count, name, total;
  auto x;
  printf("%d\n", count);
  count = 20;
  printf("%d\n", count);
  x = count;
  count = x + 1;
  printf("%d\n", count);
  name = "brocoli";
  printf("%s\n", name);
  total = 3;
  bump();
  printf("%d\n", total);
}

bump() {
  extrn total;
  auto t;
  t = total;
  total = t + 1;
}

count 10;
name;
total;
