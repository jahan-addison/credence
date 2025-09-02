main () {
  // https://godbolt.org/z/ahsv6dPWE
  auto x, y;
  x = 5 < 3 ? 1 : 10;
  // x = 5 < 4 ? 10: 1;
}

add(x,y) {
  return(x + y);
}

/*
 _t2 = 5 < 4;
 IF _t2 GOTO _L3;
 GOTO _L4;
_L1:
 LEAVE;
_L3:
 x = 10;
_L4:
 x = 1;
*/

/*
condition && true_value || false_value
_t2 = 5 < 4;
_t3 = _t2 && 10
_t4 = _t3 || 1
*/

/*

"x (5:int:4) (4:int:4) (10:int:4) (1:int:4) ?: < = ";

x = 5 < 4 ? false : true;
_t2 = 5 < 4;
PUSH _t2;
x = 10 ?: 1;
POP 4;
*/