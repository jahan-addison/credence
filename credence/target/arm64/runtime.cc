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

#include "runtime.h"

#include "assembly.h"                           // for Register, operator<<
#include "credence/target/common/flags.h"       // for Instruction_Flag
#include "credence/target/common/runtime.h"     // for library_list
#include "credence/target/common/stack_frame.h" // for Locals
#include "matchit.h"                            // for Or, Wildcard, pattern
#include "memory.h"                             // for Register, Address_Ac...
#include "stack.h"                              // for Stack
#include <array>                                // for get, array
#include <credence/error.h>                     // for credence_assert
#include <credence/object.h>                    // for get_rvalue_at_lvalue...
#include <credence/target/common/assembly.h>    // for get_storage_as_string
#include <credence/target/common/types.h>       // for Stack_Offset, Immediate
#include <credence/types.h>                     // for get_type_from_rvalue...
#include <credence/util.h>                      // for contains, sv, is_var...
#include <deque>                                // for deque
#include <fmt/format.h>                         // for format
#include <stdexcept>                            // for out_of_range
#include <variant>                              // for get, monostate, visit

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

/**
 * @brief Check at compile time that a storage device holds a buffer address
 *
 * A doubleword register, a stack address, a data section label, or a string
 * is a buffer. A local on the stack is a buffer when it is a string or
 * `null`, and an argument stored before its call when it is a doubleword.
 *
 *  Examples:
 *
 *    is_address_device_pointer_to_buffer(x9)          // true
 *    is_address_device_pointer_to_buffer(w9)          // false
 *    is_address_device_pointer_to_buffer(._L_str1__)  // true
 */
bool Library_Call_Inserter::is_address_device_pointer_to_buffer(
    address_t& address)
{
    auto& stack = accessor_->stack;
    auto& table = accessor_->table_accessor.get_table();
    auto& stack_frame = accessor_->get_frame_in_memory().get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](common::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                // an argument stored before its call is a doubleword
                if (common::memory::is_parameter(lvalue) or
                    type::is_temporary(lvalue)) {
                    is_buffer = stack->get(offset).second ==
                                assembly::Operand_Size::Doubleword;
                    return;
                }
                auto type = type::get_type_from_rvalue_data_type(
                    object::get_rvalue_at_lvalue_object_storage(
                        lvalue, stack_frame, table->vectors, __source__));
                is_buffer = type == "null" or type == "string";
            },
            [&](assembly::Register& device) {
                is_buffer = assembly::is_doubleword_register(device);
            },
            [&](common::Immediate& immediate) {
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "[sp]"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "sp,"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "._L"))
                    is_buffer = true;
                if (type::is_rvalue_data_type_string(immediate))
                    is_buffer = true;
            } },
        address);
    return is_buffer;
}

using library_register = std::deque<assembly::Register>;

/**
 * @brief Load an argument of a library call into its register
 *
 * A string is loaded from its page address, and a float or double from the
 * data section. Any other argument is moved from its device: a stack slot is
 * loaded, `argc` on Linux is loaded through its address in x19, and a word
 * register is moved to the word half of the argument register.
 */
void Library_Call_Inserter::
    insert_argument_instructions_standard_library_function(Register storage,
        Instructions& instructions,
        address_t const& argument,
        unsigned int index)
{
    auto* signal_register = accessor_->register_accessor.signal_register;
    namespace m = matchit;
    auto& locals = accessor_->get_frame_in_memory().argument_stack;
    auto rvalue = index >= locals.size() ? locals.at(0) : locals.at(index);
    // the type of a literal is only its storage while it is a label
    std::string arg_type =
        locals.size() > index and is_variant(Immediate, argument)
            ? type::get_type_from_rvalue_data_type(locals.at(index))
            : "";

    auto is_double_immediate = [&](RValue const& rvalue) {
        return rvalue.starts_with("._L_double") or arg_type == "double";
    };
    auto is_float_immediate = [&](RValue const& rvalue) {
        return rvalue.starts_with("._L_float") or arg_type == "float";
    };
    auto is_string = [&]([[maybe_unused]] RValue const& rvalue) {
        return arg_type == "string";
    };

    m::match(rvalue)(
        m::pattern | m::app(is_string, true) =
            [&] {
                auto immediate = type::get_value_from_rvalue_data_type(
                    std::get<Immediate>(argument));
                auto imm_1 = assembly::page_offset_upper_immediate(immediate);
                arm64_add__asm(instructions, adrp, storage, imm_1);
                auto imm_2 = assembly::page_offset_lower_immediate(immediate);
                arm64_add__asm(instructions, add, storage, storage, imm_2);
            },
        m::pattern | m::or_(m::app(is_double_immediate, true),
                         m::app(is_float_immediate, true)) =
            [&] {
                auto immediate = type::get_value_from_rvalue_data_type(
                    std::get<Immediate>(argument));
                auto imm_1 = assembly::page_offset_upper_immediate(immediate);
                arm64_add__asm(instructions, adrp, x8, imm_1);
                auto imm = assembly::load_offset_lower_immediate(
                    Register::x8, immediate);
                arm64_add__asm(instructions, ldr, storage, imm);
            },
        m::pattern | m::_ =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Argument, instructions.size());
                if (storage == assembly::Register::x6 and
                    *signal_register == assembly::Register::x6) {
                    *signal_register = assembly::Register::w0;
                    arm64_add__asm(instructions, mov, storage, x6);
                    return;
                }
                if (is_variant(common::Stack_Offset, argument) or
                    assembly::is_immediate_pc_address_offset(argument)) {
                    arm64_add__asm(instructions, ldr, storage, argument);
                } else if (is_variant(Register, argument) and
                           std::get<Register>(argument) == Register::x19) {
                    // argc, argv register
                    auto argc_imm = direct_immediate("[x19]");
                    arm64_add__asm(instructions, ldr, storage, argc_imm);

                } else if (is_variant(Register, argument) and
                           assembly::is_word_register(
                               std::get<Register>(argument))) {
                    auto storage_dword =
                        assembly::get_word_register_from_doubleword(storage);
                    arm64_add__asm(instructions, mov, storage_dword, argument);
                } else
                    arm64_add__asm(instructions, mov, storage, argument);
            });
}

