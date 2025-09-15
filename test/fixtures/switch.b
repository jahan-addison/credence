main() {
  auto x, y;
  x = 5;
  switch (x) {
    case 0:
      y = 1;
      break;
    case 1:
      y = 2;
    case 2:
      x = 5;
    break;
  }
  y = 10;
}

/**
x = 5;
_t1 = CMP x;
IF_Z _t1 0 _L1;
IF_Z _t1 1 _L2;
IF_Z _t1 2 _L3;
_L4:
y = 10;
LEAVE
_L1:
y = 1;
GOTO _L4;
_L2:
y = 2;
_L3:
x = 5;
GOTO _L4;
**/