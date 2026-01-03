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

if [[ "$1" == "syscall_test" ]]; then
  printf -v expected_output '%s\n%s' "hello world" "how cool is this man"
fi
if [[ "$1" == "stdlib_test" ]]; then
  printf -v expected_output '%s\n%s\n%s' "hello world" "hello world" "how cool is this man"
fi
if [[ "$1" == "call_test_1" ]]; then
  printf -v expected_output '%s' "hello, how are you"
fi
if [[ "$1" == "call_test_2" ]]; then
  printf -v expected_output '%s' "hello world"
fi
if [[ "$1" == "if_1" ]]; then
  printf -v expected_output '%s\n%s\n%s\n%s\n%s\n%s' "equal to 10" "greater than or equal to 5" "not equal to 5" "greater than 8" "less than 20" "done!"
fi
if [[ "$1" == "while_1" ]]; then
  printf -v expected_output '%s\n%s' "yes!" "x, y: 49 48"
fi
if [[ "$1" == "switch_1" ]]; then
  printf -v expected_output '%s' "should say 1: 1"
fi

if [[ "$1" == "stdlib_putchar_test" ]]; then
  printf -v expected_output '%s' "lol"
fi
if [[ "$1" == "stdlib_printf_test" ]]; then
  printf -v expected_output '%s' "hello 5 5.200000 x 1"
fi

if [[ "$1" == "arm64_constant_1" ]]; then
  printf -v expected_output '%s\n' "m is 60"
fi


program_name=$1

if [[ "$1" == "stdin" ]]; then
  program_name=$2
  if [[ "$2" == "stdlib_getchar_test" ]]; then
    expected_output='h'
    # shellcheck disable=SC2217
    program_output=$(echo -n "h" < ./stdlib_getchar_test)
  fi
elif [[ "$1" == "argc" ]]; then
  if [[ "$2" == "argc_argv" ]]; then
    printf -v expected_output '%s\n%s\n%s\n%s' "argc count: 6" "argv 1: test" "argv 2: 5" "argv 3: test"
    program_name="argc_argv"
    program_output=$(./argc_argv test 5 test hello world)
  fi
else
  program_output=$(./"$1")
fi




if [[ "$program_output" == "$expected_output" ]]; then
  printf 'Source code compiled SUCCESSFULLY for "%s" on %s with: %s\n' "$program_name" "$(uname -s)" "$(uname -a)"
  exit 0
else
  printf 'Source code compiled FAILED for "%s" on %s with: %s\n' "$program_name" "$(uname -s)" "$(uname -a)"
  echo "--- Expected Output ---"
  echo "$expected_output"
  echo "--- Actual Output ---"
  echo "$program_output"
  exit 1
fi