/**
 * @brief Load an element of `argv` into an argument register
 *
 * An element is a doubleword from x19, see make_argv_offset_immediate
 *
 *  Example:
 *
 *    printf("%s", argv[1]);
 *
 *  Becomes, on Darwin:
 *
 *    ldr x1, [x19, #8]
 */
bool Library_Call_Inserter::try_insert_operand_from_argv_rvalue(
    Instructions& instructions,
    Register argument_storage,
    unsigned int index)
{
    try {
        auto& locals = accessor_->get_frame_in_memory().argument_stack;
        if (stack_frame_.symbol == "main" and
            type::from_lvalue_offset(locals.at(index)) == "argv") {
            auto offset = type::from_decay_offset(locals.at(index));
            auto offset_integer = type::integral_from_type_ulint(offset);
            auto argv_offset =
                assembly::make_argv_offset_immediate(offset_integer);
            auto storage =
                assembly::get_doubleword_register_from_word(argument_storage);
            arm64_add__asm(instructions, ldr, storage, argv_offset);
            return true;
        }
    } catch ([[maybe_unused]] std::out_of_range const& e) {
        return false;
    }
    return false;
}

/**
 * @brief Insert a call to a standard library function
 *
 * The argument count is checked, and each argument is loaded into the next
 * argument register of its size and type. An argument in x0 is the return
 * value of an earlier call, which was stored to its local, so it is loaded
 * from the local's slot.
 */
void Library_Call_Inserter::make_library_call(Instructions& instructions,
    std::string_view syscall_function,
    library_arguments_t const& arguments)
{
    credence_assert(common::runtime::library_list.contains(syscall_function));
    auto [arg_size] = common::runtime::library_list.at(syscall_function);
    auto& locals = accessor_->get_frame_in_memory().argument_stack;
    library_call_argument_check(syscall_function, arguments, arg_size);
    auto& address_accessor = accessor_->address_accessor;

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);
        Register storage = Register::wzr;

        try {
            if (memory::is_doubleword_storage_size(
                    arg, accessor_->stack, stack_frame_))
                storage = dword_registers_.back();
            if (address_accessor.is_lvalue_storage_type(locals.at(i), "float"))
                storage = float_registers_.back();
            if (address_accessor.is_lvalue_storage_type(locals.at(i), "double"))
                storage = double_registers_.back();
            if (storage == Register::wzr)
                storage = word_registers_.back();
        } catch (...) {
            storage = word_registers_.back();
        }

        if (try_insert_operand_from_argv_rvalue(instructions, storage, i)) {
            dword_registers_.pop_back();
            word_registers_.pop_back();
            float_registers_.pop_back();
            double_registers_.pop_back();
            continue;
        }

        if (is_variant(Register, arg) and
            (std::get<Register>(arg) == Register::x0 or
                std::get<Register>(arg) == Register::w0)) {
            auto lvalue = accessor_->table_accessor.get_last_lvalue_assignment(
                accessor_->table_accessor.get_index(),
                accessor_->get_frame_in_memory().get_stack_frame());
            auto stack_address = accessor_->stack->get(lvalue).first;
            arm64_add__asm(instructions, ldr, storage, stack_address);
        } else {
            insert_argument_instructions_standard_library_function(
                storage, instructions, arg, i);
        }
        dword_registers_.pop_back();
        word_registers_.pop_back();
        float_registers_.pop_back();
        double_registers_.pop_back();
    }
#if defined(__linux__)
    auto call_immediate =
        common::assembly::make_array_immediate(syscall_function);
#elif defined(__APPLE__) || defined(__bsdi__)
    auto call_immediate = common::assembly::make_array_immediate(
        fmt::format("_{}", syscall_function));
#elif defined(_WIN32) || defined(_WIN64)
    auto call_immediate =
        common::assembly::make_array_immediate(syscall_function);
#endif
    arm64_add__asm(instructions, bl, call_immediate);
}

} // namespace credence::target::arm64::runtime
