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

#include "assembly.h"
#include "flags.h"
#include "memory.h"
#include "stack_frame.h"
#include "types.h"

#include <credence/ir/object.h>
#include <credence/types.h>
#include <credence/util.h>
#include <matchit.h>
#include <memory>
#include <string>

namespace credence::target::common::memory {

template<Enum_T Registers,
    Stack_T Stack,
    Pair_T Address_Result,
    Stack_Frame_T Frame>
class Address_Accessor;

/**
 * @brief Buffer accessor that stores addresses of strings, floats, and doubles
 * Architecture-agnostic implementation.
 */
template<Stack_Frame_T T>
class Buffer_Accessor
{
  public:
    explicit Buffer_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }

    using Stack_Frame_Type = T;

    std::size_t get_lvalue_string_size(LValue const& lvalue,
        Stack_Frame_Type const& stack_frame)
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
                if (last_stack_frame->locals.is_pointer(
                        tail_frame->ret->first)) {
                    return type::get_size_from_rvalue_data_type(
                        type::get_rvalue_datatype_from_string(
                            last_stack_frame->locals.get_pointer_by_name(
                                tail_frame->ret->first)));
                }
                if (type::is_rvalue_data_type(tail_frame->ret->first))
                    return type::get_size_from_rvalue_data_type(
                        type::get_rvalue_datatype_from_string(
                            tail_frame->ret->first));
                return type::get_size_from_rvalue_data_type(
                    last_stack_frame->locals.get_symbol_by_name(
                        tail_frame->ret->first));
            }
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(tail_frame->ret->first));
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
            auto local_rvalue =
                type::get_value_from_rvalue_data_type(local_symbol);
            if (local_rvalue == "RET") {
                credence_assert(table_->functions.contains(return_frame));
                auto tail_frame = table_->functions.at(return_frame);
                return type::get_size_from_rvalue_data_type(
                    type::get_rvalue_datatype_from_string(
                        tail_frame->ret->first));
            }
            return type::get_size_from_rvalue_data_type(local_symbol);
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
                key =
                    std::string{ type::get_value_from_rvalue_data_type(index) };
            }
            len = type::get_size_from_rvalue_data_type(vector->data.at(key));
        } else if (is_vector_offset(lvalue)) {
            auto vector = table_->vectors.at(lhs);
            if (util::is_numeric(offset)) {
                key = offset;
            } else {
                auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                    offset, frame, vectors, __source__);
                key =
                    std::string{ type::get_value_from_rvalue_data_type(index) };
            }
            len = type::get_size_from_rvalue_data_type(vector->data.at(key));
        } else if (is_immediate(lvalue)) {
            len = type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(lvalue));
        } else {
            len = type::get_size_from_rvalue_data_type(
                ir::object::get_rvalue_at_lvalue_object_storage(
                    lvalue, frame, vectors, __source__));
        }
        return len;
    }

    void set_buffer_size_from_syscall(std::string_view routine,
        memory::Locals& argument_stack)
    {
        credence_assert(!argument_stack.empty());
        namespace m = matchit;
        m::match(routine)(m::pattern | m::or_(sv("read")) = [&] {
            auto argument = argument_stack.back();
            if (credence::util::is_numeric(argument))
                read_bytes_cache_ = std::stoul(argument);
            else if (type::is_rvalue_data_type_a_type(argument, "int"))
                read_bytes_cache_ =
                    std::stoul(type::get_value_from_rvalue_data_type(argument));
        });
    }

    constexpr bool has_bytes() { return read_bytes_cache_ == 0UL; }
    constexpr std::size_t read_bytes()
    {
        auto read_bytes = read_bytes_cache_;
        read_bytes_cache_ = 0;
        return read_bytes;
    }
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table_->vectors.contains(rvalue_reference) and
               table_->globals.is_pointer(rvalue_reference);
    };

  public:
    void insert_string_literal(RValue const& key, Label const& asciz_address)
    {
        string_cache_.insert_or_assign(key, asciz_address);
    }
    void insert_float_literal(RValue const& key, Label const& floatz_address)
    {
        float_cache_.insert_or_assign(key, floatz_address);
    }
    void insert_double_literal(RValue const& key, Label const& doublez_address)
    {
        double_cache_.insert_or_assign(key, doublez_address);
    }
    RValue get_string_address_offset(RValue const& string)
    {
        return string_cache_.at(string);
    }

    RValue get_float_address_offset(RValue const& string)
    {
        return float_cache_.at(string);
    }
    RValue get_double_address_offset(RValue const& string)
    {
        return double_cache_.at(string);
    }
    bool is_allocated_string(RValue const& rvalue)
    {
        return string_cache_.contains(rvalue);
    }
    bool is_allocated_float(RValue const& rvalue)
    {
        return float_cache_.contains(rvalue);
    }
    bool is_allocated_double(RValue const& rvalue)
    {
        return double_cache_.contains(rvalue);
    }

  private:
    Table_Pointer& table_;

  public:
    std::size_t constant_size_index{ 0 };

  private:
    std::map<std::string, RValue> string_cache_{};
    std::map<std::string, RValue> float_cache_{};
    std::map<std::string, RValue> double_cache_{};
    std::size_t read_bytes_cache_{ 0 };
};

