main () {
  auto m,c,k;
  m = 1;
  c = 5;
  // k = m || c;
  m = 10 * m + c - 0;
}

// --------------------------------------------

/*
__main():
 BeginFunc ;
    LOCL m;
    LOCL c;
    m = (1:int:4);
    c = (5:int:4);
    _t2 = c - (0:int:4);
    _t3 = m + _t2;
    _t4 = (10:int:4) * _t3;
    m = _t4;
_L1:
    LEAVE;
 EndFunc ;
*/

// --------------------------------------------

/*
mov m, 1
mov c, 5
mov eax, c
sub eax, 0
add m, eax
imul m, 10
*/