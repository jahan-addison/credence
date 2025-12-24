main () {
  auto x[50], *y;
  extrn str, unit;
  y = str;
  x[unit] = 0;
  printf("%s %d", y, x[unit]);
}

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


mess [6] "too bad", "tough luck", "sorry, Charlie", "that's the breaks",
       "what a shame", "some days you can't win";