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

/****************************************************************************
 * ARM64 Syscall Interface
 *
 * Syscalls on ARM64 Linux and Darwin. The arguments are in x0 to x5, the
 * syscall number is in x8 on Linux and x16 on Darwin, and the syscall is
 * made with `svc #0` on Linux and `svc #0x80` on Darwin. The `main` function
 * returns with the exit syscall.
 *
 *  Example:
 *
 *    LEAVE;
 *
 *  Becomes, in `main` on Linux:
 *
 *    mov w0, #0
 *    mov x8, #93
 *    svc #0
 *
 *  And on Darwin:
 *
 *    mov w0, #0
 *    mov x16, #1
 *    svc #0x80
 *
 ****************************************************************************/

namespace credence::target::arm64::syscall_ns {

using Instructions = arm64::assembly::Instructions;
using Register = arm64::assembly::Register;

using syscall_t = std::array<std::size_t, 2>;
using syscall_list_t = std::map<std::string_view, syscall_t>;
using syscall_arguments_t = std::deque<assembly::Storage>;
using syscall_register_t = memory::registers::general_purpose;

/**
 * @brief Translate a syscall and its arguments on ARM64
 *
 * Each argument takes the next argument register of its size and type, and
 * the argument count must be the count of the syscall
 */
class Syscall_Invocation_Inserter
{
  public:
    Syscall_Invocation_Inserter() = delete;
    Syscall_Invocation_Inserter(Syscall_Invocation_Inserter const&) = delete;
    Syscall_Invocation_Inserter& operator=(
        Syscall_Invocation_Inserter const&) = delete;

  public:
    explicit Syscall_Invocation_Inserter(memory::Memory_Access& accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
        auto [doubleword_storage, word_storage, float_storage, double_storage] =
            assembly::get_available_argument_register();
        word_registers_ = word_storage;
        dword_registers_ = doubleword_storage;
        float_registers_ = float_storage;
        double_registers_ = double_storage;
    }

  public:
    void exit_syscall(assembly::Instructions& instructions,
        int exit_status = 0);

    void make_syscall(assembly::Instructions& instructions,
        std::string_view syscall,
        syscall_arguments_t const& arguments,
        common::memory::Locals& argument_stack);

    void syscall_operands_to_instructions(assembly::Instructions& instructions,
        syscall_arguments_t const& arguments,
        common::memory::Locals& argument_stack);

    Register get_storage_register_from_safe_address(
        assembly::Storage const& argument,
        common::memory::Locals& argument_stack,
        unsigned int index);

  private:
    bool check_signal_register_from_safe_address(Instructions& instructions,
        assembly::Register storage);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame stack_frame_;
    syscall_register_t word_registers_;
    syscall_register_t dword_registers_;
    syscall_register_t float_registers_;
    syscall_register_t double_registers_;
};

}