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

#include "memory.h"

#include "assembly.h"                        // for Register, is_immediate_...
#include "credence/target/common/flags.h"    // for Flag_Accessor, Instruct...
#include "credence/target/common/memory.h"   // for is_vector_offset
#include "credence/target/common/types.h"    // for Stack_Offset
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for throw_compiletime_error
#include <credence/ir/object.h>              // for Object, LValue, RValue
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/assembly.h> // for make_direct_immediate
#include <credence/types.h>                  // for from_decay_offset, from...
#include <credence/util.h>                   // for is_numeric, overload
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for matchit
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <utility>                           // for pair
#include <variant>                           // for variant, monostate, visit

#define credence_current_location std::source_location::current()

namespace credence::target::x86_64::memory {

namespace m = matchit;

namespace detail {

/**
 * @brief Get a storage device for an binary expression operand
 */
assembly::Storage Register_Accessor::get_register_for_binary_operator(
    RValue const& rvalue,
    Stack_Pointer& stack)
{
    // argc and argc storage resolution
    if (rvalue == "argc")
        return common::assembly::make_direct_immediate("[r15]");
    if (type::from_lvalue_offset(rvalue) == "argv") {
        auto offset = type::from_decay_offset(rvalue);
        if (!util::is_numeric(offset) and
            not address_accessor.is_lvalue_storage_type(offset, "int"))
            throw_compiletime_error(
                fmt::format(
                    "invalid argv access, argv has malformed offset '{}'",
                    offset),
                rvalue);
        auto offset_integer = type::integral_from_type_ulint(offset) + 1;
        return common::assembly::make_direct_immediate(
            fmt::format("[r15 + 8 * {}]", offset_integer));
    }
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (stack->contains(rvalue))
        return stack->get(rvalue).first;

    return assembly::Register::rax;
}

/**
 * @brief Get an available register storage device, use the stack if none
 * available
 */
assembly::Storage Register_Accessor::get_available_register(
    assembly::Operand_Size size,
    Stack_Pointer& stack)
{
    auto& registers =
        size == Operand_Size::Qword ? available_qword : available_dword;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return stack->allocate(size);
    }
}

/**
 * @brief Get the storage device of an lvalue, and its insertion instructions
 */
Address_Accessor::Address
Address_Accessor::get_lvalue_address_and_insertion_instructions(
    LValue const& lvalue,
    std::size_t instruction_index,
    bool use_prefix)
{
    auto vector_accessor = Vector_Accessor{ table_ };
    assembly::Instruction_Pair instructions{ Register::eax, {} };
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);

    if (type::is_dereference_expression(lvalue)) {
        auto storage =
            stack_->get(type::get_unary_rvalue_reference(lvalue)).first;
        x8664_add__asm(instructions.second, mov, Register::rax, storage);
        flag_accessor_.set_instruction_flag(
            common::flag::Indirect, instruction_index + 1);
        instructions.first = Register::rax;
    } else if (is_global_vector(lhs)) {
        auto vector = table_->vectors.at(lhs);
        if (table_->globals.is_pointer(lhs)) {
            auto rip_storage =
                vector_accessor.get_offset_address(lvalue, offset);
            auto prefix = storage_prefix_from_operand_size(rip_storage.second);
            auto rip_arithmetic =
                rip_storage.first == 0UL
                    ? lhs
                    : fmt::format("{}+{}", lhs, rip_storage.first);
            rip_arithmetic =
                use_prefix
                    ? fmt::format("{} [rip + {}]", prefix, rip_arithmetic)
                    : fmt::format("[rip + {}]", rip_arithmetic);
            instructions.first =
                common::assembly::make_array_immediate(rip_arithmetic);
        }
    } else if (is_vector_offset(lvalue)) {
        credence_assert(table_->vectors.contains(lhs));
        auto vector = table_->vectors.at(lhs);
        instructions.first = stack_->get_stack_offset_from_table_vector_index(
            lhs, offset, *vector);

    } else if (stack_->is_allocated(lvalue)) {
        instructions.first = stack_->get(lvalue).first;
    }
    return instructions;
}

/**
 * @brief Check if the type of an lvalue, even if the lvalue is undefined
 */
/**
 * @brief Check if the storage device is a QWord size in address space
 */
bool Address_Accessor::is_qword_storage_size(assembly::Storage const& storage,
    Stack_Frame& stack_frame)
{
    auto result{ false };
    auto frame = stack_frame.get_stack_frame();
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](common::Stack_Offset const& s) {
                result = stack_->get_operand_size_from_offset(s) ==
                         Operand_Size::Qword;
            },
            [&](Register const& s) { result = assembly::is_qword_register(s); },
            [&](Immediate const& s) {
                result = type::is_rvalue_data_type_string(s) or
                         assembly::is_immediate_r15_address_offset(s) or
                         assembly::is_immediate_rip_address_offset(s);
            },
        },
        storage);

    return result;
}

/**
 * @brief Get the address on the stack from an lvalue
 */
// get_lvalue_address_from_stack moved to shared inc

} // namespace detail

} // namespace memory