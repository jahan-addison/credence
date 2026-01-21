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

#include "accessor.h"
#include "credence/error.h"                     // for credence_assert, thr...
#include "credence/ir/ita.h"                    // for Instruction
#include "credence/target/common/memory.h"      // for is_vector_offset
#include "credence/target/common/stack_frame.h" // for Stack_Frame, Locals
#include "credence/values.h"                    // for is_integer_string
#include "easyjson.h"                           // for JSON
#include "types.h"                              // for RValue, Table_Pointer
#include <credence/ir/object.h>                 // for Object, Function
#include <credence/types.h>                     // for get_size_from_rvalue...
#include <credence/util.h>                      // for is_numeric, __source__
#include <fmt/format.h>                         // for format
#include <map>                                  // for map
#include <matchit.h>                            // for Or, match, or_, pattern
#include <string>                               // for basic_string, char_t...
#include <tuple>                                // for get, tuple

/****************************************************************************
 *
 * Pure virtual and template types for platform-agnostic memory access data
 * types to facilitate the Memory_Accessor facade.
 *
 * Provides access to the object table (globals, functions, vectors), pushdown
 * stack, available registers, stack frame during assembly code generation.
 *
 ****************************************************************************/

namespace credence::target::x86_64::assembly {
enum class Operand_Size : std::size_t;
}

namespace credence::target::common::memory {

struct Memory_Accessor::impl
{
    explicit impl(std::shared_ptr<ir::object::Object> obj)
        : objects(std::move(obj))
        , stack_frame(objects)
    {
    }
    impl(const impl& other)
        : objects(other.objects)
        , stack_frame(other.objects)
    {
    }
    std::shared_ptr<ir::object::Object> objects;
    Stack_Frame stack_frame;
};

Memory_Accessor::Memory_Accessor(std::shared_ptr<ir::object::Object> objects)
    : pimpl(std::make_unique<impl>(std::move(objects)))
{
}

Memory_Accessor::~Memory_Accessor() = default;

Stack_Frame& Memory_Accessor::get_frame_in_memory()
{
    return pimpl->stack_frame;
}
const Stack_Frame& Memory_Accessor::get_frame_in_memory() const
{
    return pimpl->stack_frame;
}

struct Buffer_Accessor::impl
{
    impl(Table_Pointer& table)
        : table(table)
        , constant_size_index(0)
    {
    }
    Table_Pointer& table;
    std::size_t constant_size_index;
    std::map<RValue, Label> string_literals;
    std::map<RValue, Label> float_literals;
    std::map<RValue, Label> double_literals;
    std::size_t read_bytes_cache_{ 0 };
};

Buffer_Accessor::Buffer_Accessor(Table_Pointer& table)
    : pimpl(std::make_unique<impl>(table))
{
}

Buffer_Accessor::~Buffer_Accessor() = default;

struct Table_Accessor::impl
{
    impl(Table_Pointer& table)
        : table_(table)
    {
    }
    Table_Pointer& table_;
    unsigned int index{};
};

Table_Accessor::Table_Accessor(Table_Pointer& table)
    : pimpl(std::make_unique<impl>(table))
{
}

Table_Accessor::~Table_Accessor() = default;

Size Buffer_Accessor::get_size_of_return_string(Stack_Frame const& stack_frame)
{
    auto& table_ = pimpl->table;
    credence_assert(table_->get_functions().contains(stack_frame.tail));
    auto tail_frame = table_->get_functions().at(stack_frame.tail);
    auto return_rvalue = tail_frame->get_ret()->second;

    if (tail_frame->is_parameter(return_rvalue)) {
        credence_assert(
            table_->get_functions().contains(stack_frame.call_stack.back()));
        auto last_stack_frame =
            table_->get_functions().at(stack_frame.call_stack.back());
        if (last_stack_frame->get_locals().is_pointer(
                tail_frame->get_ret()->first)) {
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(
                    last_stack_frame->get_locals().get_pointer_by_name(
                        tail_frame->get_ret()->first)));
        }
        if (type::is_rvalue_data_type(tail_frame->get_ret()->first))
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(
                    tail_frame->get_ret()->first));
        return type::get_size_from_rvalue_data_type(
            last_stack_frame->get_locals().get_symbol_by_name(
                tail_frame->get_ret()->first));
    }
    return type::get_size_from_rvalue_data_type(
        type::get_rvalue_datatype_from_string(tail_frame->get_ret()->first));
}

