# roxas

### B Compiler to z80 in C++

* The frontend (Lexer, Parser) and first-pass is built with an [easy-to-use LALR(1) grammar and parser generator in python](https://github.com/jahan-addison/xion/tree/master), that interfaces with C++ via libpython or json via `simdjson`
* The backend will focus on modern work in SSA, Sea of Nodes, and compiler optimizations through IR breakthroughs in LLVM, gcc, V8, and related toolchains


<img src="docs/images/roxas-2.png" width="800px" alt="sunil sapkota twitter" > </img>


### Installation

* Install git submodules

```bash
git submodule update --init --recursive
```

...

#### MacOS

...

#### Windows (mingw/msys)

```bash
pacman -S git wget mingw-w64-x86_64-clang mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja mingw-w64-x86_64-cmake make mingw-w64-x86_64-python3 autoconf libtool mingw-w64-x86_64-unicode-character-database
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

```

[More information on llvm in mingw.](https://github.com/mstorsjo/llvm-mingw)


#### Dependencies

Note: These are installed automatically via CPM and cmake.

* simdjson
* cxxopts

### Resources

* [Simple and Efficient Construction of Static Single
Assignment Form](https://c9x.me/compile/bib/braun13cc.pdf)
* [Engineering a compiler](https://shop.elsevier.com/books/engineering-a-compiler/cooper/978-0-12-815412-0)

Check the [project board](https://github.com/users/jahan-addison/projects/3/views/1) for development and feature progress.

### License

Apache 2 License.


![img2](docs/images/roxas-xion-axel.jpg)