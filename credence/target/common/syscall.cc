/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include "syscall.h"
#include "assembly.h"
#include <string> // for basic_string, string

/****************************************************************************
 *
 * System call tables and kernel interface
 *
 * Maps system calls (write, read, exit, etc.) to their numbers and calling
 * conventions for each platform: x86-64 Linux, x86-64 Darwin (macOS),
 * ARM64 Linux, and ARM64 Darwin. Handles differences in syscall numbers
 * and register usage across platforms.
 *
 * Example - exit syscall:
 *
 *   B code:    main() { return(42); }
 *
 *   x86-64 Linux:  mov rax, 60    ; exit syscall number
 *                  mov rdi, 42    ; exit code
 *                  syscall
 *
 *   x86-64 Darwin: mov rax, 0x2000001  ; Darwin exit
 *                  mov rdi, 42
 *                  syscall
 *
 *   ARM64 Linux:   mov x8, #93    ; exit syscall
 *                  mov x0, #42
 *                  svc #0
 *
 ****************************************************************************/

namespace credence::target::common::syscall_ns {

/**
 * @brief Get the list of available syscalls on the current platform
 */
std::vector<std::string> get_platform_syscall_symbols(assembly::OS_Type os_type,
    assembly::Arch_Type arch_type)
{
    std::vector<std::string> symbols{};
    auto syscall_list = get_syscall_list(os_type, arch_type);
    // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
    for (auto const& syscall : syscall_list) {
        // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
        symbols.emplace_back(syscall.first.data());
    }
    return symbols;
}

} // namespace credence::target::arm64::syscall_ns
