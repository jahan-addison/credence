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
#include "flags.h"                           // for flags
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

namespace credence::target::arm64::memory::detail {

namespace m = matchit;

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
 * @brief Get a storage device for an binary expression operand
 */
Device_Accessor::Device Device_Accessor::get_device_for_binary_operand(
    RValue const& rvalue)
{
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (is_lvalue_allocated_in_memory(rvalue))
        return get_device_by_lvalue(rvalue);

    return assembly::Register::w8;
}

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

Address_Accessor::Address
Address_Accessor::get_arm64_lvalue_and_insertion_instructions(
    LValue const& lvalue,
    Device_Accessor& device_accessor,
    std::size_t instruction_index)
{
    Address instructions{ Register::wzr, {} };

    get_lvalue_address_and_from_unary_and_vectors(
        instructions, lvalue, instruction_index, device_accessor);

    if (std::get<Register>(instructions.first) == Register::wzr)
        instructions.first = device_accessor.get_device_by_lvalue(lvalue);

    return instructions;
}

Size Device_Accessor::get_size_of_address_table(Stack_Frame& stack_frame)
{
    auto size = 0UL;
    for (auto const& device : address_table) {
        size += static_cast<std::size_t>(
            get_word_size_from_storage(device.second, stack_frame));
    }
    return size;
}

void Device_Accessor::save_and_allocate_before_instruction_jump(
    assembly::Instructions& instructions,
    Stack_Frame& stack_frame)
{
    local_size =
        common::memory::align_up_to(get_size_of_address_table(stack_frame), 16);
    arm64_add__asm(instructions, sub, sp, sp, u32_int_immediate(local_size));
    Size size_at = 0;
    for (auto const& device : address_table) {
        auto stack_offset = direct_immediate(fmt::format("[sp, #{}]", size_at));
        arm64_add__asm(instructions, str, device.second, stack_offset);
        size_at +=
            assembly::get_size_from_register(std::get<Register>(device.second));
    }
}

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

Device_Accessor::Device Device_Accessor::get_device_by_lvalue(
    LValue const& lvalue)
{
    credence_assert(is_lvalue_allocated_in_memory(lvalue));
    return address_table.at(lvalue);
}

void Device_Accessor::insert_lvalue_to_device(LValue const& lvalue,
    Stack_Frame& stack_frame)
{
    auto& table_ = address_accessor_.table_;
    auto frame = stack_frame.get_stack_frame();
    credence_assert(frame->locals.is_defined(lvalue));

    if (is_lvalue_allocated_in_memory(lvalue))
        return;

    auto rvalue = ir::object::get_rvalue_at_lvalue_object_storage(
        lvalue, frame, table_->vectors, __source__);

    auto size = get_size_from_temporary_rvalue_data_type(lvalue, rvalue, frame);

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

void Device_Accessor::set_vector_offset_to_storage_space(LValue const& lvalue,
    Stack_Frame& stack_frame)
{
    auto offset_lvalue =
        fmt::format("__{}_vector_offset_{}", lvalue, ++vector_index);
    insert_lvalue_to_device(offset_lvalue, stack_frame);
}

Size Device_Accessor::get_size_from_temporary_rvalue_data_type(
    LValue const& lvalue,
    Immediate const& rvalue,
    ir::object::Function_PTR& frame)
{
    Size size = 0UL;
    auto& table_ = address_accessor_.table_;
    if (type::is_temporary_datatype_binary_expression(rvalue)) {
        auto temp_side = type::is_temporary_operand_binary_expression(rvalue);
        auto [left, right, _] = type::from_rvalue_binary_expression(rvalue);
        if (temp_side == "left")
            if (!type::is_rvalue_data_type(right))
                size = type::get_size_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
                        right, frame, table_->vectors, __source__));
            else
                size = type::get_size_from_rvalue_data_type(right);
        else {
            if (!type::is_rvalue_data_type(left))
                size = type::get_size_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
                        left, frame, table_->vectors, __source__));
            else
                size = type::get_size_from_rvalue_data_type(left);
        }
    } else if (type::is_binary_datatype_expression(rvalue)) {
        auto [left, right, _] = type::from_rvalue_binary_expression(rvalue);
        size = type::get_size_from_rvalue_data_type(left);
    } else {
        size = type::get_size_from_rvalue_data_type(
            ir::object::get_rvalue_at_lvalue_object_storage(
                lvalue, frame, table_->vectors, __source__));
    }
    return size;
}

bool Device_Accessor::is_lvalue_allocated_in_memory(LValue const& lvalue)
{
    return address_table.contains(lvalue) or stack_->is_allocated(lvalue);
}

bool Address_Accessor::is_doubleword_storage_size(
    assembly::Storage const& storage,
    Stack_Frame& stack_frame)
{
    auto result{ false };
    auto frame = stack_frame.get_stack_frame();
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

} // namespace credence::target::arm64::memory::detail