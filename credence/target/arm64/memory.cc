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

#include "assembly.h"                        // for Register, is_doubleword...
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for credence_assert
#include <credence/ir/object.h>              // for Object, LValue, RValue
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/assembly.h> // for make_array_immediate
#include <credence/types.h>                  // for is_rvalue_data_type_string
#include <credence/util.h>                   // for overload
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for matchit
#include <string>                            // for basic_string, char_traits
#include <utility>                           // for pair
#include <variant>                           // for variant, monostate, visit

namespace credence::target::arm64::memory {

namespace m = matchit;

namespace detail {

/**
 * @brief Get an available register storage device, use the stack if none
 * available
 */
assembly::Storage Register_Accessor::get_available_register(
    assembly::Operand_Size size,
    Stack_Pointer& stack)
{
    auto& registers = size == assembly::Operand_Size::Doubleword
                          ? d_size_registers
                          : w_size_registers;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return stack->allocate(size);
    }
}

/**
 * @brief Get a storage device for an binary expression operand
 */
assembly::Storage Register_Accessor::get_register_for_binary_operator(
    RValue const& rvalue,
    Stack_Pointer& stack)
{
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (stack->contains(rvalue))
        return stack->get(rvalue).first;

    return assembly::Register::w0;
}

/**
 * @brief Get the storage device of an lvalue, and its insertion instructions
 */
Address_Accessor::Address Address_Accessor::get_lvalue_address_and_instructions(
    LValue const& lvalue,
    std::size_t instruction_index,
    [[maybe_unused]] bool use_prefix)
{
    using namespace assembly;
    auto vector_accessor = Vector_Accessor{ table_ };
    assembly::Instruction_Pair instructions{ Register::w0, {} };
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);

    if (type::is_dereference_expression(lvalue)) {
        auto storage =
            stack_->get(type::get_unary_rvalue_reference(lvalue)).first;
        arm64_add__asm(instructions.second, mov, Register::x0, storage);
        flag_accessor_.set_instruction_flag(
            common::flag::Indirect, instruction_index + 1);
        instructions.first = Register::x0;
    } else if (is_global_vector(lhs)) {
        auto vector = table_->vectors.at(lhs);
        if (table_->globals.is_pointer(lhs)) {
            auto offset_storage =
                vector_accessor.get_offset_address(lvalue, offset);
            auto offset_arithmetic =
                offset_storage.first == 0UL
                    ? lhs
                    : fmt::format("{}+{}", lhs, offset_storage.first);
            offset_arithmetic = fmt::format("[sp, {}]", offset_arithmetic);
            instructions.first =
                common::assembly::make_array_immediate(offset_arithmetic);
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

} // namespace detail

} // namespace credence::target::arm64::memory