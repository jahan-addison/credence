# Intermediate Representation

The intermediate representation (IR) is formalized as a linear four-tuple, named the Instruction Tuple Abstraction (ITA). The ITA constitutes a collection of platform-independent, generic instructions that approximate the structure and semantics of a target machine language.

Its construction is governed by two interacting stacks: an operand stack and a temporary stack. The operand stack is derived from an [r-value queue](https://github.com/jahan-addison/credence/blob/master/credence/queue.cc) associated with a block scope of expressions, ordered according to operator precedence. The temporary stack serves to decouple mutual recursion, enabling data types to be encoded within a three- or four-tuple framework. The detailed algorithm for temporary construction is provided [here](https://github.com/jahan-addison/credence/blob/master/credence/ir/temp.cc).


## Example:

B Code:

```C
main() {
  auto x, y, z;
  x = 5;
  y = 1;
  z = add(x, y) * sub(x,y);
  while(z > x) {
    z = z - 1;
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
    PUSH y;
    PUSH x;
    CALL add;
    POP 16;
    _t1 = RET;
    PUSH y;
    PUSH x;
    CALL sub;
    POP 16;
    _t2 = RET;
    _t3 = _t1 * _t2;
    z = _t3;
    _t6 = z > x;
_L4:
    IF _t6 GOTO _L5;
    GOTO _L1;
_L1:
    x = (0:int:4);
_L8:
    LEAVE;
_L5:
    _t7 = z - (1:int:4);
    z = _t7;
    GOTO _L4;
 EndFunc ;
__add:
 BeginFunc ;
    _t1 = x + y;
    RET _t1;
    LEAVE;
 EndFunc ;
__sub:
 BeginFunc ;
    _t1 = x - y;
    RET _t1;
    LEAVE;
 EndFunc ;
```
