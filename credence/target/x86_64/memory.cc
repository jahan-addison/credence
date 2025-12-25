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
#include "assembly.h"          // for get_operand_size_from_rvalue_datatype
#include "credence/error.h"    // for credence_assert
#include "credence/symbol.h"   // for Symbol_Table
#include "credence/values.h"   // for is_integer_string
#include "easyjson.h"          // for JSON
#include "stack.h"             // for Stack
#include <credence/ir/table.h> // for Table, Function, Vector
#include <credence/map.h>      // for Ordered_Map
#include <credence/types.h>    // for get_size_from_rvalue_data_type, get_v...
#include <credence/util.h>     // for is_numeric, sv
#include <deque>               // for deque
#include <fmt/format.h>        // for format
#include <source_location>     // for source_location
#include <string>              // for basic_string, char_traits, operator==
#include <string_view>         // for basic_string_view, operator==, string...
#include <tuple>               // for tuple
#include <utility>             // for make_pair, pair
#include <variant>             // for variant

#include <credence/ir/object.h>

#define credence_current_location std::source_location::current()

namespace credence::target::x86_64::memory {

namespace m = matchit;

/**
 * @brief Set the current stack frame from the address in the Table
 */
void Stack_Frame::set_stack_frame(Label const& name)
{
    accessor_->table_accessor.table_->set_stack_frame(name);
}

/**
 * @brief Get the current stack frame from the address in the Table by name
 */
Stack_Frame::IR_Function Stack_Frame::get_stack_frame(Label const& name) const
{
    return accessor_->table_accessor.table_->functions.at(name);
}

/**
 * @brief Get the current stack frame from the address in the Table
 */
Stack_Frame::IR_Function Stack_Frame::get_stack_frame() const
{
    return accessor_->table_accessor.table_->functions.at(symbol);
}

namespace detail {

/**
 * @brief Set an emission flag on the instruction at the index
 */
void Flag_Accessor::set_instruction_flag(flag::Instruction_Flag set_flag,
    unsigned int index)
{
    if (!instruction_flag.contains(index))
        instruction_flag[index] = set_flag;
    else
        instruction_flag[index] |= set_flag;
}

/**
 * @brief Unset an emission flag on the instruction at the index
 */

void Flag_Accessor::unset_instruction_flag(flag::Instruction_Flag set_flag,
    unsigned int index)
{
    if (instruction_flag.contains(index)) {
        auto flags = get_instruction_flags_at_index(index);
        if (flags & set_flag)
            set_instruction_flag(flags & ~set_flag, index);
    }
}

/**
 * @brief Set emission flags on the instruction at the index
 */
void Flag_Accessor::set_instruction_flag(flag::flags flags, unsigned int index)
{
    if (!instruction_flag.contains(index + 1))
        instruction_flag[index] = flags;
    else
        instruction_flag[index] |= flags;
}

/**
 * @brief Set the load address flag on an instruction to use `lea' instead of
 * 'mov'
 */
void Flag_Accessor::set_load_address_from_previous_instruction(
    assembly::Instructions const& instructions)
{
    credence_assert(instructions.size() > 1);
    auto last_instruction_flags = instruction_flag.at(instructions.size() - 1);
    if (instructions.size() > 1 and last_instruction_flags & flag::Load) {
        set_instruction_flag(
            last_instruction_flags &= ~flag::Load, instructions.size() - 1);
        set_instruction_flag(flag::Load, instructions.size());
    }
}

/**
 * @brief Get an available register storage device, use the stack if none
 * available
 */
assembly::Storage Register_Accessor::get_available_register(
    assembly::Operand_Size size,
    Stack_Pointer& stack)
{
    auto& registers = size == Operand_Size::Qword ? available_qword_register
                                                  : available_dword_register;
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
    // argc and argc storage resolution
    if (rvalue == "argc")
        return assembly::make_direct_immediate("[r15]");
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
        return assembly::make_direct_immediate(
            fmt::format("[r15 + 8 * {}]", offset_integer));
    }
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (stack->contains(rvalue))
        return stack->get(rvalue).first;

