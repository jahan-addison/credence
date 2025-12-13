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

CREDENCE=$(pwd)
UNAMESTR=$(uname -s)

if [ ! -d "$CREDENCE/credence" ]; then
    echo "Error: Please run script from credence root directory" >&2
    exit 1
fi

MACOS_DETAILS=$(cat << EOM
$(printf '\033[38;5;113m')
You may need to add the following lines to your shell environment:

export CXX=/usr/bin/c++
export SDKROOT="\$(xcrun --sdk macosx --show-sdk-path)"
export PATH=\$PATH:\$HOME/.local/bin:

You might also need LLVM setup in your \$PATH:

export PATH=/opt/homebrew/bin:\$PATH:/opt/homebrew/opt/llvm/bin:\$HOME/.local/bin:
$(printf '\033[0m')
EOM
)

Python3_ROOT="$HOME/.local/share/uv/python/cpython-3.14.2-*/"

if [[ "$UNAMESTR" == 'Linux' ]]; then
  sudo apt-get update
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh 20
  sudo apt-get install clang-20 llvm-20 lldb-20 lld-20 libc++-20-dev libc++abi-20-dev
  sudo apt-get install -y python3-dev libpython3-dev gcc-10 valgrind iwyu cppcheck binutils cmake clang-tidy
  python3 -m pip install --user pipx
  sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-20 100
  sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-20 100
  sudo update-alternatives \
    --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-10 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-10 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-10
  cmake -Bbuild -DPython3_ROOT_DIR="$Python3_ROOT" -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_CXX_COMPILER="$(which clang++-20)" -DIWYU=OFF -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

elif [[ "$UNAMESTR" == 'Darwin' ]]; then
  brew install include-what-you-use pipx include-what-you-use llvm@20 cmake gcc clang-format cppcheck coreutils poetry

  echo -e "$MACOS_DETAILS"

  cmake -Bbuild -DPython3_ROOT_DIR="$Python3_ROOT" -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_CXX_COMPILER=c++ -DCMAKE_CXX_STANDARD=20 -DIWYU=OFF -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

echo -e "\033[38;5;113mInstalling the Lexer and Parser Python submodule...\033[0m"

pipx install uv
uv venv --python 3.14.2 --clear
uv tool install pip
source .venv/bin/activate
git submodule update --init --recursive

# shellcheck disable=SC2164
cd python/chakram
uv pip install .

cd $CREDENCE

echo "$(pwd)" > $HOME/.credence

cmake --build build
chmod +x ./bin/credence

sudo ln -sf "$CREDENCE"/bin/credence /usr/bin/credence

echo -e "\033[38;5;113mDone.\033[0m"

