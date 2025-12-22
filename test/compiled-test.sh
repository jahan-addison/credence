#!/usr/bin/env bash
#########################################################################
# Copyright (c) Jahan Addison
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#########################################################################

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
if [[ "$1" == "stdlib_putchar_test" ]]; then
  printf -v expected_output '%s' "lol"
fi
if [[ "$1" == "stdlib_printf_test" ]]; then
  printf -v expected_output '%s' "hello 5 5.200000 x"
fi


program_name=$1

if [[ "$1" == "stdin" ]]; then
  program_name=$2
  if [[ "$2" == "stdlib_getchar_test" ]]; then
    expected_output='h'
    # shellcheck disable=SC2217
    program_output=$(echo -n "h" < ./stdlib_getchar_test)
  fi
else
  program_output=$(./"$1")
fi


if [[ "$program_output" == "$expected_output" ]]; then
  printf 'Source code compiled successfully for "%s" on current platform: %s\n' "$program_name" "$(uname -s)"
  exit 0
else
  printf 'Source code FAILED to compile for "%s" on current platform: %s\n' "$program_name" "$(uname -s)"
  echo "--- Expected Output ---"
  echo "$expected_output"
  echo "--- Actual Output ---"
  echo "$program_output"
  exit 1
fi