/**
 * @brief Table accessor for memory access to the table, type checker, and
 * current index in IR visitor iteration. Architecture-agnostic.
 */
struct Table_Accessor
{
    Table_Accessor() = delete;
    explicit Table_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }
    constexpr void set_ir_iterator_index(unsigned int index_)
    {
        index = index_;
    }
    bool is_ir_instruction_temporary()
    {
        return type::is_temporary(
            std::get<1>(table_->ir_instructions->at(index)));
    }
    std::string get_ir_instruction_lvalue()
    {
        return std::get<1>(table_->ir_instructions->at(index));
    }
    bool last_ir_instruction_is_assignment()
    {
        if (index < 1)
            return false;
        auto last = table_->ir_instructions->at(index - 1);
        return std::get<0>(last) == ir::Instruction::MOV and
               not type::is_temporary(std::get<1>(last));
    }
    bool next_ir_instruction_is_temporary()
    {
        if (table_->ir_instructions->size() < index + 1)
            return false;
        auto next = table_->ir_instructions->at(index + 1);
        return std::get<0>(next) == ir::Instruction::MOV and
               type::is_temporary(std::get<1>(next));
    }
    Table_Pointer& table_;
    unsigned int index{ 0 };
};

template<Deque_T T>
struct Instruction_Accessor
{
    T& get_instructions() { return instructions_; }

    auto push(T& instruction) { instructions_.emplace_back(instruction); }

    auto begin() { return instructions_.begin(); }
    auto end() { return instructions_.end(); }
    auto begin() const { return instructions_.begin(); }
    auto end() const { return instructions_.end(); }

    std::size_t size() { return instructions_.size(); }

  private:
    T instructions_{};
};

/**
 * @brief Vector accessor for vectors in the data section
 */
template<typename Entry>
class Vector_Accessor
{
  public:
    using Entry_Size = Entry;

