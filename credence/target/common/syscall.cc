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

/****************************************************************************
 *
 * System call management implementation
 *
 * Retrieves available syscalls for the target platform and architecture.
 * Returns list of syscall symbols that can be used in B programs.
 *
 *****************************************************************************/

#include "syscall.h"
#include "assembly.h"
#include <string> // for basic_string, string

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
