/* The following complete B program, if compiled and put on your
  file "hstar", will act as an ascii file copy routine; the command
  at "SYSTEM?" level:

   /hstar file1 file2

  will copy file1 to file2. */

// TODO : error handling ...

main () {
   auto j,s[20],t[20];
   reread();
   getstr(s);
   j = getarg(t,s,0);
   j = getarg(t,s,j);
   openr( 5,t );
   getarg(t,s,j);
   openw( 6,t );
   while( putchar( getchar() ) != 'e' ) ;
   }

// TODO: STL functions
reread() {}
getstr() {}
getarg() {}
getchar() {}
putchar() {}
openr() {}
openw() {}

char(a,b) {
}

printf(s) {
   return(s);
}

clear(a,b) {
   a = 0;
   b = 0;
}

error() {
   printf("bad syntax*n");
   return(-1);
}

/* This function is called with a string s of the form nnn, nnn,
   nnn, . . .  , where the nnn are integers.  The values are placed
   in successive locations in a vector v.  The number of integers
   converted is returned as a function value.  This program
   provides a simple illustration of the switch and case state-
   ments. */

convert(s,v) {

   auto m,i,j,c,sign,C;
   auto loop;
   i = 0; /* vector index */
   j = 1; /* character index */

   m = 0; /* the integer value */
   sign = 0; /* sign = 1 if the integer is negative */
   loop = 1;
   while(loop == 1) {
      switch (C = char(s,++j)) { // evaluated expression disappeared
         case '-':
            if (sign) {
               loop = 0;
               error();
            }
         s = 1;
         case ' ':
            break;
         case '0':
         case ',':  /* delimiter . . . store converted value */
            //v[i++] = sign?(-m):m;
            if( c == '0' ) return(i);
      }
      /* none of the above cases . . . if a digit, add to m */
      if ( '0' <= c && c <= '9' ){
         m = 10*m + c - '0';
         }
   }


}
