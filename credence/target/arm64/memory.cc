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

#include "assembly.h"                        // for Register, Operand_Size
#include "credence/target/common/flags.h"    // for Flag_Accessor, Instruct...
#include "credence/target/common/memory.h"   // for align_up_to, is_vector_...
#include "credence/target/common/types.h"    // for Stack_Offset, Table_Poi...
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for credence_assert, creden...
#include <credence/ir/object.h>              // for LValue, Object, get_rva...
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/assembly.h> // for direct_immediate, make_...
#include <credence/types.h>                  // for get_size_from_rvalue_da...
#include <credence/util.h>                   // for INLINE_DEBUG, is_variant
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for App, pattern, app, Patt...
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get
#include <utility>                           // for pair
#include <variant>                           // for get, variant, monostate

namespace credence::target::arm64::memory {

namespace m = matchit;

/**
 * @brief Get a general purpose accumulator including during temporary expansion
 */
Register Memory_Accessor::get_accumulator_with_rvalue_context(
    Storage const& device)
{
    auto size = device_accessor.get_word_size_from_storage(device);
    return table_accessor.next_ir_instruction_is_temporary() and
                   not table_accessor.last_ir_instruction_is_assignment()
               ? device_accessor.get_second_register_for_binary_operand(size)
               : accumulator_accessor.get_accumulator_register_from_size(size);
}

Register Memory_Accessor::get_accumulator_with_rvalue_context(
    assembly::Operand_Size size)
{
    return table_accessor.next_ir_instruction_is_temporary() and
                   not table_accessor.last_ir_instruction_is_assignment()
               ? device_accessor.get_second_register_for_binary_operand(size)
               : accumulator_accessor.get_accumulator_register_from_size(size);
}

namespace detail {

/**
 * @brief Unary template instantiation for matchit from credence/types.h
 */
constexpr bool arm64_is_binary_datatype_expression(Immediate const& immediate)
{
    auto rvalue = type::get_value_from_rvalue_data_type(immediate);

    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = type::from_rvalue_binary_expression(rvalue);
    if (!type::is_rvalue_data_type(std::get<0>(test)))
        return false;
    if (!type::is_rvalue_data_type(std::get<1>(test)))
        return false;
    return true;
}

/**
 * @brief Unary template instantiation for matchit from credence/types.h
 */
constexpr bool arm64_is_temporary_datatype_binary_expression(
    Immediate const& immediate)
{
    return type::is_temporary_datatype_binary_expression(
        type::get_value_from_rvalue_data_type(immediate));
}

/**
 * @brief Get the accumulator register from size
 */
Register Accumulator_Accessor::get_accumulator_register_from_size(
    assembly::Operand_Size size)
{
    namespace m = matchit;
    if (*signal_register_ != Register::w0) {
        auto designated = *signal_register_;
        *signal_register_ = Register::w0;
        return designated;
    }
    return m::match(size)(
        m::pattern |
            assembly::Operand_Size::Doubleword = [&] { return Register::x8; },
        m::pattern | assembly::Operand_Size::Halfword =
            [&] { return Register::w8; }, // No 16-bit direct
        m::pattern | assembly::Operand_Size::Byte =
            [&] { return Register::w8; }, // No 8-bit direct
        m::pattern | m::_ = [&] { return Register::w8; });
}

/**
 * @brief Get a storage device for a binary expression operand
 */
Device_Accessor::Device Device_Accessor::get_operand_rvalue_device(
    RValue const& rvalue)
{
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);
    else if (is_lvalue_allocated_in_memory(rvalue))
        return get_device_by_lvalue(rvalue);
    else {
        auto frame = stack_frame_.get_stack_frame();
        auto size = assembly::get_operand_size_from_size(
            address_accessor_.table_->lvalue_size_at_temporary_object_address(
                rvalue, frame));
        if (size == Operand_Size::Doubleword)
            return Register::x8;
        else
            return Register::w8;
    }
}

/**
 * @brief Gets the second register for a binary operand based on size
 */
Register Device_Accessor::get_second_register_for_binary_operand(
    Operand_Size size)
{
    if (size == Operand_Size::Doubleword)
        return Register::x23;
    else
        return Register::w23;
}

/**
 * @brief Gets the address and instructions for an lvalue from unary and vector
 * expressions
 */
Address_Accessor::Address
Address_Accessor::get_lvalue_address_and_from_unary_and_vectors(
    Address& instructions,
    LValue const& lvalue,
    std::size_t instruction_index,
    Device_Accessor& device_accessor)
{
    auto vector_accessor = Vector_Accessor{ table_ };
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);

