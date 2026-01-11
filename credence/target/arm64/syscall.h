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
 * ARM64 System Call Interface
 *
 * Implements syscall invocation for ARM64 Linux and Darwin. Loads syscall
 * number into x8 (Linux) or x16 (Darwin) and arguments into x0-x7.
 * Executes svc #0 instruction. Return value in x0.
 *
 * Example - exit syscall:
 *
 *   B code:    main() { return(0); }
 *
 * Generates (Linux):
 *   mov x8, #93        ; exit syscall number
 *   mov x0, #0         ; exit code
 *   svc #0
 *
 * Generates (Darwin):
 *   mov x16, #1        ; Darwin exit number
 *   mov x0, #0
 *   svc #0x80
 *
 *****************************************************************************/

#pragma once

#include "assembly.h"                     // for Instructions, Storage, Reg...
#include "memory.h"                       // for Memory_Access, general_pur...
#include <array>                          // for array
#include <credence/target/common/types.h> // for get_first_of_enum_t
#include <cstddef>                        // for size_t
#include <deque>                          // for deque
#include <map>                            // for map
#include <string>                         // for basic_string
#include <string_view>                    // for basic_string_view, string_...

namespace credence::target::arm64::syscall_ns {

using Instructions = arm64::assembly::Instructions;
using Register = arm64::assembly::Register;

using syscall_t = std::array<std::size_t, 2>;
using syscall_list_t = std::map<std::string_view, syscall_t>;
using syscall_arguments_t = std::deque<assembly::Storage>;

void exit_syscall(assembly::Instructions& instructions, int exit_status = 0);

void make_syscall(assembly::Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Memory_Access* accessor = nullptr);

void syscall_operands_to_instructions(assembly::Instructions& instructions,
    syscall_arguments_t const& arguments,
    memory::registers::general_purpose& w_registers,
    memory::registers::general_purpose& d_registers,
    memory::Memory_Access* accessor = nullptr);

inline Register get_storage_register_from_safe_address(
    assembly::Storage& argument,
    memory::registers::general_purpose const& w_registers,
    memory::registers::general_purpose const& d_registers,
    memory::Memory_Access const* accessor = nullptr)
{
    Register storage = common::get_first_of_enum_t<Register>();
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        if (accessor_->device_accessor.is_doubleword_storage_size(argument)) {
            storage = w_registers.back();
        } else {
            storage = d_registers.back();
        }
    }
    return storage;
}

/**
 * @brief NULL-check the memory access, then set the signal register
 */
bool check_signal_register_from_safe_address(Instructions& instructions,
    assembly::Register storage,
    memory::Memory_Access* accessor);
}