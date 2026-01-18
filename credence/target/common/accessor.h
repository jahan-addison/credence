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

#include "stack_frame.h"        // for Locals
#include "types.h"              // for RValue, LValue, Stack_Pointer, Table...
#include <concepts>             // for derived_from
#include <credence/ir/object.h> // for Object, get_rvalue_at_lvalue_object_...
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for from_lvalue_offset, get_rvalue_data_...
#include <credence/util.h>      // for __source__, overload
#include <cstddef>              // for size_t
#include <deque>                // for deque
#include <memory>               // for shared_ptr, unique_ptr
#include <string>               // for basic_string, char_traits, string
#include <string_view>          // for basic_string_view, operator==, strin...
#include <utility>              // for pair
#include <variant>              // for monostate, visit

namespace credence::target::common {
struct Flag_Accessor;
}

/****************************************************************************
 *
 * Pure virtual and template types for platform-specific memory accessors
 *
 * Provides access to the object table (globals, functions, vectors) and
 * stack frame during assembly code generation.
 *
 ****************************************************************************/

namespace credence::target::common::memory {

class Memory_Accessor
{
  public:
    explicit Memory_Accessor(std::shared_ptr<ir::object::Object> objects);
    ~Memory_Accessor();
    Memory_Accessor(const Memory_Accessor&) = delete;
    Memory_Accessor& operator=(const Memory_Accessor&) = delete;

    Stack_Frame& get_frame_in_memory();
    const Stack_Frame& get_frame_in_memory() const;

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

template<typename T>
concept Memory_Accessor_T = std::derived_from<T, Memory_Accessor>;

template<Memory_Accessor_T Accessor>
using Memory_Access = std::shared_ptr<Accessor>;

template<Enum_T Registers, Stack_T Stack, Pair_T Address_Result>
class Address_Accessor;

class Buffer_Accessor
{
  public:
    explicit Buffer_Accessor(Table_Pointer& table);
    ~Buffer_Accessor();

    Buffer_Accessor(const Buffer_Accessor&) = delete;
    Buffer_Accessor& operator=(const Buffer_Accessor&) = delete;

    Size get_size_of_string_lvalue_buffer_address(LValue const& lvalue,
        Stack_Frame const& stack_frame);

    void set_buffer_size_from_syscall(std::string_view routine,
        memory::Locals& argument_stack);

    bool has_bytes();

    std::size_t read_bytes();
    void insert_string_literal(RValue const& key, Label const& asciz_address);
    void insert_float_literal(RValue const& key, Label const& floatz_address);
    void insert_double_literal(RValue const& key, Label const& doublez_address);
    RValue get_string_address_offset(RValue const& string);

    RValue get_float_address_offset(RValue const& string);
    RValue get_double_address_offset(RValue const& string);
    bool is_allocated_string(RValue const& rvalue);
    bool is_allocated_float(RValue const& rvalue);
    bool is_allocated_double(RValue const& rvalue);

    std::size_t* get_constant_size_index();
    void set_constant_size_index(std::size_t index);

  private:
    Size get_size_in_local_address(LValue const& lvalue,
        Stack_Frame const& stack_frame);
    Size get_size_of_return_string(Stack_Frame const& stack_frame);

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

/**
 * @brief
 * Table accessor for access to the object table and type checker
 */
struct Table_Accessor
{
    Table_Accessor() = delete;
    explicit Table_Accessor(Table_Pointer& table);
    ~Table_Accessor();

    Table_Accessor(const Table_Accessor&) = delete;
    Table_Accessor& operator=(const Table_Accessor&) = delete;

    void set_ir_iterator_index(unsigned int index_);
    LValue get_last_lvalue_assignment(unsigned int index_);
    bool is_ir_instruction_temporary();
    LValue get_ir_instruction_lvalue();
    bool last_ir_instruction_is_assignment();
    bool next_ir_instruction_is_assignment();
    bool next_ir_instruction_is_temporary();

    Table_Pointer& get_table();
    const Table_Pointer& get_table() const;
    unsigned int get_index() const;

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

/**
 * @brief Template machine code instruction accessor and storage
 */
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

    /**
     * @brief Get the offset address of a vector from its lvalue and rvalue
     * offset
     */
    Entry_Pair get_offset_address(LValue const& lvalue, RValue const& offset);

  private:
    /**
     * @brief Get the offset in vector from hoisted symbols
     */
    Entry_Pair get_offset_from_hoisted_symbols(LValue const& vector,
        RValue const& offset);

    /**
     * @brief Type check invalid vector symbol or offset rvalue type
     */
    void type_check_invalid_vector_symbol(LValue const& vector,
        RValue const& offset);

    /**
     * @brief Get offset by valid integer rvalue
     */
    Entry_Pair get_offset_from_integer_rvalue(LValue const& vector,
        RValue const& offset);

    /**
     * @brief Get offset by trivial vector with no indices
     */
    Entry_Pair get_offset_from_trivial_vector(LValue const& vector);

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
                    auto size = stack->get(offset).second;
                    accumulator = get_accumulator_register_from_size(size);
                },
                [&](Register const& device) { accumulator = device; },
                [&](Immediate const& immediate) {
                    auto size = get_operand_size_from_immediate(immediate);
                    accumulator = get_accumulator_register_from_size(size);
                } },
            storage);
        return accumulator;
    }

    virtual T get_operand_size_from_immediate(Immediate const& immediate) = 0;
    virtual Register get_accumulator_register_from_size(T size) = 0;

  protected:
    Register* signal_register_;
};

/**
 * @brief Address accessor base class with common address resolution logic.
 */
template<Enum_T Registers, Stack_T Stack, Pair_T Address_Result>
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
                               lvalue,
                               frame,
                               table_->get_vectors(),
                               __source__))) == type_check;
        } catch (...) {
            return false;
        }
    }

  protected:
    Operand_Lambda is_vector = [&](RValue const& rvalue) {
        return table_->get_vectors().contains(type::from_lvalue_offset(rvalue));
    };
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table_->get_vectors().contains(rvalue_reference) and
               table_->get_globals().is_pointer(rvalue_reference);
    };

  public:
    bool address_ir_assignment{ false };

    std::deque<Immediate> immediate_stack{};

  protected:
    Table_Pointer& table_;
    Stack_Pointer<Stack>& stack_;
    Flag_Accessor& flag_accessor_;

  public:
    Buffer_Accessor buffer_accessor;
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

} // namespace credence::target::common::memory