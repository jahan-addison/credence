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
#include "credence/ir/object.h"                 // for get_rvalue_at_lvalue...
#include "credence/target/common/flags.h"       // for Instruction_Flag
#include "credence/target/common/runtime.h"     // for library_list
#include "credence/target/common/stack_frame.h" // for Locals
#include "matchit.h"                            // for Or, Wildcard, pattern
#include "memory.h"                             // for Register, Address_Ac...
#include "stack.h"                              // for Stack
#include <array>                                // for get, array
#include <credence/error.h>                     // for credence_assert
#include <credence/target/common/assembly.h>    // for get_storage_as_string
#include <credence/target/common/types.h>       // for Stack_Offset, Immediate
#include <credence/types.h>                     // for get_type_from_rvalue...
#include <credence/util.h>                      // for contains, sv, is_var...
#include <deque>                                // for deque
#include <fmt/format.h>                         // for format
#include <stdexcept>                            // for out_of_range
#include <variant>                              // for get, monostate, visit

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

/**
 * @brief A compiletime check on a buffer allocation in a storage device
 */
bool Library_Call_Inserter::is_address_device_pointer_to_buffer(
    address_t& address)
{
    auto& stack = accessor_->stack;
    auto& table = accessor_->table_accessor.get_table();
    auto stack_frame = accessor_->get_frame_in_memory().get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](common::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                auto type = type::get_type_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
                        lvalue, stack_frame, table->get_vectors(), __source__));
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
 * @brief Prepare registers for argument operand storage
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
    std::string arg_type =
        locals.size() > index
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
                auto imm_1 =
                    direct_immediate(fmt::format("{}@PAGE", immediate));
                arm64_add__asm(instructions, adrp, storage, imm_1);
                auto imm_2 =
                    direct_immediate(fmt::format("{}@PAGEOFF", immediate));
                arm64_add__asm(instructions, add, storage, storage, imm_2);
            },
        m::pattern | m::or_(m::app(is_double_immediate, true),
                         m::app(is_float_immediate, true)) =
            [&] {
                auto immediate = type::get_value_from_rvalue_data_type(
                    std::get<Immediate>(argument));
                auto imm_1 =
                    direct_immediate(fmt::format("{}@PAGE", immediate));
                arm64_add__asm(instructions, adrp, x8, imm_1);
                auto imm = direct_immediate(
                    fmt::format("[x8, {}@PAGEOFF]", immediate));
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
                    assembly::is_immediate_pc_address_offset(argument))
                    arm64_add__asm(instructions, ldr, storage, argument);
                else if (is_variant(Register, argument) and
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
 * @brief Load the rvalue address from the offset in argv
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
            auto argv_address = accessor_->stack->get("argv").first;
            auto offset_integer = type::integral_from_type_ulint(offset);
            auto argv_offset =
                direct_immediate(fmt::format("[x10, #{}]", 8 * offset_integer));
            arm64_add__asm(instructions, ldr, x10, argv_address);
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
 * @brief Create the instructions for a standard library call
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
                accessor_->table_accessor.get_index());
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
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    auto call_immediate = common::assembly::make_array_immediate(
        fmt::format("_{}", syscall_function));
#endif
    arm64_add__asm(instructions, bl, call_immediate);
}

} // namespace credence::target::arm64::runtime
