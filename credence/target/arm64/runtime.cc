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

#include "assembly.h"                        // for Register, O_NUL, operat...
#include "stack.h"                           // for Stack
#include <array>                             // for get, array
#include <credence/error.h>                  // for credence_assert
#include <credence/target/arm64/memory.h>    // for Address_Accessor, Register
#include <credence/target/common/assembly.h> // for get_storage_as_string
#include <credence/target/common/types.h>    // for Immediate, Stack_Offset
#include <credence/types.h>                  // for get_type_from_rvalue_da...
#include <credence/util.h>                   // for contains, __source__
#include <deque>                             // for deque
#include <stdexcept>                         // for out_of_range
#include <variant>                           // for get, monostate, visit

namespace credence::target::arm64::runtime {

/**
 * @brief A compiletime check on a buffer allocation in a storage device
 */
bool Library_Call_Inserter::is_address_device_pointer_to_buffer(
    address_t& address,
    ir::object::Object_PTR& table,
    memory::Stack_Pointer& stack)
{
    auto stack_frame = table->get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](common::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                auto type = type::get_type_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
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
 * @brief Get registers for argument storage
 *
 * ARM64 AAPCS64 uses:
 * - x0-x7 for integer/pointer arguments
 * - v0-v7 (d0-d7 for doubles, s0-s7 for floats) for floating point
 */
assembly::Register
Library_Call_Inserter::get_available_standard_library_register(
    std::deque<assembly::Register>& available_registers,
    common::memory::Locals& argument_stack,
    std::size_t index)
{
    auto address_accessor = accessor_->address_accessor;
    Register storage = Register::w0;
    try {
        if (address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "float") or
            address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "double")) {
            storage = float_registers_.back();
            float_registers_.pop_back();
        } else {
            storage = available_registers.back();
        }
    } catch ([[maybe_unused]] std::out_of_range const& e) {
        storage = available_registers.back();
    }
    return storage;
}

/**
 * @brief Prepare registers for argument operand storage
 *
 * ARM64 uses:
 * - adr/ldr for loading addresses
 * - fmov/ldr for floating point
 * - mov for general purpose
 */
void Library_Call_Inserter::
    insert_argument_instructions_standard_library_function(Register storage,
        Instructions& instructions,
        std::string_view arg_type,
        address_t const& argument)
{
    auto* signal_register = accessor_->register_accessor.signal_register;
    if (arg_type == "string") {
        // Use adr for PC-relative address loading
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::adr, storage, argument, assembly::O_NUL });
    } else if (arg_type == "float") {
        // ARM64 uses ldr for loading floats into SIMD registers
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::ldr, storage, argument, assembly::O_NUL });
    } else if (arg_type == "double") {
        // ARM64 uses ldr for loading doubles into SIMD registers
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::ldr, storage, argument, assembly::O_NUL });
    } else {
        if (storage == assembly::Register::x0 and
            *signal_register == assembly::Register::x9) {
            *signal_register = assembly::Register::w0;
            instructions.emplace_back(
                assembly::Instruction{ assembly::Mnemonic::mov,
                    storage,
                    assembly::Register::x9,
                    assembly::O_NUL });
            return;
        }
        if (is_variant(Register, argument) and
            assembly::is_word_register(std::get<Register>(argument))) {
            auto storage_dword =
                assembly::get_word_register_from_doubleword(storage);
            instructions.emplace_back(
                assembly::Instruction{ assembly::Mnemonic::mov,
                    storage_dword,
                    argument,
                    assembly::O_NUL });
        } else
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::mov, storage, argument, assembly::O_NUL });
    }
}

/**
 * @brief General purpose register stack in ARM64 AAPCS64 order
 *
 * ARM64 calling convention:
 * - x0-x7 for integer/pointer arguments (64-bit)
 * - w0-w7 for 32-bit integer arguments
 * - v0-v7 for floating point arguments
 */
std::pair<memory::registers::general_purpose,
    memory::registers::general_purpose>
get_argument_general_purpose_registers()
{
    std::deque<assembly::Register> d = { assembly::Register::x7,
        assembly::Register::x6,
        assembly::Register::x5,
        assembly::Register::x4,
        assembly::Register::x3,
        assembly::Register::x2,
        assembly::Register::x1,
        assembly::Register::x0 };

    std::deque<assembly::Register> w = { assembly::Register::w7,
        assembly::Register::w6,
        assembly::Register::w5,
        assembly::Register::w4,
        assembly::Register::w3,
        assembly::Register::w2,
        assembly::Register::w1,
        assembly::Register::w0 };

    return std::make_pair(d, w);
}

/**
 * @brief Create the instructions for a standard library call
 */
void Library_Call_Inserter::make_library_call(Instructions& instructions,
    std::string_view syscall_function,
    common::memory::Locals& locals,
    library_arguments_t const& arguments)
{
    credence_assert(common::runtime::library_list.contains(syscall_function));
    auto [arg_size] = common::runtime::library_list.at(syscall_function);

    auto address_space = accessor_->address_accessor;

    library_call_argument_check(syscall_function, arguments, arg_size);

    auto [doubleword_storage, word_storage] =
        get_argument_general_purpose_registers();

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);
        Register storage{};
        std::string arg_type =
            locals.size() > i
                ? type::get_type_from_rvalue_data_type(locals.at(i))
                : "";

        auto float_size = float_registers_.size();

        if (address_space.is_doubleword_storage_size(arg, stack_frame_))
            storage = get_available_standard_library_register(
                doubleword_storage, locals, i);
        else
            storage = get_available_standard_library_register(
                word_storage, locals, i);

        insert_argument_instructions_standard_library_function(
            storage, instructions, arg_type, arg);
        if (float_size == float_registers_.size()) {
            doubleword_storage.pop_back();
            word_storage.pop_back();
        }
    }

    // ARM64 uses bl (branch with link) for function calls
    auto call_immediate =
        common::assembly::make_array_immediate(syscall_function);
    instructions.emplace_back(assembly::Instruction{ assembly::Mnemonic::bl,
        call_immediate,
        assembly::O_NUL,
        assembly::O_NUL });
}

} // namespace credence::target::arm64::runtime
