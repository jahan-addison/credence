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
export PYENV_ROOT="\$HOME/.pyenv"

You might also need LLVM setup in your \$PATH:

export PATH=/opt/homebrew/bin:\$PATH:/opt/homebrew/opt/llvm/bin:\$HOME/.local/bin:
$(printf '\033[0m')
EOM
)

if [[ "$UNAMESTR" == 'Linux' ]]; then

  sudo apt-get update
  wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
  sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs) main"
  sudo apt-get install -y gcc-10 llvm-20 valgrind clang-20 iwyu python3-dev cppcheck binutils cmake clang-tidy pipx
  echo 'eval "$(register-python-argcomplete pipx)"' >> ~/.bashrc
  pipx install poetry
  sudo update-alternatives \
    --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-10 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-10 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-10
  sudo update-alternatives \
    --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-20 100 \
    --slave /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-20 \
    --slave /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-20
  sudo update-alternatives \
    --install /usr/bin/clang clang /usr/bin/clang-20 100

  cmake -Bbuild -DCMAKE_CXX_COMPILER="$(which clang++)" -DIWYU=OFF -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

elif [[ "$UNAMESTR" == 'Darwin' ]]; then
  brew install include-what-you-use pipx include-what-you-use llvm@20 cmake gcc clang-format cppcheck coreutils poetry

  echo -e "$MACOS_DETAILS"

  cmake -Bbuild -DCMAKE_CXX_COMPILER=/usr/bin/c++ -DIWYU=OFF -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

echo -e "\033[38;5;113mInstalling the Lexing and Parsing Python submodule...\033[0m"

git submodule update --init --recursive
# shellcheck disable=SC2164
cd python/chakram
poetry install
make install

cd ../../

echo "$(pwd)" > $HOME/.credence
cmake --build build
chmod +x ./bin/credence

sudo ln -sf "$CREDENCE"/bin/credence /usr/bin/credence

echo -e "\033[38;5;113mDone.\033[0m"

