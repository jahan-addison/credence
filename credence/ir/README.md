# Intermediate Representation

The intermediate representation (IR) is formalized as a linear four-tuple, named the Instruction Tuple Abstraction (ITA). The ITA constitutes a collection of platform-independent, generic instructions that approximate the structure and semantics of a target machine language.

Its construction is governed by two interacting stacks: an operand stack and a temporary stack. The operand stack is derived from an [r-value queue](https://github.com/jahan-addison/credence/blob/master/credence/queue.cc) associated with a block scope of expressions, ordered according to operator precedence. The temporary stack serves to decouple mutual recursion, enabling data types to be encoded within a three- or four-tuple framework. The detailed algorithm for temporary construction is provided [here](https://github.com/jahan-addison/credence/blob/edc637f9d41fa4a52a49351f273d8203030b559c/credence/ir/temp.cc#L521).


## Instructions

The instruction set:

```C++
    enum class Instruction
    {
        FUNC_START,
        FUNC_END,
        LABEL,
        GOTO,
        LOCL,
        GLOBL,
        IF,
        JMP_E,
        PUSH,
        POP,
        CALL,
        CMP,
        MOV,
        RETURN,
        LEAVE,
        NOOP
    };
```

## Labels

#### _L{integer}

Local scoped labels:

* **Note**: `_L1` is reserved for the root function scope

```asm
_L2:
    _t5 = x > y;
    IF _t5 GOTO _L4;
_L3:
    x = (0:int:4);
_L1:
    LEAVE;
_L4:
    x = x - 1;
    GOTO _L2;
```


#### __{string}

Symbolic labels that are global scope and added in the symbol table:

* **Note** `__main` is the reserved main function

```asm
__main():
 BeginFunc ;
    x = (5:int:4);
    y = (1:int:4);
    LEAVE;
 EndFunc ;
```

## Branching

See the branch state machine object for details [here](https://github.com/jahan-addison/credence/blob/99d882fb813fe6964092f7ed7ac2f07b30c86cf8/credence/ir/ita.h#L369).


## Table

The `Table` serves as a pre-selection pass that constructs tables for functions, labels, and stack frames along with their corresponding local allocations. During this stage, it also performs vector management, out-of-range boundary checks, and type validation. The resulting data structures provide a foundation for generating type- and data-size-safe platform-specific machine code.


## Example:

B Code:

```C
main() {
  auto *a;
  auto c, i, j;
  extrn unit;
  c = unit;
  a = &c;
  i = 1;
  j = add(c, sub(c, i)) - 2;
  if (c > i) {
    while(j > i) {
      j--;
    }
  }
  c = 0;
}

str(i) {
  extrn mess;
  return(mess[i]);
}

add(x,y) {
  return(x + y);
}

sub(x,y) {
  return(x - y);
}

unit 10;

mess [3] "too bad", "tough luck", "that's the breaks";

```

ITA:


```asm
__main():
 BeginFunc ;
    LOCL *a;
    LOCL c;
    LOCL i;
    LOCL j;
    GLOBL unit;
    c = unit;
    _t2 = & c;
    a = _t2;
    i = (1:int:4);
    _p1 = c;
    _p3 = c;
    _p4 = i;
    PUSH _p4;
    PUSH _p3;
    CALL sub;
    POP 16;
    _t3 = RET;
    _p2 = _t3;
    PUSH _p2;
    PUSH _p1;
    CALL add;
    POP 16;
    _t4 = RET;
    _t5 = _t4;
    j = (2:int:4) - _t5;
_L6:
    _t9 = c > i;
    IF _t9 GOTO _L8;
_L7:
    c = (0:int:4);
_L1:
    LEAVE;
_L8:
_L10:
_L12:
    _t13 = j > i;
    IF _t13 GOTO _L11;
    GOTO _L7;
_L11:
    j = --j;
    GOTO _L10;
 EndFunc ;


__str(i):
 BeginFunc ;
    GLOBL mess;
    RET mess[i] ;
_L1:
    LEAVE;
 EndFunc ;


__add(x,y):
 BeginFunc ;
    _t2 = x + y;
    RET _t2;
_L1:
    LEAVE;
 EndFunc ;


__sub(x,y):
 BeginFunc ;
    _t2 = x - y;
    RET _t2;
_L1:
    LEAVE;
 EndFunc ;


```