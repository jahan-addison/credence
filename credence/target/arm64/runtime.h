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
#include <initializer_list>                     // for initializer_list
#include <string>                               // for basic_string
#include <string_view>                          // for string_view
#include <utility>                              // for pair

namespace credence::target::arm64::runtime {

using Instructions = arm64::assembly::Instructions;

using library_t = common::runtime::library_t;
using address_t = common::runtime::address_t<assembly::Register>;
using library_list_t = common::runtime::library_list_t;
using library_register_t = memory::registers::general_purpose;
using library_arguments_t =
    common::runtime::library_arguments_t<assembly::Register>;

using ARM64_Library_Call_Inserter =
    common::runtime::Library_Call_Inserter<assembly::Register,
        assembly::Stack,
        assembly::Instructions>;

std::pair<library_register_t, library_register_t>
get_argument_general_purpose_registers();

struct Library_Call_Inserter : public ARM64_Library_Call_Inserter
{
    explicit Library_Call_Inserter(memory::Memory_Access& accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
        word_registers_ = assembly::WORD_REGISTER;
        dword_registers_ = assembly::DOUBLEWORD_REGISTER;
        float_registers_ = assembly::FLOAT_REGISTER;
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
        memory::Stack_Pointer& stack) override;

    void insert_argument_instructions_standard_library_function(
        Register storage,
        Instructions& instructions,
        std::string_view arg_type,
        address_t const& argument);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame stack_frame_;
    library_register_t word_registers_;
    library_register_t dword_registers_;
    library_register_t float_registers_;
};

} // namespace credence::target::arm64::runtime
