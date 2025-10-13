main() {
   auto m,i,j,c,sign,C,s;
   auto loop;
   i = 0; /* vector index */
   j = 1; /* character index */
   m = 0; /* the integer value */
   sign = 0; /* sign = 1 if the integer is negative */
   loop = 1;
   while(loop == 1) {
      switch ((C = char(s,++j))) {
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

char(a,b) {
}

error() {
   auto m;
   printf("bad syntax*n");
   return(-1);
}

printf(s) {
   return(s);
}