    return assembly::Register::rax;
}

/**
 * @brief Set the buffer size for compile-time safe buffer reads
 */
void Buffer_Accessor::set_buffer_size_from_syscall(std::string_view routine,
    Stack_Frame::IR_Stack& argument_stack)
{
    credence_assert(!argument_stack.empty());
    m::match(routine)(m::pattern | m::or_(sv("read")) = [&] {
        auto argument = argument_stack.back();
        if (util::is_numeric(argument))
            read_bytes_cache_ = std::stoul(argument);
        else if (type::is_rvalue_data_type_a_type(argument, "int"))
            read_bytes_cache_ =
                std::stoul(type::get_value_from_rvalue_data_type(argument));
    });
}

/**
 * @brief Get the length of a string by its lvalue storage device
 */
std::size_t Buffer_Accessor::get_lvalue_string_size(LValue const& lvalue,
    Stack_Frame const& stack_frame)
{
    Size len{ 0 };
    std::string key{};
    auto& locals = table_->get_stack_frame_symbols();
    auto& vectors = table_->vectors;
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);
    auto frame = stack_frame.get_stack_frame();
    auto return_frame = stack_frame.tail;
    // A stack of return calls, i.e. return_value = func(func(func(x)));
    if (lvalue == "RET") {
        credence_assert(table_->functions.contains(return_frame));
        auto tail_frame = table_->functions.at(return_frame);
        auto return_rvalue = tail_frame->ret->second;
        // what was passed to this function in its parameters?
        if (tail_frame->is_parameter(return_rvalue)) {
            credence_assert(
                table_->functions.contains(stack_frame.call_stack.back()));
            // get the last frame in the call stack
            auto last_stack_frame =
                table_->functions.at(stack_frame.call_stack.back());
            if (last_stack_frame->locals.is_pointer(tail_frame->ret->first)) {
                return type::get_size_from_rvalue_data_type(
                    last_stack_frame->locals.get_pointer_by_name(
                        tail_frame->ret->first));
            } else {
                if (type::is_rvalue_data_type(tail_frame->ret->first))
                    return type::get_size_from_rvalue_data_type(
                        tail_frame->ret->first);
                else
                    return type::get_size_from_rvalue_data_type(
                        last_stack_frame->locals.get_symbol_by_name(
                            tail_frame->ret->first));
            }
        }
        return type::get_size_from_rvalue_data_type(tail_frame->ret->first);
    }
    // The string is a local in the stack frame
    if (locals.is_defined(lvalue)) {
        if (locals.is_pointer(lvalue) and
            type::is_rvalue_data_type_string(
                locals.get_pointer_by_name(lvalue))) {
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(
                    locals.get_pointer_by_name(lvalue)));
        }
        if (locals.is_pointer(lvalue)) {
            auto rvalue_address =
                ir::object::get_rvalue_at_lvalue_object_storage(
                    lvalue, frame, vectors, __source__);
            return type::get_size_from_rvalue_data_type(rvalue_address);
        }
        auto local_symbol = locals.get_symbol_by_name(lvalue);
        auto local_rvalue = type::get_value_from_rvalue_data_type(local_symbol);
        if (local_rvalue == "RET") {
            credence_assert(table_->functions.contains(return_frame));
            auto tail_frame = table_->functions.at(return_frame);
            return type::get_size_from_rvalue_data_type(tail_frame->ret->first);
        }
        return type::get_size_from_rvalue_data_type(
            locals.get_symbol_by_name(lvalue));
    }

    if (type::is_dereference_expression(lvalue)) {
        len = type::get_size_from_rvalue_data_type(
            ir::object::get_rvalue_at_lvalue_object_storage(
                type::get_unary_rvalue_reference(lvalue),
                frame,
                vectors,
                __source__));
    } else if (is_global_vector(lhs)) {
        auto vector = table_->vectors.at(lhs);
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        len = type::get_size_from_rvalue_data_type(vector->data.at(key));
    } else if (is_vector_offset(lvalue)) {
        auto vector = table_->vectors.at(lhs);
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        len = type::get_size_from_rvalue_data_type(vector->data.at(key));
    } else if (is_immediate(lvalue)) {
        len = type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(lvalue));
    } else {
        len = type::get_size_from_rvalue_data_type(
            ir::object::get_rvalue_at_lvalue_object_storage(
                lvalue, frame, vectors, credence_current_location));
    }
    return len;
}

