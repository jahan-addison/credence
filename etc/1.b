/* The following complete B program, if compiled and put on your
  file "hstar", will act as an ascii file copy routine; the command
  at "SYSTEM?" level:

   /hstar file1 file2

  will copy file1 to file2. */

main () {
   auto j,s[20],t[20];
   reread(); /* get command line */
   getstr(s); /* put into s */
   j = getarg(t,s,0); /* skip H* name */
   j = getarg(t,s,j); /* filel */
   openr( 5,t );
   getarg(t,s,j); /* file2 */
   openw( 6,t );
   while( putchar( getchar() ) != '*e' ) ; /* copy contents */
   }


/* This function is called with a string s of the form nnn, nnn,
   nnn, . . .  , where the nnn are integers.  The values are placed
   in successive locations in a vector v.  The number of integers
   converted is returned as a function value.  This program
   provides a simple illustration of the switch and case state-
   ments. */

convert(s,v) {

   auto m,i,j,c,sign;

   i = O; /* vector index '/
   j =-1; /* character index */

init: /* initialize to convert an integer */
   m = 0; /* the integer value '/
   sign = 0; /* sign = 1 if the integer is negative */

loop: /* convert an integer */

   switch (C = char(s,++j)){

   case '-':
      if(sign) goto syntax;
     s = 1;

   case ' ':
      goto loop;

   case  '*e':
   case ',':  /* delimiter . . . store converted value */

      v[i++] = sign?(-m):m;
      if( c == '*e' ) return(i);
      goto init;
      }

/* none of the above cases . . . if a digit, add to m */

   if ( '0' <= c & c <= '9' ){
      m = 10*m + c- '0';
      goto loop;
      }

/* syntax error . . . print message and return -1 */

syntax:
   printf("bad syntax*n");
   return(-1 );
   }
