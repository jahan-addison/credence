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

"$DIRECTORY"/test/e2e.sh -a "$ARCH" -d "$DIRECTORY"

send_message "... Done!"