Size Buffer_Accessor::get_size_in_local_address(LValue const& lvalue,
    Stack_Frame const& stack_frame)
{
    auto frame = stack_frame.get_stack_frame();
    auto& locals = pimpl->table->get_stack_frame_symbols();
    if (locals.is_pointer(lvalue) and
        type::is_rvalue_data_type_string(locals.get_pointer_by_name(lvalue))) {
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(
                locals.get_pointer_by_name(lvalue)));
    }
    if (locals.is_pointer(lvalue)) {
        auto rvalue_address = ir::object::get_rvalue_at_lvalue_object_storage(
            lvalue, frame, pimpl->table->get_vectors(), __source__);
        return type::get_size_from_rvalue_data_type(rvalue_address);
    }
    auto local_symbol = locals.get_symbol_by_name(lvalue);
    auto local_rvalue = type::get_value_from_rvalue_data_type(local_symbol);

    if (local_rvalue == "RET") {
        credence_assert(
            pimpl->table->get_functions().contains(stack_frame.tail));
        auto tail_frame = pimpl->table->get_functions().at(stack_frame.tail);
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(
                tail_frame->get_ret()->first));
    }
    return type::get_size_from_rvalue_data_type(local_symbol);
}

/**
 * @brief Get the size of a string in the object table at compile-time
 */
Size Buffer_Accessor::get_size_of_string_lvalue_buffer_address(
    LValue const& lvalue,
    Stack_Frame const& stack_frame)
{
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);
    auto& vectors = pimpl->table->get_vectors();
    auto frame = stack_frame.get_stack_frame();

    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto& table = pimpl->table;
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->get_vectors().contains(rvalue_reference) and
               table->get_globals().is_pointer(rvalue_reference);
    };

    if (lvalue == "RET")
        return get_size_of_return_string(stack_frame);

    if (pimpl->table->get_stack_frame_symbols().is_defined(lvalue))
        return get_size_in_local_address(lvalue, stack_frame);

    if (type::is_dereference_expression(lvalue)) {
        return type::get_size_from_rvalue_data_type(
            ir::object::get_rvalue_at_lvalue_object_storage(
                type::get_unary_rvalue_reference(lvalue), frame, vectors));
    }

    if (is_global_vector(lhs)) {
        std::string key{};
        auto vector = pimpl->table->get_vectors().at(lhs);
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        return type::get_size_from_rvalue_data_type(vector->get_data().at(key));
    }

    if (is_vector_offset(lvalue)) {
        std::string key{};
        auto vector = pimpl->table->get_vectors().at(lhs);
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        return type::get_size_from_rvalue_data_type(vector->get_data().at(key));
    }

    if (is_immediate(lvalue))
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(lvalue));
    else
        return type::get_size_from_rvalue_data_type(
            ir::object::get_rvalue_at_lvalue_object_storage(
                lvalue, frame, vectors, __source__));
}

/**
 * @brief Set a buffer size from an object in the cache maps
 */
void Buffer_Accessor::set_buffer_size_from_syscall(std::string_view routine,
    memory::Locals& argument_stack)
{
    credence_assert(!argument_stack.empty());
    namespace m = matchit;
    m::match(routine)(m::pattern | m::or_(sv("read")) = [&] {
        auto argument = argument_stack.back();
        if (credence::util::is_numeric(argument))
            pimpl->read_bytes_cache_ = std::stoul(argument);
        else if (type::is_rvalue_data_type_a_type(argument, "int"))
            pimpl->read_bytes_cache_ =
                std::stoul(type::get_value_from_rvalue_data_type(argument));
    });
}

