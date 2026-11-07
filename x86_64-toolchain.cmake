set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross-compilers
set(CMAKE_C_COMPILER x86_64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER x86_64-linux-gnu-g++)

# If you ever need to link against x86-64-specific system libraries
# set(CMAKE_SYSROOT /usr/x86_64-linux-gnu)

set(CMAKE_CROSSCOMPILING TRUE)

# The `stdlib` target assembles the standard library of this platform only.
set(CREDENCE_STDLIB_PLATFORMS
    "x86_64/linux"
    CACHE STRING "Standard library platforms (<arch>/<os>) to assemble")
