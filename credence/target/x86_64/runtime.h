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

#include "assembly.h"                           // for Register, Instructions
#include "memory.h"                             // for Memory_Access, Stack...
#include "stack.h"                              // for Stack
#include <credence/ir/object.h>                 // for Object_PTR
#include <credence/target/common/runtime.h>     // for address_t, library_a...
#include <credence/target/common/stack_frame.h> // for Locals
#include <cstddef>                              // for size_t
#include <string>                               // for basic_string
#include <string_view>                          // for string_view
#include <utility>                              // for pair

/****************************************************************************
 *
 * x86-64 Runtime and Standard Library Integration
 *
 * Handles function calls to the standard library and manages the System V
 * ABI calling convention. Arguments passed in registers: rdi, rsi, rdx,
 * rcx, r8, r9, then stack. Return value in rax.
 *
 * Example - calling printf:
 *
 *   B code:    printf("Value: %d\n", x);
 *
 * Generates:
 *   lea rdi, [rip + ._L_str1__]  ; format string in rdi
 *   mov rsi, qword ptr [rbp - 8] ; x in rsi
 *   call printf                   ; from stdlib
 *
 * Example - main with argc/argv:
 *
 *   B code:    main(argc, argv) { ... }
 *
 * Setup:
 *   r15 points to stack with argc/argv (Darwin/Linux compatible)
 *
 *****************************************************************************/

namespace credence::target::x86_64::runtime {

using Instructions = x86_64::assembly::Instructions;

using library_t = common::runtime::library_t;
using address_t = common::runtime::address_t<assembly::Register>;
using library_list_t = common::runtime::library_list_t;
using library_register_t = memory::registers::general_purpose;
using library_arguments_t =
    common::runtime::library_arguments_t<assembly::Register>;

using X8664_Library_Call_Inserter =
    common::runtime::Library_Call_Inserter<assembly::Register,
        assembly::Stack,
        assembly::Instructions>;

std::pair<library_register_t, library_register_t>
get_argument_general_purpose_registers();

struct Library_Call_Inserter : public X8664_Library_Call_Inserter
{
    explicit Library_Call_Inserter(memory::Memory_Access& accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
        dword_registers_ = { assembly::Register::r9d,
            assembly::Register::r8d,
            assembly::Register::ecx,
            assembly::Register::edx,
            assembly::Register::esi,
            assembly::Register::edi };
        qword_registers_ = { assembly::Register::r9,
            assembly::Register::r8,
            assembly::Register::rcx,
            assembly::Register::rdx,
            assembly::Register::rsi,
            assembly::Register::rdi };

        xmm_registers_ = { assembly::Register::xmm7,
            assembly::Register::xmm6,
            assembly::Register::xmm5,
            assembly::Register::xmm4,
            assembly::Register::xmm3,
            assembly::Register::xmm2,
            assembly::Register::xmm1,
            assembly::Register::xmm0 };
    }

    assembly::Register get_available_standard_library_register(
        std::deque<assembly::Register>& available_registers,
        common::memory::Locals& argument_stack,
        std::size_t index) override;

    void make_library_call(Instructions& instructions,
        std::string_view syscall_function,
        common::memory::Locals& locals,
        library_arguments_t const& arguments) override;

    bool is_address_device_pointer_to_buffer(address_t& address,
        ir::object::Object_PTR& table,
        std::shared_ptr<assembly::Stack>& stack) override;

    void insert_argument_instructions_standard_library_function(
        Register storage,
        Instructions& instructions,
        std::string_view arg_type,
        address_t const& argument);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame stack_frame_;
    library_register_t dword_registers_;
    library_register_t qword_registers_;
    library_register_t xmm_registers_;
};

} // namespace credence::target::x86_64::runtime
