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

# End-to-end tests of the generated code. Each test/e2e/<name>.b is compiled
# for a target, run, and its standard output compared with <name>.out.
#
#   <name>.args          the arguments of the program, on one line
#   <name>.stdin         the standard input of the program
#   <name>.xfail         a known failure on every target, and its reason
#   <name>.<arch>.xfail  a known failure on one target, and its reason
#
# A test that fails with an xfail file is expected, and a test that passes
# with one is reported, so the file is removed once the bug is fixed.

usage() {
    cat <<EOF
Usage: $(basename "$0") -a <arch> [-d <directory>] [name ...]

Compile and run the end-to-end tests in test/e2e for a target, and compare
the output of each with its .out file.

Options:
  -a <arch>       Target, arm64 or x86_64
  -d <directory>  Root of the credence source, default .
  -h, --help      Show this help message

Examples:
  $(basename "$0") -a arm64
  $(basename "$0") -a x86_64 globals switch_assignment
EOF
}

send_message() {
    local COLOR='\033[1;32m'
    local RESET='\033[0m'
    printf "${COLOR}%s${RESET}\n" "$*"
}

send_error() {
    local COLOR='\033[1;31m'
    local RESET='\033[0m'
    printf "${COLOR}%s${RESET}\n" "$*"
}

ARCH=''
DIRECTORY='.'
NAMES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        -a) ARCH="$2"; shift 2 ;;
        -d) DIRECTORY="$2"; shift 2 ;;
        -*) usage >&2; exit 1 ;;
        *) NAMES+=("$1"); shift ;;
    esac
done

if [[ "$ARCH" != "arm64" && "$ARCH" != "x86_64" ]]; then
    usage >&2
    exit 1
fi

E2E="$DIRECTORY/test/e2e"
CREDENCE_BINARY="$DIRECTORY/bin/credence"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

TIMEOUT=''
if command -v timeout > /dev/null; then
    TIMEOUT='timeout 10'
elif command -v gtimeout > /dev/null; then
    TIMEOUT='gtimeout 10'
fi

if [[ ${#NAMES[@]} -eq 0 ]]; then
    for source in "$E2E"/*.b; do
        NAMES+=("$(basename "$source" .b)")
    done
fi

passed=0
failed=0
expected_failures=0

# the reason a test is a known failure on this target, or empty
xfail_reason() {
    local name="$1"
    if [[ -f "$E2E/$name.$ARCH.xfail" ]]; then
        cat "$E2E/$name.$ARCH.xfail"
    elif [[ -f "$E2E/$name.xfail" ]]; then
        cat "$E2E/$name.xfail"
    fi
}

# compile and run a test, and write its output, or the reason it failed
run_test() {
    local name="$1"
    local binary="$WORK/$name"
    local args=''
    if ! "$CREDENCE_BINARY" -t "$ARCH" -o "$binary" "$E2E/$name.b" \
        > "$WORK/$name.compile" 2>&1; then
        echo "compile error:"
        cat "$WORK/$name.compile"
        return 1
    fi
    [[ -f "$E2E/$name.args" ]] && args=$(<"$E2E/$name.args")
    local stdin="/dev/null"
    [[ -f "$E2E/$name.stdin" ]] && stdin="$E2E/$name.stdin"
    # shellcheck disable=SC2086
    $TIMEOUT "$binary" $args < "$stdin" > "$WORK/$name.actual" 2>&1
    local status=$?
    if [[ $status -ne 0 ]]; then
        echo "exit status $status, output:"
        cat "$WORK/$name.actual"
        return 1
    fi
    if ! diff -u "$E2E/$name.out" "$WORK/$name.actual" > "$WORK/$name.diff"; then
        cat "$WORK/$name.diff"
        return 1
    fi
    return 0
}

send_message "> E2E: $ARCH on $(uname -s), ${#NAMES[@]} tests"

for name in "${NAMES[@]}"; do
    if [[ ! -f "$E2E/$name.b" ]]; then
        send_error "MISSING $name: no $E2E/$name.b"
        failed=$((failed + 1))
        continue
    fi
    reason=$(xfail_reason "$name")
    if result=$(run_test "$name"); then
        if [[ -n "$reason" ]]; then
            send_error "XPASS   $name: passes, remove its xfail file ($reason)"
            failed=$((failed + 1))
        else
            send_message "PASS    $name"
            passed=$((passed + 1))
        fi
    elif [[ -n "$reason" ]]; then
        printf 'XFAIL   %s: %s\n' "$name" "$reason"
        expected_failures=$((expected_failures + 1))
    else
        send_error "FAIL    $name"
        printf '%s\n' "$result" | sed 's/^/        /'
        failed=$((failed + 1))
    fi
done

summary="$passed passed, $failed failed, $expected_failures known failures"
if [[ $failed -ne 0 ]]; then
    send_error "... $summary"
    exit 1
fi
send_message "... $summary"
