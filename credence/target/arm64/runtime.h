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

/****************************************************************************
 *
 * ARM64 Runtime and Standard Library
 *
 * See common/runtime.h for details
 *
 * Runtime function invocation to the standard library and via the ARM64 PCS
 * calling convention. Arguments passed in registers: x0-x7 or register variant
 * depending on rvalue type then uses the stack. Return value in x0. x30 (lr)
 * holds return address.
 *
 * Example - calling printf:
 *
 *   B code:    printf("Value: %d\n", x);
 *
 *   adrp x0, ._L_str1__@PAGE       ; format string in x0
 *   add x0, x0, ._L_str1__@PAGEOFF
 *   mov x1, x9                      ; x from register x9
 *   bl printf                       ; from stdlib
 *
 *****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x6  = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x15      = Second data section register
 *   x7       = multiplication scratch register
 *   x8       = The default "accumulator" register for expression expansion
 *   x10      = The stack move register; additional scratch register
 *   x9 - x18 = If there are no function calls in a stack frame, local scope
 *             variables are stored in x9-x18, after which the stack is used
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

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

struct Library_Call_Inserter : public ARM64_Library_Call_Inserter
{
    explicit Library_Call_Inserter(memory::Memory_Access& accessor,
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

    bool try_insert_operand_from_argv_rvalue(Instructions& instructions,
        Register argument_storage,
        unsigned int index);

    void make_library_call(Instructions& instructions,
        std::string_view syscall_function,
        library_arguments_t const& arguments) override;

    bool is_address_device_pointer_to_buffer(address_t& address) override;

    void insert_argument_instructions_standard_library_function(
        Register storage,
        Instructions& instructions,
        address_t const& argument,
        unsigned int index);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame stack_frame_;
    library_register_t word_registers_;
    library_register_t dword_registers_;
    library_register_t float_registers_;
    library_register_t double_registers_;
};

} // namespace credence::target::arm64::runtime
