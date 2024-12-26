# roxas

### B Compiler to z80 in C++

* The frontend is built with an [easy-to-read LALR(1) grammar and parser generator in python](https://github.com/jahan-addison/xion/tree/master), that interfaces with C++ via CPython
* The backend will focus on modern work in SSA, Sea of Nodes, and compiler optimizations through IR breakthroughs in LLVM, gcc, V8, and related toolchains


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
pacman -S git wget mingw-w64-x86_64-clang mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja mingw-w64-x86_64-cmake make mingw-w64-x86_64-python3 autoconf libtool
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

```

[More information on llvm in mingw.](https://github.com/mstorsjo/llvm-mingw)


### Resources

* [Simple and Efficient Construction of Static Single
Assignment Form](https://c9x.me/compile/bib/braun13cc.pdf)

Check the [project board](https://github.com/users/jahan-addison/projects/3/views/1) for development and feature progress.

<table border="0">
	<td width="350px">
		<b>Apache License 2.0</b>
		<br>Grammar under CC0 1.0<br>
		<i>roxas</i>
	</td>
	<td border="0"><img src="docs/images/roxas.jpg" width="400px" alt="sunil sapkota twitter" > </img></td>
</table>


#### Special Thanks

* [Engineering a compiler](https://shop.elsevier.com/books/engineering-a-compiler/cooper/978-0-12-815412-0)