/**
 * @brief Compile-time buffer storage for syscalls
 */
bool Buffer_Accessor::has_bytes()
{
    return pimpl->read_bytes_cache_ == 0UL;
}
std::size_t Buffer_Accessor::read_bytes()
{
    auto read_bytes = pimpl->read_bytes_cache_;
    pimpl->read_bytes_cache_ = 0;
    return read_bytes;
}
void Buffer_Accessor::insert_string_literal(RValue const& key,
    Label const& asciz_address)
{
    pimpl->string_literals.insert_or_assign(key, asciz_address);
}
void Buffer_Accessor::insert_float_literal(RValue const& key,
    Label const& floatz_address)
{
    pimpl->float_literals.insert_or_assign(key, floatz_address);
}
void Buffer_Accessor::insert_double_literal(RValue const& key,
    Label const& doublez_address)
{
    pimpl->double_literals.insert_or_assign(key, doublez_address);
}
RValue Buffer_Accessor::get_string_address_offset(RValue const& string)
{
    credence_assert(is_allocated_string(string));
    return pimpl->string_literals.at(string);
}
RValue Buffer_Accessor::get_float_address_offset(RValue const& string)
{
    credence_assert(is_allocated_float(string));
    return pimpl->float_literals.at(string);
}
RValue Buffer_Accessor::get_double_address_offset(RValue const& string)
{
    credence_assert(is_allocated_double(string));
    return pimpl->double_literals.at(string);
}
bool Buffer_Accessor::is_allocated_string(RValue const& rvalue)
{
    return pimpl->string_literals.contains(rvalue);
}
bool Buffer_Accessor::is_allocated_float(RValue const& rvalue)
{
    return pimpl->float_literals.contains(rvalue);
}
bool Buffer_Accessor::is_allocated_double(RValue const& rvalue)
{
    return pimpl->double_literals.contains(rvalue);
}
std::size_t* Buffer_Accessor::get_constant_size_index()
{
    return &pimpl->constant_size_index;
}
void Buffer_Accessor::set_constant_size_index(std::size_t index)
{
    pimpl->constant_size_index = index;
}

void Table_Accessor::set_ir_iterator_index(unsigned int index_)
{
    pimpl->index = index_;
}
bool Table_Accessor::is_ir_instruction_temporary()
{
    return type::is_temporary(
        std::get<1>(pimpl->table_->get_ir_instructions()->at(pimpl->index)));
}

LValue Table_Accessor::get_last_lvalue_assignment(unsigned int index_)
{
    for (; index_ > 0; index_--) {
        auto ir_inst = pimpl->table_->get_ir_instructions()->at(index_);
        auto lvalue = std::get<1>(ir_inst);
        if (std::get<0>(ir_inst) == ir::Instruction::MOV and
            pimpl->table_->local_contains(lvalue) and
            not lvalue.starts_with("_p") and not lvalue.starts_with("_t")) {
            return lvalue;
        }
    }
    return "";
}

