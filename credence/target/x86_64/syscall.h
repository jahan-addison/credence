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
 * x86-64 System Call Interface
 *
 * Implements syscall invocation for x86-64 Linux and Darwin. Loads syscall
 * number into rax and arguments into rdi, rsi, rdx, r10, r8, r9. Executes
 * syscall instruction. Return value in rax.
 *
 * Example - exit syscall:
 *
 *   B code:    main() { return(0); }
 *
 * Generates (Linux):
 *   mov rax, 60        ; exit syscall number
 *   mov rdi, 0         ; exit code
 *   syscall
 *
 * Generates (Darwin):
 *   mov rax, 0x2000001 ; Darwin exit number
 *   mov rdi, 0
 *   syscall
 *
 *****************************************************************************/

#pragma once

#include "assembly.h"  // for Instructions, Storage, Reg...
#include "memory.h"    // for general_purpose, Stack_Frame
#include <array>       // for array
#include <cstddef>     // for size_t
#include <deque>       // for deque
#include <map>         // for map
#include <string>      // for basic_string
#include <string_view> // for basic_string_view, string_...

#include <credence/target/common/assembly.h> // for make_numeric_immediate
#include <credence/target/common/types.h>    // for get_first_of_enum_t

namespace credence::target::x86_64::syscall_ns {

using Instructions = x86_64::assembly::Instructions;
using Register = x86_64::assembly::Register;

using syscall_t = std::array<std::size_t, 2>;
using syscall_list_t = std::map<std::string_view, syscall_t>;
using syscall_arguments_t = std::deque<assembly::Storage>;

void syscall_operands_to_instructions(assembly::Instructions instructions,
    syscall_arguments_t const& arguments,
    memory::registers::general_purpose& w_registers,
    memory::registers::general_purpose& d_registers,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Accessor* accessor = nullptr);

inline Register get_storage_register_from_safe_address(
    assembly::Storage const& argument,
    memory::registers::general_purpose const& w_registers,
    memory::registers::general_purpose const& d_registers,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Accessor const* accessor = nullptr)
{
    Register storage = target::common::get_first_of_enum_t<Register>();
    if (accessor != nullptr and stack_frame != nullptr) {
        auto accessor_ = *accessor;
        if (accessor_.address_accessor.is_qword_storage_size(
                argument, *stack_frame)) {
            storage = w_registers.back();
        } else {
            storage = d_registers.back();
        }
    }
    return storage;
}

namespace common {

void exit_syscall(assembly::Instructions& instructions, int exit_status = 0);

void make_syscall(assembly::Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame = nullptr,
    memory::Memory_Access* accessor = nullptr);

}

/**
 * @brief NULL-check the memory access, then set the signal register
 */
bool check_signal_register_from_safe_address(Instructions& instructions,
    assembly::Register storage,
    memory::Memory_Access* accessor);
}