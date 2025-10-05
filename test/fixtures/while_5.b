main() {
  auto x, y;
  x = 5;
  while (x) {
    y = x;
    while (!y) {
      while (x > y) {
        x = y;
          while (!x) {
            x = y;
            while(y) {
              y = x;
            }
          }
      }
    }
  }
}

/*
__main:
 BeginFunc ;
    x = (5:int:4);
    _t4 = CMP x;
    IF _t4 GOTO _L3;
_L1:
    LEAVE;
_L3:
    y = x;
    _t7 = ! y;
    IF _t7 GOTO _L6;
    GOTO _L1;
_L6:
    _t10 = x > y;
    IF _t10 GOTO _L9;
    GOTO _L3;
_L9:
    x = y;
    _t13 = ! x;
    IF _t13 GOTO _L12;
    GOTO _L6;
_L12:
    x = y;
    GOTO _L9;
 EndFunc ;
*/