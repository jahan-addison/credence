# Known Bugs

Each bug is a known failure of the [end-to-end suite](/development.md#end-to-end-tests), and is named by its test in [`test/e2e/`](/test/e2e). The `.xfail` file of the test holds its reason, on every target or on one. A bug is fixed when its test passes and its `.xfail` file is removed - the suite reports a known failure that passes, so the file is not left behind.

```bash
./test/e2e.sh -a arm64 while_break    # one test
```

## Both Targets

* `while_break` - a `break` in an `if` statement in a `while` loop does not leave the loop, and the program does not end
* `char_escape` - an escape sequence in a char literal, `'\n'`, is the backslash
* `vector_variable_index` - a store to a local vector element at a variable offset, `v[i] = x`, is to the wrong element, and the arm64 compiler crashes in a loop
* `vector_element_copy` - an assignment of a vector element to another, `v[2] = v[1]`, stores garbage on arm64, and fails to compile on x86-64 with `unreachable`
* `unary_minus_operand` - unary minus as an operand of a binary expression, `-a + 10`, crashes the arm64 compiler, and the x86-64 program
* `ternary` - a ternary expression crashes the arm64 compiler, and is its condition on x86-64, not its operand
* `float_arithmetic` - float and double arithmetic does not assemble on arm64, and prints `0.000000` on x86-64
* `global_arithmetic` - a global as an operand of a binary expression, `x = unit + 1;`, is moved from memory, `mov x8, [x6]` and `mov rax, dword ptr [rip + unit]`, which do not assemble
* `global_double` - a double global is the address of its literal, so `printf("%f", ratio)` prints `0.000000` on arm64, and does not assemble on x86-64
* `vector_parameter` - a vector offset that is a parameter, `mess[n]`, fails to compile with "symbol not found"

## x86-64

* `seven_arguments` - a call with more than six arguments exhausts the argument registers, and the compiler crashes
* `parameters` - an assignment to a parameter after a call is lost
* `globals` - a store to a global moves a word into a doubleword register, `mov rax, eax`, which does not assemble

## Unsupported

* `goto` and labels are not supported, and [`test/fixtures/label.b`](/test/fixtures/label.b) hangs on arm64 and x86-64

**Note**: The `.xfail` file of a bug on one target is `<name>.<arch>.xfail`, such as `ternary.x86_64.xfail`, so a fix on one target is reported on its own.
