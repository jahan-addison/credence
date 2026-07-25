set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Specify the cross-compilers
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# If you ever need to link against ARM-specific system libraries
# set(CMAKE_SYSROOT /usr/aarch64-linux-gnu)

set(CMAKE_CROSSCOMPILING TRUE)