    m::match(lvalue)(
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto storage = device_accessor.get_device_by_lvalue(
                    type::get_unary_rvalue_reference(lvalue));
                arm64_add__asm(
                    instructions.second, mov, Register::x26, storage);
                flag_accessor_.set_instruction_flag(
                    common::flag::Indirect, instruction_index + 1);
                instructions.first = Register::x26;
            },
        m::pattern | m::app(is_global_vector, true) =
            [&] {
                credence_assert(table_->vectors.contains(lhs));
                auto vector = table_->vectors.at(lhs);
                if (table_->globals.is_pointer(lhs)) {
                    auto offset_storage =
                        vector_accessor.get_offset_address(lvalue, offset);
                    auto offset_arithmetic =
                        offset_storage.first == 0UL
                            ? lhs
                            : fmt::format("{}+{}", lhs, offset_storage.first);
                    offset_arithmetic =
                        fmt::format("[sp, {}]", offset_arithmetic);
                    instructions.first = common::assembly::make_array_immediate(
                        offset_arithmetic);
                }
            },
        m::pattern | m::app(is_vector_offset, true) =
            [&] {
                credence_assert(table_->vectors.contains(lhs));
                auto vector = table_->vectors.at(lhs);
                instructions.first =
                    stack_->get_stack_offset_from_table_vector_index(
                        lhs, offset, *vector);
            });

    return instructions;
}

/**
 * @brief Gets the ARM64 lvalue address and insertion instructions
 */
Address_Accessor::Address
Address_Accessor::get_arm64_lvalue_and_insertion_instructions(
    LValue const& lvalue,
    Device_Accessor& device_accessor,
    std::size_t instruction_index,
    INLINE_DEBUG)
{
    Address instructions{ Register::wzr, {} };

    get_lvalue_address_and_from_unary_and_vectors(
        instructions, lvalue, instruction_index, device_accessor);

    if (std::get<Register>(instructions.first) == Register::wzr)
        instructions.first =
            device_accessor.get_device_by_lvalue(lvalue, source);

    return instructions;
}

/**
 * @brief Calculates the total size of the address table
 */
Size Device_Accessor::get_size_of_address_table()
{
    auto size = 0UL;
    for (auto const& device : address_table) {
        size +=
            static_cast<std::size_t>(get_word_size_from_storage(device.second));
    }
    return size;
}

/**
 * @brief Saves registers to the stack before an instruction jump
 */
void Device_Accessor::save_and_allocate_before_instruction_jump(
    assembly::Instructions& instructions)
{
    local_size = common::memory::align_up_to(get_size_of_address_table(), 16);
    arm64_add__asm(instructions, sub, sp, sp, u32_int_immediate(local_size));
    Size size_at = 0;
    for (auto const& device : address_table) {
        auto stack_offset = direct_immediate(fmt::format("[sp, #{}]", size_at));
        arm64_add__asm(instructions, str, device.second, stack_offset);
        size_at +=
            assembly::get_size_from_register(std::get<Register>(device.second));
    }
}

/**
 * @brief Restores registers from the stack after an instruction jump
 */
void Device_Accessor::restore_and_deallocate_after_instruction_jump(
    assembly::Instructions& instructions)
{
    Size size_at = 0;
    for (auto const& device : address_table) {
        auto stack_offset = direct_immediate(fmt::format("[sp, #{}]", size_at));
        arm64_add__asm(instructions, str, device.second, stack_offset);
        size_at +=
            assembly::get_size_from_register(std::get<Register>(device.second));
    }
    arm64_add__asm(instructions, add, sp, sp, u32_int_immediate(local_size));
    local_size = 0;
}

/**
 * @brief Gets an available register for temporary storage
 */
Register Device_Accessor::get_available_storage_register(
    assembly::Operand_Size size)
{
    auto registers = register_accessor_.get_register_list_by_size(size);
    register_id.insert(id_index);
    credence_assert(registers.size() > id_index);
    Register storage = registers.at(id_index);
    address_table.insert("_", storage);
    id_index++;
    return storage;
}

/**
 * @brief Allocates a word or doubleword register for an lvalue
 */
void Device_Accessor::set_word_or_doubleword_register(LValue const& lvalue,
    assembly::Operand_Size size)
{
    auto registers = register_accessor_.get_register_list_by_size(size);
    register_id.insert(id_index);
    credence_assert(registers.size() > id_index);
    Register storage = registers.at(id_index);
    address_table.insert(lvalue, storage);
    id_index++;
}

/**
 * @brief Gets the storage device for an lvalue
 */
Device_Accessor::Device Device_Accessor::get_device_by_lvalue(
    LValue const& lvalue,
    INLINE_DEBUG)
{
    credence_assert_message_trace(
        is_lvalue_allocated_in_memory(lvalue), lvalue, source);
    return address_table.at(lvalue);
}

/**
 * @brief Allocates a register or stack space for a given lvalue
 */
