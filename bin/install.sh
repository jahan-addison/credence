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
CXX=''

if [ ! -d "$CREDENCE/credence" ]; then
    echo "Error: Please run script from credence root directory" >&2
    exit 1
fi

##############
# @brief
# Send message
##############
message() {
    local CC='\033[38;5;113m'
    local NC='\033[0m'

    echo -e "\n${CC}$1${NC}\n"
}

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

  message "Installing llvm@20 for Linux ..."

  if ! command -v apt-get &> /dev/null
  then
      message "Error: apt-get command is not available."
      message "This script requires a Debian-based system (e.g., Ubuntu, Debian, Mint)."
      exit 1
  fi

  sudo apt-get update
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh 20

  message "Installing apt-get dependencies ..."

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

  CXX="$(which clang++-20)"

elif [[ "$UNAMESTR" == 'Darwin' ]]; then

  message "Installing brew dependencies ..."

  brew install include-what-you-use pipx uv include-what-you-use llvm@20 cmake gcc clang-format cppcheck coreutils poetry

  echo -e "$MACOS_DETAILS"

  CXX="$(which c++)"
fi

message "Installing the Parser Python submodule ..."

pipx install uv
uv venv --python 3.14.2 --clear
uv tool install pip
source .venv/bin/activate
git submodule update --init --recursive

# shellcheck disable=SC2164
cd python/chakram
uv pip install .
# shellcheck disable=SC2164
cd "$CREDENCE"

echo "$(pwd)" > $HOME/.credence

message "Building with cmake ..."

cmake -Bbuild -DPython3_ROOT_DIR="$Python3_ROOT" -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_CXX_COMPILER="$CXX" -DIWYU=OFF -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

message "Building targets ..."

cmake --build build
chmod +x ./bin/credence

sudo ln -sf "$CREDENCE"/bin/credence /usr/bin/credence

message "A binary is now available at '/usr/bin/credence' with an assembler and linker provided."

message "The build files (including the test suite) are available in './build'"

message "Done! 🚀"

