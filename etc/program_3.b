/* This function prints out an unsympathetic error message on the
terminal for each integer value of errno from O to 5 */

snide(errno) {
   extrn wr.unit, mess;
   auto u; /* temporary storage for the unit number */

   u = wr.unit ; wr.unit = 1;

   printf("error number %d, %s*n'*,errno,mess[errno]");

   wr.unit = u;
   }

mess [5] "too bad", "tough luck", "sorry, Charlie", "that's the breaks",
       "what a shame", "some days you can't win";