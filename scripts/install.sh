#!/bin/bash
echo "### Installing dependencies ..."

directory=$(pwd)

git submodule update --init --recursive
cd external/xion
poetry install
make install

echo "### Creating build directory"

cd "$directory"
mkdir -p "$directory"/build

echo "Done."

