<div align="center">
  <img src="docs/images/credence-compiler-logo.png" width="800px" alt="credence"> </img>
</div>

> B Language Compiler in C++

* B Language grammar - [here](https://github.com/jahan-addison/chakram/blob/master/chakram/grammar.lark)
* Language reference - [here](https://www.nokia.com/bell-labs/about/dennis-m-ritchie/btut.pdf)

---


* The frontend (Lexer, Parser) first-pass is built with an [LALR(1) grammar and parser generator in python](https://github.com/jahan-addison/chakram/tree/master), that interfaces with C++ via libpython
* The backend is exploratory research in modern IRs such as SSA, Sea of Nodes, and compiler optimizations through breakthroughs in LLVM, V8, and similar toolchains. The target platforms are x86_64, arm64, and z80.

_**Status**: in progress_


## Usage

```
Credence :: B Language Compiler
Usage:
  Credence [OPTION...] positional parameters

  -a, --ast-loader arg   AST Loader [json, python] (default: python)
  -t, --target arg       Target [ast, ir, arm64, x86_64, z80] (default: ir)
  -d, --debug            Enable debugging
  -h, --help             Print usage
      --source-code arg  B Source file
```

```bash
./credence --help
```

## Targets

The default compile target is currently the [linear IR](https://github.com/jahan-addison/credence/tree/master/credence/ir):

#### Example

```B
main() {
  auto x, y;
  x = (1 + 1) * (2 + 2);
  y = add(x, 5);
  if (x > y) {
    x = 100;
  }
  y = x;
}

add(x, y) {
  return(x + y);
}
```

Result:


```asm
__main:
 BeginFunc ;
_t2 = (1:int:4) + (1:int:4);
_t3 = (2:int:4) + (2:int:4);
_t4 = _t2 * _t3;
x = _t4;
PUSH (5:int:4);
PUSH x;
CALL add;
POP 16;
y = RET;
_t5 = x > y;
IF _t5 GOTO _L6;
_L1:
y = x;
LEAVE;
_L6:
x = (100:int:4);
GOTO _L1;
 EndFunc ;
__add:
 BeginFunc ;
_t2 = x + y;
RET _t2;
LEAVE;
 EndFunc ;
```

---

Test suite:

```bash
make test
```

## Language

There are a few implementation differences between the compiler and B specification, namely:

* Support for C++ style comments (i.e. `//`)
* Boolean type coercion for conditionals
* Switch statement condition must always be enclosed with `(` and `)`
* Uses C operator precedence
* Constant literals must be exactly 1 byte
* Logical and/or operators behave more like C (i.e. `||` and `&&`)
* Bitwise and/or operators behave more like C (i.e. `|` and `&`)

---

## Installation

### MacOS

```bash
brew update
brew install coreutils include-what-you-use llvm@20 cmake python3 poetry python-dev
git clone git@github.com:jahan-addison/credence.git
cd credence
bash ./scripts/install.sh
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
make
./credence
```

### Windows (mingw/msys)

```bash
pacman -S git wget mingw-w64-x86_64-clang mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja mingw-w64-x86_64-cmake make mingw-w64-x86_64-python3 autoconf libtool
git clone git@github.com:jahan-addison/credence.git
cd credence
bash ./scripts/install.sh
cd build
# Note: iwyu and sanitizers may not work in mingw
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja
./credence

```

## Dependencies

**Note: These are installed automatically via CPM and cmake.**

* `simplejson++` - [lightweight memory safe json library](https://github.com/jahan-addison/simplejson)
* `chakram` - [LALR(1) parser generator](https://github.com/jahan-addison/chakram)
* `cxxopts` - lightweight commandline parser
* `matchit` - pattern matching
* `eternal` - `constexpr` lookup tables
* `pybind11`

# License

Apache 2 License.
