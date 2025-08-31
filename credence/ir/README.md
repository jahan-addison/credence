# Intermediate Representation

The currently available IR is a linear 4-tuple ive named a "Qaudruple". it is a generic platform-agnostic set of instructions that closely matches a target machine language.

The construction uses two stacks, an operand stack and "temporary" stack. The operand stack comes from an [rvalue queue](https://github.com/jahan-addison/credence/blob/master/credence/queue.cc) of a block scope of expressions, ordered by operator precedence. A "temporary" breaks up mutual-recursion such that data types may fit on a 3- or 4- tuple. The temporary construction algorithm may be found [here](https://github.com/jahan-addison/credence/blob/master/credence/ir/temp.cc).

## Examples:

```B
__main:
 BeginFunc ;
_t1 = (5:int:4) + (5:int:4);
_t2 = (3:int:4) + (3:int:4);
_t3 = _t1 * _t2;
x = _t3;
_t4 = x <= (10:int:4);
IF _t4 GOTO _L5;
GOTO _L6;
_L5:
x = (1:int:4);
_L6:
x = (8:int:4);
 EndFunc ;
 ```


```B
__main:
 BeginFunc ;
PUSH (6:int:4);
CALL exp;
POP 8;
_t1 = RET;
_t2 = (5:int:4) || _t1;
_t3 = ~ (2:int:4);
_t4 = _t2 || _t3;
x = _t4;
 EndFunc ;
__exp:
 BeginFunc ;
RET a ;
 EndFunc ;
 ```