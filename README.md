<center>
  <img src="docs/images/credence-compiler-logo.png" width="600px" alt="credence"> </img>
</center>

> B Language Compiler in C++

* B Language grammar - [here](https://github.com/jahan-addison/xion/blob/master/xion/grammar.lark)
* Language reference - [here](https://www.nokia.com/bell-labs/about/dennis-m-ritchie/btut.pdf)

---


* The frontend (Lexer, Parser) and first-pass is built with an [LALR(1) grammar and parser generator in python](https://github.com/jahan-addison/xion/tree/master), that interfaces with C++ via libpython
* The backend is exploratory research in modern IRs such as SSA, Sea of Nodes, and compiler optimizations through breakthroughs in LLVM, V8, and similar toolchains. The target platforms are x86_64, arm64, and z80.

_**status**: in progress_


### Usage

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
./credence my_b_program.b
```

```bash
./credence --help
```
---

Test suite:

```bash
make test
```

### Installation

#### MacOS

```bash
brew update
brew install coreutils include-what-you-use llvm@18 cmake python3 poetry
git clone git@github.com:jahan-addison/credence.git
cd credence
bash ./scripts/install.sh
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
make
./credence
```

#### Windows (mingw/msys)

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

### Dependencies

**Note: These are installed automatically via CPM and cmake.**

* `SimpleJSON` - Simple JSON parser
* `cxxopts` - Lightweight commandline option parser
* `cpptrace` - Stack traces, until C++23
* `matchit` - C++17 pattern matching

### License

Apache 2 License.
