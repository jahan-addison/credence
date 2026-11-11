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

#include "stack_frame.h"     // for Locals
#include "types.h"           // for RValue, LValue, Stack_Pointer, Table...
#include <concepts>          // for derived_from
#include <credence/object.h> // for Object, get_rvalue_at_lvalue_object_...
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for from_lvalue_offset, get_rvalue_data_...
#include <credence/util.h>   // for __source__, overload
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <memory>            // for shared_ptr, unique_ptr
#include <string>            // for basic_string, char_traits, string
#include <string_view>       // for basic_string_view, operator==, strin...
#include <utility>           // for pair
#include <variant>           // for monostate, visit

namespace credence::target::common {
struct Flag_Accessor;
}

/****************************************************************************
 * Memory Accessors
 *
 * The platform-agnostic base types of the Memory_Accessor facade. Each target
 * derives its own accessors from them, and the inserters reach the object
 * table, the IR instruction under translation, the stack, the registers, and
 * the literals of the data section through one accessor object.
 *
 *  Example:
 *
 *    main() {
 *      auto x;
 *      x = 5;
 *      print("hello");
 *    }
 *
 *  Becomes, on arm64:
 *
 *    ldr w10, [sp, #16]               // x is on the stack of the device
 *    mov w10, #5
 *    str w10, [sp, #16]
 *    adrp x0, ._L_str1__@PAGE         // the label of the buffer accessor
 *    add x0, x0, ._L_str1__@PAGEOFF
 *    mov w1, #5                       // the size of the buffer accessor
 *    bl _print
 *
 ****************************************************************************/

namespace credence::target::common::memory {

/**
 * @brief The base of each target's memory accessor facade
 *
 * Owns the object table and the stack frame of the function under
 * translation. A target's facade adds its own register, stack, device,
 * address, and instruction accessors, and is shared by every inserter as a
 * Memory_Access pointer.
 */
class Memory_Accessor
{
  public:
    explicit Memory_Accessor(std::shared_ptr<object::Object_Table> objects);
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

/**
 * @brief Compile-time buffer sizes and the literals of the data section
 *
 * A buffer argument of a library call or syscall, such as the string of
 * `print` or `write(2)`, has its length passed in a register, and the length
 * is known at compile time from the object table. The length of the last
 * `read(2)` is cached for the buffer it filled.
 *
 * String, float, and double literals are emitted once in the data section,
 * and each literal is mapped to its label so every use loads the same
 * address.
 *
 *  Example:
 *
 *    x = "hello";
 *    print(x);
 *
 *  Becomes:
 *
 *    ._L_str1__:
 *        .asciz "hello"
 *
 *    get_string_address_offset("\"hello\"")   // ._L_str1__
 *    get_size_of_string_lvalue_buffer_address("x", frame)   // 5
 */
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

    bool has_read_bytes();

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
 * @brief Access to the object table and the IR instruction under translation
 *
 * The visitor sets the index of each IR instruction before it is translated,
 * and the inserters look behind and ahead of it. An inserter may emit
 * different instructions when the next IR instruction is an assignment or a
 * temporary, such as an address-of expression stored to a local.
 */
struct Table_Accessor
{
    Table_Accessor() = delete;
    explicit Table_Accessor(Table_Pointer& table,
        ir::Instructions_PTR& instructions);
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
    ir::Instructions& get_ir_instructions();
    unsigned int get_index() const;

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

/**
 * @brief The machine code instructions of a target
 *
 * The inserters append to it in order, and the emitter writes it to the
 * text section after the function epilogues and labels are resolved.
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
 * @brief The offset and size of an element of a global vector
 *
 * A global vector is in the data section, so an element is the address of
 * the vector plus its offset. The index is an integer literal or a local whose
 * value is known at compile time, and an index out of range is a compile-time
 * error.
 *
 *  Example:
 *
 *    mess [3] "too bad", "tough luck", "that sucks";
 *    x = mess[1];
 *
 *  Becomes:
 *
 *    get_offset_address("mess[1]", "1")   // (8, Doubleword)
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
    using Entry_Pair = std::pair<object::Vector::Address, Entry>;

    Entry_Pair get_offset_address(LValue const& lvalue, RValue const& offset);

  private:
    Entry_Pair get_offset_from_hoisted_symbols(LValue const& vector,
        RValue const& offset);
    void type_check_invalid_vector_symbol(LValue const& vector,
        RValue const& offset);
    Entry_Pair get_offset_from_integer_rvalue(LValue const& vector,
        RValue const& offset);
    Entry_Pair get_offset_from_trivial_vector(LValue const& vector);

  protected:
    Table_Pointer& table_;
};

/**
 * @brief Selection of the accumulator register for an operand
 *
 * The accumulator holds the value of an expression, and its width is the
 * width of the operand it is loaded from: a word register for an `int`, a
 * doubleword register for a pointer or `long`.
 *
 *  Example:
 *
 *    get_accumulator_register_from_storage(Immediate{ "5", "int", 4 })
 *    // w8 on arm64, eax on x86-64
 */
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
     *
     * An empty storage is the first register of the target, and a register
     * is its own accumulator
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
 * @brief Address resolution of lvalues on the stack and in the data section
 *
 * Each target derives the lvalue address and insertion instructions from it,
 * the instructions that load a vector element, a dereference, or a stack
 * local into a register before it is used.
 *
 * `address_ir_assignment` is set while an address-of expression is stored to
 * an lvalue, and `immediate_stack` holds the folded value of a bitwise
 * expression of two immediates in a temporary, in place of a `mov`.
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

    /**
     * @brief Check the type of the value an lvalue holds
     *
     * An lvalue not in the object table is not the type, such as a
     * temporary or a register argument
     *
     *  Examples:
     *
     *    is_lvalue_storage_type("x", "string")   // true, x = "hello"
     *    is_lvalue_storage_type("_t1", "int")    // false
     */
    bool is_lvalue_storage_type(LValue const& lvalue,
        std::string_view type_check)
    {
        try {
            auto& frame = table_->get_stack_frame();
            return type::get_type_from_rvalue_data_type(
                       type::get_rvalue_data_type_as_string(
                           object::get_rvalue_at_lvalue_object_storage(
                               lvalue, frame, table_->vectors, __source__))) ==
                   type_check;
        } catch (...) {
            return false;
        }
    }

  protected:
    // a vector is local or global, and a global vector is a pointer in the
    // globals of the object table
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
    Buffer_Accessor buffer_accessor;
};

/**
 * @brief The registers of a target available to locals and temporaries
 *
 * `signal_register` is set by an inserter to pass the register that holds a
 * value to the assignment of the next lvalue, such as the result of a syscall,
 * and is reset to the zero register once the assignment reads it.
 */
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