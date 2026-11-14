# Development

## Building

The recommended build commands, with the address and undefined behavior sanitizers:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DIWYU=OFF
cmake --build build --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
```

* `-DIWYU=ON` runs include-what-you-use over each C++ target
* `cmake --build build --target tidy` runs clang-tidy over the `credence/` sources

The pre-commit hooks run clang-format, clang-tidy, and cppcheck, and expect a `build/` directory with a compilation database:

```bash
pre-commit install
```

## Cross-compilation

There is a docker image and cross-compilation toolchain available to run the compiler in arm64 on a x86-64 host:

```bash
cmake -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake -DUSE_SANITIZER=OFF -DCMAKE_BUILD_TYPE=Debug -DIWYU=OFF
cmake --build build-arm64 --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
```

And the same for x86-64 on an arm64 host. `bin/x86_64_runner.sh` runs, traces, or debugs an x86-64 Linux binary in the `Dockerfile.x86_64` image, and `--suite` builds Credence and runs the test suite with x86-64 code generation in the image:

```bash
cmake -B build-x86_64 -DCMAKE_TOOLCHAIN_FILE=x86_64-toolchain.cmake -DUSE_SANITIZER=OFF -DCMAKE_BUILD_TYPE=Debug -DIWYU=OFF
cmake --build build-x86_64 --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
./bin/x86_64_runner.sh --suite
```

## Test Suite

Credence [line and function code coverage](https://jahan-addison.github.io/Brocoli/) is available with exhaustive type checking, ast, and code generation test suites.

* `./build/test_suite` runs the doctest suite - the frontend, the IR, and the expected assembly of each target in `test/arm64/expected/` and `test/x86_64/expected/`
* `./test/e2e.sh -a arm64` compiles and runs each program in `test/e2e/`, and compares its output with its `.out` file. On an Apple Silicon Mac, `-a x86_64` runs the x86-64 programs with Rosetta, so both targets are tested natively on macOS
* `./test/run-tests.sh -a arm64 -d .` runs both, as the CI and Docker images do

The expected assembly is a record of the output of the backends, and the end-to-end suite is the test of their correctness - a backend that emits the wrong instructions is caught when its program is run, not when its assembly is compared.

### End-to-End Tests

Each test is a Brocoli program in `test/e2e/`, and the files beside it with the same name:

* `<name>.out` - its expected standard output
* `<name>.args` - its arguments, on one line
* `<name>.stdin` - its standard input
* `<name>.xfail` or `<name>.<arch>.xfail` - a known failure, on every target or on one, and its reason

A test is added with its files, and needs no change to a script. A test with an `.xfail` file that fails is a known failure, and one that passes is reported, so the file is removed with the fix. Each known failure is listed in [`docs/known_bugs.md`](docs/known_bugs.md). I write each `.out` file from what the program should print, rather than from the output of the compiler, so a bug is never recorded as the expected output.

```bash
./test/e2e.sh -a arm64                      # every test
./test/e2e.sh -a x86_64 globals switch_1    # some tests
```

To build the test suite and coverage yourself (with `llvm-cov`), run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DIWYU=OFF -DENABLE_TEST_COVERAGE=ON
cmake --build build --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
cmake --build build --target coverage
```

## The Standard Library

The standard library is hand-written assembly in `stdlib/<arch>/<os>/stdlib.s`, and its object file is committed beside it, so the compiler and test suite never assemble it. After a change to a `stdlib.s` file, the `stdlib` target re-assembles each object in the `Dockerfile.stdlib` image:

```bash
cmake --build build --target stdlib
```

* Docker is required. Without it the target is unavailable, and the compiler and test suite use the committed objects
* The Linux objects are assembled with the GNU cross binutils, and the Darwin objects with the clang integrated assembler - no Mac or SDK is needed
* Only the objects whose `stdlib.s` changed are assembled
* `CREDENCE_STDLIB_PLATFORMS` selects the platforms, e.g. `-DCREDENCE_STDLIB_PLATFORMS="arm64/linux"`. The cross-compilation toolchains set it to their own platform

**Note**: Commit the `stdlib.o` files with the `stdlib.s` change.

## The Lexer

`credence/language/lexer.cc` is generated from `lexer.re` by [re2c](https://github.com/skvadrik/re2c), and committed. After a change to `lexer.re`, with `re2c` installed:

```bash
cmake --build build --target regenerate_lexer
```

The `#line` directives in `lexer.cc` are relative to the source directory, and the output is formatted with clang-format.

**Note**: Commit `lexer.cc` with the `lexer.re` change.
