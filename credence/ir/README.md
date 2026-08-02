# Intermediate Representation

The intermediate representation (IR) is formalized as a linear four-tuple, named the Instruction Tuple Abstraction (ITA). The ITA comprises a collection of platform-independent  instructions that approximate the structure and semantics of a target machine language.

`ITA` walks the definitions of the `hir::Unit` the frontend returns, statement by statement. For a statement holding an expression, `hir_to_ita_instructions` first gets a queue back from `queue_from_hir` (see [the frontend](/credence/frontend/README.md) for that part) with the operands in reverse polish form - the HIR is already in post-order with precedence resolved, so the queue is the subtree read front to back. From there it's constructed by two stacks, an operand stack and a temporary stack: the operand stack is derived from that queue, and the temporary stack serves to decouple operands, enabling data types to be encoded within a three- or four-tuple framework. The detailed algorithm for temporary stack construction is provided [here](https://github.com/jahan-addison/credence/blob/d9eb0ce3dafc5606a32eff7cf457e3ed985ea650/credence/ir/temporary.h#L68).

```mermaid
flowchart LR
    A(hir::Unit statement) -->|queue_from_hir| B(queue)
    B -->|Temporary, operand + temporary stacks| C(Quadruples)
    C -->|ir::Table| D(Object)
    D -->|checker.h| E[Type-checked ITA]

    style A fill:#2d2d2d,stroke:#888,color:#fff
    style B fill:#2d2d2d,stroke:#888,color:#fff
    style C fill:#2d2d2d,stroke:#888,color:#fff
    style D fill:#2d2d2d,stroke:#888,color:#fff
    style E fill:#2d2d2d,stroke:#888,color:#fff
```

## Operands

An operand of a quadruple is an [`operand::Operand`](/credence/ir/operand.h) - either an lvalue, a literal, or nothing. It is what the IR prints as the `(value : type : size)` tuple seen throughout the examples below, and the type and size in that tuple come from the `Type_Index` the frontend assigned the node.

The hoisted symbol table in [`symbols.h`](/credence/ir/symbols.h) is built from the same unit and gives the object table and the backends the shape of every name - `function_definition`, `vector_definition`, `lvalue`, `indirect_lvalue`, `vector_lvalue`, or `label` - along with the size of a vector.

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
_L4:
    _t5 = x > y;
    IF _t5 GOTO _L3;
    x = (0:int:4);
_L1:
    LEAVE;
_L3:
    _t6 = x - (1:int:4);
    x = _t6;
    GOTO _L2;
```


#### __{string}

Symbolic labels that are global scope and added in the symbol table:

* **Note** `__main` is the reserved main function

```asm
__main():
 BeginFunc ;
    LOCL x;
    LOCL y;
    x = (5:int:4);
    y = (1:int:4);
_L1:
    LEAVE;
 EndFunc ;
```

## Branching

See the branch state machine object for details [here](https://github.com/jahan-addison/credence/blob/d9eb0ce3dafc5606a32eff7cf457e3ed985ea650/credence/ir/ita.h#L216).


## Table

The `Table` constructs a set of data structures in a [table object](/credence/ir/object.h) with allocations of functions, labels, vectors, and stack frames from the IR. During this stage, it also performs type checking, vector memory management, and out-of-range boundary checks via the [type checker](/credence/ir/checker.h). The result provides a base for generating type- and size-safe platform-specific machine code.


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
    _p3_1 = c;
    _p5_3 = c;
    _p6_4 = i;
    PUSH _p6_4;
    PUSH _p5_3;
    CALL sub;
    POP 16;
    _t7 = RET;
    _p4_2 = _t7;
    PUSH _p4_2;
    PUSH _p3_1;
    CALL add;
    POP 16;
    _t8 = RET;
    _t9 = _t8;
    j = (2:int:4) - _t9;
_L10:
    _t13 = c > i;
    IF _t13 GOTO _L12;
_L11:
    c = (0:int:4);
_L1:
    LEAVE;
_L12:
_L14:
_L16:
    _t17 = j > i;
    IF _t17 GOTO _L15;
    GOTO _L11;
_L15:
    j = --j;
    GOTO _L14;
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