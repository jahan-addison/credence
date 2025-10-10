# Intermediate Representation

The intermediate representation (IR) is formalized as a linear four-tuple, named the Instruction Tuple Abstraction (ITA). The ITA constitutes a collection of platform-independent, generic instructions that approximate the structure and semantics of a target machine language.

Its construction is governed by two interacting stacks: an operand stack and a temporary stack. The operand stack is derived from an [r-value queue](https://github.com/jahan-addison/credence/blob/master/credence/queue.cc) associated with a block scope of expressions, ordered according to operator precedence. The temporary stack serves to decouple mutual recursion, enabling data types to be encoded within a three- or four-tuple framework. The detailed algorithm for temporary construction is provided [here](https://github.com/jahan-addison/credence/blob/edc637f9d41fa4a52a49351f273d8203030b559c/credence/ir/temp.cc#L521).


## Example:

B Code:

```C
main() {
  auto x, y, z;
  x = 5;
  y = 1;
  z = add(x, sub(x, y)) - 2;
  if (x > y) {
    while(z > x) {
      z--;
    }
  }
  x = 0;
}

add(x,y) {
  return(x + y);
}

sub(x,y) {
  return(x - y);
}

```

ITA:

```asm
__main:
 BeginFunc ;
    x = (5:int:4);
    y = (1:int:4);
    _p1 = x;
    _p3 = x;
    _p4 = y;
    PUSH _p4;
    PUSH _p3;
    CALL sub;
    POP 16;
    _t2 = RET;
    _p2 = _t2;
    PUSH _p2;
    PUSH _p1;
    CALL add;
    POP 16;
    _t3 = RET;
    _t4 = _t3;
    z = (2:int:4) - _t4;
_L5:
    _t8 = x > y;
    IF _t8 GOTO _L7;
_L6:
    x = (0:int:4);
_L1:
    LEAVE;
_L7:
_L9:
    _t12 = z > x;
    IF _t12 GOTO _L10;
    GOTO _L6;
_L10:
    z = --z;
    GOTO _L9;
 EndFunc ;
__add:
 BeginFunc ;
    _t2 = x + y;
    RET _t2;
_L1:
    LEAVE;
 EndFunc ;
__sub:
 BeginFunc ;
    _t2 = x - y;
    RET _t2;
_L1:
    LEAVE;
 EndFunc ;

```