/**
 * @brief Get the %rip offset of a vector index and operand size
 */
Vector_Accessor::Vector_Entry_Pair Vector_Accessor::get_rip_offset_address(
    LValue const& lvalue,
    RValue const& offset)
{
    auto frame = table_->get_stack_frame();
    auto& vectors = table_->vectors;
    auto vector = type::from_lvalue_offset(lvalue);
    if (!is_vector_offset(lvalue))
        return std::make_pair(0UL,
            assembly::get_operand_size_from_rvalue_datatype(
                table_->vectors.at(vector)->data.at("0")));
    if (!table_->hoisted_symbols.has_key(offset) and
        not value::is_integer_string(offset))
        throw_compiletime_error(
            fmt::format("Invalid index '{} on vector lvalue", offset), vector);
    if (table_->hoisted_symbols.has_key(offset)) {
        auto index = ir::object::get_rvalue_at_lvalue_object_storage(
            offset, frame, vectors, credence_current_location);
        auto key = std::string{ type::get_value_from_rvalue_data_type(index) };
        if (!vectors.at(vector)->data.contains(key))
            throw_compiletime_error(
                fmt::format(
                    "Invalid out-of-range index '{}' on vector lvalue", key),
                vector);
        return std::make_pair(vectors.at(vector)->offset.at(key),
            assembly::get_operand_size_from_rvalue_datatype(
                vectors.at(vector)->data.at(key)));
    } else if (value::is_integer_string(offset)) {
        if (!vectors.at(vector)->data.contains(offset))
            throw_compiletime_error(
                fmt::format(
                    "Invalid out-of-range index '{}' on vector lvalue", offset),
                vector);
        return std::make_pair(vectors.at(vector)->offset.at(offset),
            assembly::get_operand_size_from_rvalue_datatype(
                vectors.at(vector)->data.at(offset)));
    } else
        return std::make_pair(0UL,
            assembly::get_operand_size_from_rvalue_datatype(
                vectors.at(vector)->data.at("0")));
}

/**
 * @brief Get the storage device of an lvalue, and its insertion instructions
 */
assembly::Instruction_Pair
Address_Accessor::get_lvalue_address_and_instructions(LValue const& lvalue,
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
        add_asm__as(instructions.second, mov, Register::rax, storage);
        flag_accessor_.set_instruction_flag(
            flag::Indirect, instruction_index + 1);
        instructions.first = Register::rax;
    } else if (is_global_vector(lhs)) {
        auto vector = table_->vectors.at(lhs);
        if (table_->globals.is_pointer(lhs)) {
            auto rip_storage =
                vector_accessor.get_rip_offset_address(lvalue, offset);
            auto prefix = storage_prefix_from_operand_size(rip_storage.second);
            auto rip_arithmetic =
                rip_storage.first == 0UL
                    ? lhs
                    : fmt::format("{}+{}", lhs, rip_storage.first);
            rip_arithmetic =
                use_prefix
                    ? fmt::format("{} [rip + {}]", prefix, rip_arithmetic)
                    : fmt::format("[rip + {}]", rip_arithmetic);
            instructions.first = assembly::make_array_immediate(rip_arithmetic);
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
bool Address_Accessor::is_lvalue_storage_type(LValue const& lvalue,
    std::string_view type_check)
{
    try {
        auto frame = table_->get_stack_frame();
        return type::get_type_from_rvalue_data_type(
                   type::get_rvalue_data_type_as_string(
                       ir::object::get_rvalue_at_lvalue_object_storage(
                           lvalue, frame, table_->vectors, __source__))) ==
               type_check;
    } catch (...) {
        return false;
    }
}

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
            [&](assembly::Stack_Offset const& s) {
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
Storage Address_Accessor::get_lvalue_address_from_stack(LValue const& lvalue)
{
    return stack_->get(lvalue).first;
}

} // namespace detail

} // namespace memory