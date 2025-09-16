<div align="center">
  <img src="docs/images/credence-compiler-logo.png" width="800px" alt="credence"> </img>
</div>

* B Language grammar - [here](https://github.com/jahan-addison/chakram/blob/master/chakram/grammar.lark)
* Language reference - [here](https://www.nokia.com/bell-labs/about/dennis-m-ritchie/btut.pdf)

---

The compiler works in 3 stages:

* The Lexer, Parser first-pass, built with an LALR(1) grammar and parser generator in python that interfaces with C++ via `pybind11`
* An IR I've called [Instruction Tuple Abstraction or ITA](credence/ir/README.md) - a linear 4-tuple set of platform-agnostic instructions that represent program flow, scope, and application runtime

* The target platforms - currently x86_64, arm64, and z80

---

There are a few differences between the compiler and B specification, namely:

* Support for C++ style comments (i.e. `//`)
* Boolean type coercion for all data types in conditionals
* Switch statement condition must always be enclosed with `(` and `)`,
* Uses C operator precedence
* Constant literals must be exactly 1 byte
* Logical operators behave more like C (i.e. `||` and `&&`)
* Bitwise operators behave more like C (i.e. `|` and `&`)

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

The default compile target is currently ITA:

### Example:

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


---

## Test suite

```bash
make test
```

---

## Installation

Note: `$PYTHONHOME` must be set to an installation that has [chakram](https://github.com/jahan-addison/chakram) installed.


### Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y gcc-10 llvm-20 valgrind clang-20 iwyu python3-dev cppcheck clang-tidy pipx
# Inside the repository:
echo 'eval "$(register-python-argcomplete pipx)"' >> ~/.profile
source ~/.profile
cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

### MacOS

```bash
brew update
brew install coreutils include-what-you-use llvm@20 cmake python3 pyenv
# Inside the repository:
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

### Windows (mingw/msys)

```bash
pacman -S git wget mingw-w64-x86_64-clang mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja mingw-w64-x86_64-cmake make mingw-w64-x86_64-python3 autoconf libtool
# Inside the repository:
# Note: iwyu and the -fsanitizers do not work in mingw
cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja
```

#### Installing chakram

```bash
git submodule update --init --recursive
cd python/chakram
pipx install poetry # or similar
poetry install
# Be sure to use pyenv or similar
make install
```

---

## Dependencies

**Note: These are installed automatically via CPM and cmake.**

* `simplejson++` - [lightweight memory safe json library](https://github.com/jahan-addison/simplejson)
* `chakram` - [LALR(1) parser generator and Lexer](https://github.com/jahan-addison/chakram)
* `cxxopts` - lightweight commandline parser
* `matchit` - pattern matching
* `eternal` - `constexpr` lookup tables
* `pybind11`

# License

Apache 2 License.
