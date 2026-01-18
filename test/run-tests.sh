#!/usr/bin/env bash
#####################################################################################
## Copyright (c) Jahan Addison
##
## This software is dual-licensed under the Apache License, Version 2.0
## or the GNU General Public License, Version 3.0 or later.
## You may choose either license at your option.
##
## See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
## for the full text of these licenses.
#####################################################################################
set -e

ARCH=''
DIRECTORY=""

require_install() {
    local missing_count=0
    for cmd in "$@"; do
        if ! command -v "$cmd" &> /dev/null; then
            echo "Error: Command '$cmd' not found."
            missing_count=$((missing_count + 1))
        fi
    done
    if [ $missing_count -gt 0 ]; then
        echo "Please install the missing dependencies and try again."
        exit 1
    fi
}

send_message() {
    local COLOR='\033[1;32m'
    local RESET='\033[0m'
    printf "${COLOR}%s${RESET}\n" "$*"
}


if [ "$#" -eq 0 ]; then
  echo "Usage: $0 -a <arch> -d <directory>"
  exit 1
fi

while getopts ":a:d:" opt; do
  case $opt in
    a) ARCH="$OPTARG" ;;
    d) DIRECTORY="$OPTARG" ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :)  echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
  esac
done

if [[ -z "$ARCH" ]]; then
    echo "Error: Argument -a (arch) is required."
    exit 1
fi

if [[ -z "$DIRECTORY" ]]; then
    echo "Error: Argument -d (directory) is required."
    exit 1
fi

send_message "Running test suite ..."

"$DIRECTORY"/build/test_suite

CREDENCE_BINARY="$DIRECTORY/bin/credence"

send_message "!!! RUNNING EXPECTED MACHINE CODE OUTPUT TESTS !!!"

if [[ "$ARCH" == "arm64" ]] ; then

  send_message "Building arm64 tests ..."

  "$CREDENCE_BINARY" -t arm64 -o arm64_constant_1 ./test/fixtures/platform/math_constant_8.b
  "$CREDENCE_BINARY" -t arm64 -o vector_4 ./test/fixtures/platform/vector_4.b
  "$CREDENCE_BINARY" -t arm64 -o globals_3 ./test/fixtures/platform/globals_3.b
  "$CREDENCE_BINARY" -t arm64 -o call_test_1 ./test/fixtures/platform/call_1.b
  "$CREDENCE_BINARY" -t arm64 -o call_test_2 ./test/fixtures/platform/call_2.b
  "$CREDENCE_BINARY" -t arm64 -o stdlib_putchar_test ./test/fixtures/platform/stdlib/putchar_1.b

  send_message "Running arm64 tests ..."

  ./test/compiled-test.sh arm64_constant_1
  ./test/compiled-test.sh vector_4
  ./test/compiled-test.sh globals_3
  ./test/compiled-test.sh call_test_1
  ./test/compiled-test.sh call_test_2
  ./test/compiled-test.sh stdlib_putchar_test

else

  send_message "Building x86_64 tests ..."

  "$CREDENCE_BINARY" -t x86_64 -o syscall_test ./test/fixtures/platform/stdlib/write.b
  "$CREDENCE_BINARY" -t x86_64 -o stdlib_test ./test/fixtures/platform/stdlib/print.b
  "$CREDENCE_BINARY" -t x86_64 -o globals_3 ./test/fixtures/platform/globals_3.b
  "$CREDENCE_BINARY" -t x86_64 -o stdlib_putchar_test ./test/fixtures/platform/stdlib/putchar_1.b
  "$CREDENCE_BINARY" -t x86_64 -o stdlib_getchar_test ./test/fixtures/platform/stdlib/getchar_1.b
  "$CREDENCE_BINARY" -t x86_64 -o call_test_1 ./test/fixtures/platform/call_1.b
  "$CREDENCE_BINARY" -t x86_64 -o if_1 ./test/fixtures/platform/relational/if_1.b
  "$CREDENCE_BINARY" -t x86_64 -o while_1 ./test/fixtures/platform/relational/while_1.b
  "$CREDENCE_BINARY" -t x86_64 -o switch_1 ./test/fixtures/platform/relational/switch_1.b
  "$CREDENCE_BINARY" -t x86_64 -o call_test_2 ./test/fixtures/platform/call_2.b
  "$CREDENCE_BINARY" -t x86_64 -o stdlib_printf_test ./test/fixtures/platform/stdlib/printf_1.b
  "$CREDENCE_BINARY" -t x86_64 -o argc_argv ./test/fixtures/platform/argc_argv.b

  send_message "Running x86_64 tests ..."

  ./test/compiled-test.sh syscall_test
  ./test/compiled-test.sh stdlib_test
  ./test/compiled-test.sh globals_3
  ./test/compiled-test.sh call_test_1
  ./test/compiled-test.sh if_1
  ./test/compiled-test.sh while_1
  ./test/compiled-test.sh switch_1
  ./test/compiled-test.sh call_test_2
  ./test/compiled-test.sh stdlib_putchar_test
  ./test/compiled-test.sh stdin stdlib_getchar_test
  ./test/compiled-test.sh argc argc_argv
  ./test/compiled-test.sh stdlib_printf_test
fi


send_message "... Done!"