  public:
    explicit Vector_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }

    virtual ~Vector_Accessor() = default;

    virtual Entry_Size get_size_from_vector_offset(
        Immediate const& immediate) = 0;

  public:
    using Entry_Pair = std::pair<ir::object::Vector::Address, Entry>;

    Entry_Pair get_offset_address(LValue const& lvalue, RValue const& offset)
    {
        using namespace assembly;
        auto frame = table_->get_stack_frame();
        auto& vectors = table_->vectors;
        auto vector = type::from_lvalue_offset(lvalue);
        if (!is_vector_offset(lvalue))
            return std::make_pair(0UL,
                get_size_from_vector_offset(
                    table_->vectors.at(vector)->data.at("0")));
        if (!table_->hoisted_symbols.has_key(offset) and
            not value::is_integer_string(offset))
            throw_compiletime_error(
                fmt::format("Invalid index '{} on vector lvalue", offset),
                vector);
        if (table_->hoisted_symbols.has_key(offset)) {
            auto index = ir::object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            auto key =
                std::string{ type::get_value_from_rvalue_data_type(index) };
            if (!vectors.at(vector)->data.contains(key))
                throw_compiletime_error(
                    fmt::format(
                        "Invalid out-of-range index '{}' on vector lvalue",
                        key),
                    vector);
            return std::make_pair(vectors.at(vector)->offset.at(key),
                get_size_from_vector_offset(vectors.at(vector)->data.at(key)));
        } else if (value::is_integer_string(offset)) {
            if (!vectors.at(vector)->data.contains(offset))
                throw_compiletime_error(
                    fmt::format(
                        "Invalid out-of-range index '{}' on vector lvalue",
                        offset),
                    vector);
            return std::make_pair(vectors.at(vector)->offset.at(offset),
                get_size_from_vector_offset(
                    vectors.at(vector)->data.at(offset)));
        } else
            return std::make_pair(0UL,
                get_size_from_vector_offset(vectors.at(vector)->data.at("0")));
    }

  protected:
    Table_Pointer& table_;
};

template<typename T, Enum_T Register, Stack_T Stack>
struct Accumulator_Accessor
{
    explicit Accumulator_Accessor(Register* signal_register)
        : signal_register_(signal_register)
    {
    }

    virtual ~Accumulator_Accessor() = default;

    /**
     * @brief Get the accumulator register from storage
     */
    constexpr Register get_accumulator_register_from_storage(
        Storage_T<Register> const& storage,
        Stack_Pointer<Stack>& stack)
    {
        Register accumulator{};
        std::visit(
            util::overload{ [&](std::monostate) {
                               accumulator = get_first_of_enum_t<Register>();
                           },
                [&](Stack_Offset const& offset) {
                    auto size = stack->get_size_from_offset(offset);
                    accumulator = get_accumulator_register_from_size(size);
                },
                [&](Register const& device) { accumulator = device; },
                [&](Immediate const& immediate) {
                    auto size = type::get_size_from_rvalue_data_type(immediate);
                    accumulator = get_accumulator_register_from_size(size);
                } },
            storage);
        return accumulator;
    }

    virtual Register get_accumulator_register_from_size(T size) = 0;

  protected:
    Register* signal_register_;
};

/**
 * @brief Address accessor base class with common address resolution logic.
 */
template<Enum_T Registers,
    Stack_T Stack,
    Pair_T Address_Result,
    Stack_Frame_T Frame>
class Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        Stack_Pointer<Stack>& stack,
        Flag_Accessor& flag_accessor)
        : table_(table)
        , stack_(stack)
        , flag_accessor_(flag_accessor)
        , buffer_accessor(table)
    {
    }

    using Stack_Pointer_Type = Stack_Pointer<Stack>;
    using Address = Address_Result;
    using Storage = Storage_T<Registers>;

    Storage get_lvalue_address_from_stack(LValue const& lvalue)
    {
        return stack_->get(lvalue).first;
    }

    bool is_lvalue_storage_type(LValue const& lvalue,
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

    virtual Address get_lvalue_address_and_instructions(LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true) = 0;

  protected:
    Operand_Lambda is_vector = [&](RValue const& rvalue) {
        return table_->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table_->vectors.contains(rvalue_reference) and
               table_->globals.is_pointer(rvalue_reference);
    };

  public:
    bool address_ir_assignment{ false };

    std::deque<Immediate> immediate_stack{};

  protected:
    Table_Pointer& table_;
    Stack_Pointer<Stack>& stack_;
    Flag_Accessor& flag_accessor_;

  public:
    Buffer_Accessor<Frame> buffer_accessor;
};
template<Enum_T Register>
struct Register_Accessor
{
    explicit Register_Accessor(Register* signal_register)
        : signal_register(signal_register)
    {
    }
    using available_registers = std::deque<Register>;

    Register* signal_register;

  protected:
    std::deque<Register> d_size_registers;
    std::deque<Register> w_size_registers;
};

} // namespace memory