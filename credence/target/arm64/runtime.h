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
#include <credence/object.h>                    // for Object_Table
#include <credence/target/common/runtime.h>     // for address_t, library_a...
#include <credence/target/common/stack_frame.h> // for Locals
#include <cstddef>                              // for size_t
#include <initializer_list>                     // for initializer_list
#include <string>                               // for basic_string
#include <string_view>                          // for string_view
#include <utility>                              // for pair

/****************************************************************************
 * ARM64 Runtime and Standard Library
 *
 * Calls to the standard library under the ARM64 PCS calling convention. See
 * common/runtime.h for the functions. The arguments are in x0 to x7, or w0
 * to w7, s0 to s7, and d0 to d7 by their type, and the label of a function
 * on Darwin has a leading underscore.
 *
 *  Example:
 *
 *    printf("%d %d\n", compute(1, 2), y);
 *
 *  Becomes:
 *
 *    adrp x0, ._L_str1__@PAGE         // the format string
 *    add x0, x0, ._L_str1__@PAGEOFF
 *    ldr w1, [sp, #24]                // the return value of compute
 *    ldr w2, [sp, #20]                // y
 *    bl _printf
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x0 - x7  = The arguments of a call, and x0 the return value
 *   x6       = The data section address register, and the second operand
 *              of a binary expression in a temporary
 *      s6    = floating point
 *      d6    = double
 *   x7       = The scratch register of the right-hand-side of a binary
 *              expression
 *   x8       = The default "accumulator" register for expression expansion
 *   x9, x11 - x14, x16, x17
 *            = If there are no function calls in a stack frame, local scope
 *              variables are stored in these registers, after which the
 *              stack is used
 *   x10      = The stack move register, and the address register of a
 *              pointer on the stack
 *   x15      = The address of a vector element on the stack
 *   x18      = The platform register on Darwin, never used
 *   x19      = The argc, argv scratch register
 *
 *   A local whose address is taken, vectors, and vector offsets are always
 *   on the stack
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

/**
 * @brief Translate a call to a standard library function on ARM64
 *
 * Each argument takes the next argument register of its type, so the
 * register lists are consumed in order
 */
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
