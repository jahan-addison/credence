# Overview

Each platform roughly has the same set of abstract classes, macros, and templates:

## Accessors
#### A set of pure virtual and template classes that enable platform-dependent memory access
# Visitor
#### Pure virtual [IR instruction](/credence/ir/README.md) visitor base for each platform
## Assembly
#### Raw ISA mnemonics types, registers, operand storage helpers with string overloading
## Memory
#### Dynamic `Memory_Accessor` pointer that mediates available memory, stack, and registers and their runtime storage via the accessors.
## Stack
#### A stack that represents the platform-dependent ISA stack pointer with memory-safe offset resolution
## Flags
#### Instruction flags, such as enforcing a word size or an address mode
## Syscall
#### Platform dependent kernel `syscall` codes, which in Linux's case is different for ARM64 and X8664
## Runtime
#### Standard library and syscall function invocation, operand type checkers

---

# Code generation

Each platform inherits the above types and template classes to visit each IR instruction and construct machine code for X8664, ARM64, on Linux and BSD (Darwin).


See `target/x86_64/generator.h`, and `target/arm64/generator.h` for futher details.

Check out the test suites in `test/x86_64/generator.cc` and `test/arm64/generator.cc` to see what the machine code looks like for each platform.