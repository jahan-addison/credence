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
 * Syscall Tables
 *
 * The syscalls a program may call on each platform, x86-64 and ARM64 on
 * Linux and Darwin, with their number and argument count. A syscall is
 * called like a function, and its arguments are loaded into the syscall
 * registers of the platform.
 *
 *  Example:
 *
 *    main() {
 *      write(1, "hi", 2);
 *    }
 *
 *  Becomes, on arm64 Darwin:
 *
 *    mov w0, #1
 *    adrp x1, ._L_str1__@PAGE
 *    add x1, x1, ._L_str1__@PAGEOFF
 *    mov w2, #2
 *    mov x16, #4           // write
 *    svc #0x80
 *
 *  And on x86-64 Darwin, where the number is in the Unix syscall class:
 *
 *    mov rax, 33554436     // 0x2000000 | 4
 *    mov edi, 1
 *    lea rsi, [rip + ._L_str1__]
 *    mov edx, 2
 *    syscall
 *
 ****************************************************************************/

namespace credence::target::common::syscall_ns {

/**
 * @brief Get the names of the syscalls of a platform
 *
 * Each name is added to the hoisted symbols as a function, so a program
 * calls it by name
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

} // namespace credence::target::common::syscall_ns