std::string Table_Accessor::get_ir_instruction_lvalue()
{
    return std::get<1>(pimpl->table_->get_ir_instructions()->at(pimpl->index));
}
bool Table_Accessor::last_ir_instruction_is_assignment()
{
    if (pimpl->index < 1)
        return false;
    auto last = pimpl->table_->get_ir_instructions()->at(pimpl->index - 1);
    return std::get<0>(last) == ir::Instruction::MOV and
           not type::is_temporary(std::get<1>(last));
}
bool Table_Accessor::next_ir_instruction_is_temporary()
{
    if (pimpl->table_->get_ir_instructions()->size() < pimpl->index + 1)
        return false;
    auto next = pimpl->table_->get_ir_instructions()->at(pimpl->index + 1);
    return std::get<0>(next) == ir::Instruction::MOV and
           type::is_temporary(std::get<1>(next));
}
bool Table_Accessor::next_ir_instruction_is_assignment()
{
    if (pimpl->table_->get_ir_instructions()->size() < pimpl->index + 1)
        return false;
    auto last = pimpl->table_->get_ir_instructions()->at(pimpl->index + 1);
    return std::get<0>(last) == ir::Instruction::MOV and
           not type::is_temporary(std::get<1>(last));
}
Table_Pointer& Table_Accessor::get_table()
{
    return pimpl->table_;
}
const Table_Pointer& Table_Accessor::get_table() const
{
    return pimpl->table_;
}
unsigned int Table_Accessor::get_index() const
{
    return pimpl->index;
}

/**
 * @brief Get the offset in vector from hoisted symbols
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_hoisted_symbols(
    LValue const& vector,
    RValue const& offset) -> Entry_Pair
{
    auto frame = table_->get_stack_frame();
    auto& vectors = table_->get_vectors();
    auto index = ir::object::get_rvalue_at_lvalue_object_storage(
        offset, frame, vectors, __source__);
    auto key = std::string{ type::get_value_from_rvalue_data_type(index) };
    if (!vectors.at(vector)->get_data().contains(key))
        throw_compiletime_error(
            fmt::format(
                "Invalid out-of-range index '{}' on vector lvalue", key),
            vector);
    return std::make_pair(vectors.at(vector)->get_offset().at(key),
        get_size_from_vector_offset(vectors.at(vector)->get_data().at(key)));
}

/**
 * @brief Type check invalid vector symbol or offset rvalue type
 */
template<typename Entry>
void Vector_Accessor<Entry>::type_check_invalid_vector_symbol(
    LValue const& vector,
    RValue const& offset)
{
    if (!table_->get_hoisted_symbols().has_key(offset) and
        not value::is_integer_string(offset))
        throw_compiletime_error(
            fmt::format("Invalid index '{}' on vector lvalue", offset), vector);
}

/**
 * @brief Get offset by valid integer rvalue
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_integer_rvalue(
    LValue const& vector,
    RValue const& offset) -> Entry_Pair
{
    auto& vectors = table_->get_vectors();
    if (!vectors.at(vector)->get_data().contains(offset))
        throw_compiletime_error(
            fmt::format(
                "Invalid out-of-range index '{}' on vector lvalue", offset),
            vector);
    return std::make_pair(vectors.at(vector)->get_offset().at(offset),
        get_size_from_vector_offset(vectors.at(vector)->get_data().at(offset)));
}

/**
 * @brief Get offset by trivial vector with no indices
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_trivial_vector(
    LValue const& vector) -> Entry_Pair
{
    return std::make_pair(0UL,
        get_size_from_vector_offset(
            table_->get_vectors().at(vector)->get_data().at("0")));
}

/**
 * @brief Get the offset address of a vector from its lvalue and rvalue
 * offset
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_address(LValue const& lvalue,
    RValue const& offset) -> Entry_Pair
{
    auto frame = table_->get_stack_frame();
    auto& vectors = table_->get_vectors();
    auto vector = type::from_lvalue_offset(lvalue);

    type_check_invalid_vector_symbol(vector, offset);

    if (!is_vector_offset(lvalue))
        return get_offset_from_trivial_vector(vector);

    if (table_->get_hoisted_symbols().has_key(offset))
        return get_offset_from_hoisted_symbols(vector, offset);

    if (value::is_integer_string(offset))
        return get_offset_from_integer_rvalue(vector, offset);

    return std::make_pair(0UL,
        get_size_from_vector_offset(vectors.at(vector)->get_data().at("0")));
}

template class Vector_Accessor<Size>;
template class Vector_Accessor<x86_64::assembly::Operand_Size>;

} // namespace credence::target::common::memory