void Device_Accessor::insert_lvalue_to_device(LValue const& lvalue,
    INLINE_DEBUG)
{
    auto& table_ = address_accessor_.table_;
    auto frame = stack_frame_.get_stack_frame();
    credence_assert(frame->locals.is_defined(lvalue));

    if (is_lvalue_allocated_in_memory(lvalue))
        return;

    auto rvalue = ir::object::get_rvalue_at_lvalue_object_storage(
        lvalue, frame, table_->vectors, DEBUG_SOURCE);
    auto size = get_size_from_rvalue_data_type(lvalue, rvalue);

    credence_assert_message(assembly::is_valid_size(size), lvalue);

    auto operand = static_cast<assembly::Operand_Size>(size);

    switch (operand) {
        case assembly::Operand_Size::Empty:
        case assembly::Operand_Size::Byte:
        case assembly::Operand_Size::Halfword:
            stack_->set_address_from_size(lvalue, operand);
            break;
        case assembly::Operand_Size::Word:
        case assembly::Operand_Size::Doubleword: {
            if (register_id.size() == 9) {
                stack_->set_address_from_size(lvalue, operand);
                address_table.insert(lvalue, stack_->get(lvalue).first);
            } else
                set_word_or_doubleword_register(lvalue, operand);
        } break;
    }
}

/**
 * @brief Allocates storage space for a vector offset
 */
void Device_Accessor::set_vector_offset_to_storage_space(LValue const& lvalue)
{
    auto offset_lvalue =
        fmt::format("__{}_vector_offset_{}", lvalue, ++vector_index);
    insert_lvalue_to_device(offset_lvalue);
}

/**
 * @brief Gets the size of an rvalue reference from its type or storage device
 */
Size Device_Accessor::get_size_from_rvalue_reference(RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();

    if (!type::is_rvalue_data_type_a_type(rvalue, "word"))
        return type::get_size_from_rvalue_data_type(rvalue);
    if (is_lvalue_allocated_in_memory(rvalue)) {
        auto lvalue_reference = get_device_by_lvalue(rvalue);
        if (is_variant(Register, lvalue_reference))
            return assembly::get_size_from_register(
                std::get<Register>(lvalue_reference));
        if (is_variant(Immediate, lvalue_reference))
            return get_size_from_rvalue_data_type(
                rvalue, std::get<Immediate>(lvalue_reference));
        if (is_variant(common::Stack_Offset, lvalue_reference))
            return assembly::get_size_from_operand_size(
                stack_->get_operand_size_from_offset(
                    std::get<common::Stack_Offset>(lvalue_reference)));
    }
    credence_error("unreachable");
    return 0UL;
}

/**
 * @brief Gets the size of a temporary or binary temporary rvalue data type
 */
Size Device_Accessor::get_size_from_rvalue_data_type(LValue const& lvalue,
    Immediate const& rvalue)
{
    auto& table_ = address_accessor_.table_;
    auto frame = stack_frame_.get_stack_frame();

    if (!type::is_rvalue_data_type_a_type(rvalue, "word"))
        return type::get_size_from_rvalue_data_type(rvalue);

    return m::match(rvalue)(
        m::pattern |
            m::app(arm64_is_temporary_datatype_binary_expression, true) =
            [&] {
                return table_->get_size_of_temporary_binary_rvalue(
                    type::get_value_from_rvalue_data_type(rvalue), frame);
            },
        m::pattern | m::app(arm64_is_binary_datatype_expression, true) =
            [&] {
                auto [left, right, _] =
                    type::from_rvalue_binary_expression(rvalue);
                return type::get_size_from_rvalue_data_type(left);
            },
        m::pattern | m::app(type::is_unary_data_type_expression, true) =
            [&] {
                auto lvalue_reference = type::get_unary_rvalue_reference(
                    type::get_value_from_rvalue_data_type(rvalue));
                if (is_lvalue_allocated_in_memory(lvalue_reference))
                    return assembly::get_size_from_operand_size(
                        get_word_size_from_lvalue(lvalue_reference));
                else
                    return type::get_size_from_rvalue_data_type(
                        ir::object::get_rvalue_at_lvalue_object_storage(
                            lvalue_reference, frame, table_->vectors));
            },
        m::pattern | m::_ =
            [&] {
                auto immediate =
                    ir::object::get_rvalue_at_lvalue_object_storage(
                        lvalue, frame, table_->vectors);
                return type::get_size_from_rvalue_data_type(immediate);
            }

    );
}

/**
 * @brief Checks if an lvalue is allocated in a register or on the stack
 */
bool Device_Accessor::is_lvalue_allocated_in_memory(LValue const& lvalue)
{
    return address_table.contains(lvalue) or stack_->is_allocated(lvalue);
}

/**
 * @brief Checks if the storage size is a doubleword
 */
bool Device_Accessor::is_doubleword_storage_size(
    assembly::Storage const& storage)
{
    auto result{ false };
    auto frame = stack_frame_.get_stack_frame();
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](common::Stack_Offset const& s) {
                       result = stack_->get_operand_size_from_offset(s) ==
                                assembly::Operand_Size::Doubleword;
                   },
                   [&](Register const& s) {
                       result = assembly::is_doubleword_register(s);
                   },
                   [&](Immediate const& s) {
                       result = type::is_rvalue_data_type_string(s) or
                                assembly::is_immediate_relative_address(s);
                   },
               },
        storage);

    return result;
}

} // namespace detail

} // namespace credence::target::arm64::memory
