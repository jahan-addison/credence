<div align="center">
  <img src="docs/images/credence-compiler-logo.png" width="800px" alt="credence"> </img>
</div>

<br>

> This project is dual-licensed under Apache License v2 and GPL v3, see details [here](#licensing).

---

* B Language grammar - [here](https://github.com/jahan-addison/chakram/blob/master/chakram/grammar.lark)
* Language reference - [here](https://www.nokia.com/bell-labs/about/dennis-m-ritchie/btut.pdf)

## [Blog Series](https://soliloq.uy/tag/credence/)

The compiler works in 3 stages:

* The Lexer, Parser first-pass built with an [LALR(1) grammar and parser generator](https://github.com/jahan-addison/chakram) in python that interfaces with C++ via `pybind11`
* An IR (intermediate representation) I've named [Instruction Tuple Abstraction or ITA](credence/ir/README.md) - a linear 4-tuple set of platform-agnostic instructions that represent program flow, scope, and type checking

* The target platforms - x86_64, and ARM64 for Linux and BSD (Darwin)

## Features

These features are not in the original B language specification.

* **Strongly typed** with type inference
  * Vectors (arrays) may be non-homogeneous, but their types are determined at compile time from their initial values, similarly to tuples
  * Uninitialized variables are set to an internal `null` type
* Compile-time out-of-range boundary checks on vectors and pointer arithmetic
* `GOTO` and labels are not supported, use control structures
* Float and double literal types (e.g. `5.55f`)
* Support for C++ style comments
* Logical and bitwise operators behave more like C
* Operator precedence resembles C
* `argc` and `argv` behave like C
* VSCode extension provided in `ext/`
* Switch statement condition must always be enclosed with `(` and `)`
* Binary operators may not be used directly after the `=` operator
* Constant literals must be exactly 1 byte

Note: currently, windows is not supported.

## Installation

Download via `git clone` then run the `bin/install.sh` script with `bash bin/install.sh`

## Usage

```
Credence :: B Language Compiler
Usage:
  Credence [OPTION...] positional parameters

  -a, --ast-loader arg   AST Loader [json, python] (default: python)
  -t, --target arg       Target [ir, syntax, ast, arm64, x86_64] (default:
                         ir)
  -d, --debug            Dump symbol table
  -o, --output arg       Output file (default: stdout)
  -h, --help             Print usage
      --source-code arg  B Source file
```

A complete assembler and linking tool is installed via the installation script.

---

## Targets

### x86-64:

#### Linux, BSD (Darwin)

### Done ✅

[Implementation details here](/credence/target/x86_64/readme.md).

#### Features

  * Compliance with the Application Binary Interface (ABI) for System V
  * SIMD memory alignment requirements

### ARM64:

#### Linux, BSD (Darwin)

Soon. ™️

---

## Test Suite

Credence has [80% line and function code coverage](https://jahan-addison.github.io/credence/) with exhaustive type checking, ast, and code generation test suites.

To build the test suite and coverage yourself (with `llvm-cov`):

```bash

cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DIWYU=OFF -DENABLE_TEST_COVERAGE=ON
cmake --build build
cmake --build build --target coverage

```

---

## Standard Library

#### [The Standard Library](https://github.com/jahan-addison/credence/blob/master/credence/target/x86_64/runtime.h#L44)

* The standard library object file is pre-compiled in `stdlib/` for each platform. **None of the standard library depends on libc or libc++**.

In addition, a function map for kernel syscall tables such as `write(3)` is available for each platform, with compile-time type safety.

* **Linux** x86_64
  * See details [here](https://github.com/jahan-addison/credence/blob/master/credence/target/x86_64/syscall.h#L58)
* **BSD** (Darwin) x86_64
  * See details [here](https://github.com/jahan-addison/credence/blob/master/credence/target/x86_64/syscall.h#L453)


---

## Example

```C
main(argc, argv) {
  auto *x;
  extrn strings;
  x = "Hello, how are you, %s!\n";
  // using the provided standard library printf function in stdlib/
  if (argc > 1) {
    printf(identity(identity(identity(x))), argv[1]);
    // stdlib print in stdlib/
    print(strings[0]);
  }
}

identity(*y) {
  return(y);
}

strings [3] "Good afternoon", "Good morning", "Good evening";
```
#### Result (x86-64, Linux):

<img src="docs/images/credence-example-2.png" width="700px" alt="example"> </img>


```asm

.intel_syntax noprefix

.data

._L_str1__:
    .asciz "good afternoon"

._L_str2__:
    .asciz "good evening"

._L_str3__:
    .asciz "good morning"

._L_str4__:
    .asciz "hello, how are you, %s\n"

strings:
    .quad ._L_str1__

    .quad ._L_str3__

    .quad ._L_str2__

.text
    .global _start
    .extern print
    .extern printf

_start:
    lea r15, [rsp]
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rcx, [rip + ._L_str4__]
    mov qword ptr [rbp - 8], rcx
._L2__main:
    mov rax, [r15]
    cmp rax, 1
    jg ._L4__main
._L3__main:
    jmp ._L1__main
._L4__main:
    mov rdi, qword ptr [rbp - 8]
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    call identity
    mov rdi, rax
    mov rsi, [r15 + 8 * 2]
    call printf
    mov rdi, qword ptr [rip + strings]
    mov esi, 14
    call print
    jmp ._L3__main
._L1__main:
    add rsp, 16
    mov rax, 33554433
    mov rdi, 0
    syscall


identity:
    push rbp
    mov rbp, rsp
    mov rax, rdi
    pop rbp
    ret

```

---

An example of compile-time boundary checking, if the `print(strings[0])` line is changed to `print(strings[10])` you get the following error:

<img src="docs/images/credence-out-of-range-2.png" width="700px" alt="error"> </img>



## Intermediate Representation

*Note*: The default compile target is currently the IR, [ITA](credence/ir/README.md):

### Example:

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

### Produces:


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

## Dependencies

**Note: These are installed automatically via CPM and cmake.**

* `chakram` - [LALR(1) parser generator and Lexer](https://github.com/jahan-addison/chakram)
* `easyjson` - [Lightweight memory safe json library](https://github.com/jahan-addison/easyjson)
* `cxxopts` - Lightweight commandline parser
* `matchit` - Pattern matching
* `fmt` - fast constexpr string formatting
* `pybind11`

## Licensing

This project is dual-licensed under the **Apache License, Version 2.0** and the **GNU General Public License, Version 3.0 (or later)**.

You are free to choose the license that best fits your specific use case. For the full text of each license, please see [LICENSE.Apache-v2](LICENSE.Apache-v2) and [LICENSE-GPL-v3](LICENSE.GPL-v3).
