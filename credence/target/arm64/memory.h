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

#include "assembly.h"                           // for Register, Operand_Size
#include "stack.h"                              // for Stack
#include <credence/ir/object.h>                 // for LValue, Size, RValue
#include <credence/map.h>                       // for Ordered_Map
#include <credence/target/common/accessor.h>    // for Accumulator_Accessor
#include <credence/target/common/flags.h>       // for Flag_Accessor
#include <credence/target/common/memory.h>      // for Operand_Type, is_imm...
#include <credence/target/common/stack_frame.h> // for Stack_Frame
#include <credence/target/common/types.h>       // for Storage_T
#include <credence/types.h>                     // for get_size_from_rvalue...
#include <credence/util.h>                      // for SET_INLINE_DEBUG
#include <cstddef>                              // for size_t
#include <deque>                                // for deque
#include <functional>                           // for function
#include <memory>                               // for shared_ptr, make_shared
#include <set>                                  // for set
#include <string>                               // for basic_string, string
#include <utility>                              // for move

/****************************************************************************
 *
 * ARM64 Memory and Address Accessors
 *
 * Example - local variable access:
 *
 *   B code:    auto x; x = 10;
 *
 * Memory accessor uses w9-w18 for locals:
 *   mov w9, #10             ; x in register w9 (first local)
 *
 * Or stack if w9-w18 exhausted:
 *   mov w8, #10
 *   str w8, [sp, #8]        ; x at [sp + 8]
 *
 * Example - array access (always on stack):
 *
 *   B code:    auto arr[5]; arr[2] = 42;
 *
 * Memory accessor generates:
 *   mov w8, #42
 *   str w8, [sp, #24]       ; arr[2] at base + 2*8
 *
 *****************************************************************************/

/****************************************************************************
 * Special register usage conventions:
 *
 *   x6   = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x7    = multiplication scratch register
 *   x8    = The default "accumulator" register for expression expansion
 *   x9 - x18 = Local scope variables, after which the stack is used
 *
 *  NOTE : we save x9-x18 on the stack before calling a function
 *  via the Allocate, Access, Deallocate pattern
 *
 *   w0, x0 = Return results
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64::memory {
class Memory_Accessor;
namespace detail {
class Device_Accessor;
struct Instruction_Accessor;
}
}

namespace credence::target::arm64 {

using Register = assembly::Register;
using Directive = assembly::Directive;
using Mnemonic = assembly::Mnemonic;
using Storage = common::Storage_T<assembly::Register>;
using Operator_Symbol = std::string;
using Immediate = assembly::Immediate;
using Directives = assembly::Directives;

/**
 * Short-form helpers for matchit predicate pattern matching
 */
using common::memory::is_immediate;
using common::memory::is_parameter;
using common::memory::is_temporary;
using common::memory::is_vector_offset;

namespace memory {

using Operand_Type = common::memory::Operand_Type;

namespace registers {

using general_purpose = std::deque<Register>;

const general_purpose available_doubleword = {
    Register::x9,
    Register::x10,
    Register::x11,
    Register::x12,
    Register::x13,
    Register::x14,
    Register::x15,
    Register::x16,
    Register::x17,
    Register::x18,
};
const general_purpose available_word = {
    Register::w9,
    Register::w10,
    Register::w11,
    Register::w12,
    Register::w13,
    Register::w14,
    Register::w15,
    Register::w16,
    Register::w17,
    Register::w18,
};

} // namespace registers

using Memory_Access = std::shared_ptr<Memory_Accessor>;
using Instruction_Pointer = std::shared_ptr<detail::Instruction_Accessor>;
using Stack_Pointer = std::shared_ptr<assembly::Stack>;
using Table_Pointer = std::shared_ptr<ir::object::Object>;
using Stack_Frame = target::common::memory::Stack_Frame;

Register get_second_register_for_binary_operand(assembly::Operand_Size size);

bool is_doubleword_storage_size(assembly::Storage const& storage,
    Stack_Pointer& stack,
    Stack_Frame& stack_frame);

inline assembly::Operand_Size get_word_size_from_storage(
    assembly::Storage const& storage,
    Stack_Pointer& stack,
    Stack_Frame& stack_frame)
{
    return is_doubleword_storage_size(storage, stack, stack_frame)
               ? assembly::Operand_Size::Doubleword
               : assembly::Operand_Size::Word;
}

assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack);

using ARM64_Memory_Accessor = common::memory::Memory_Accessor;

namespace detail {

class Device_Accessor;

using ARM64_Address_Accessor = common::memory::
    Address_Accessor<Register, assembly::Stack, assembly::Instruction_Pair>;

using ARM64_Accumulator_Accessor = common::memory::
    Accumulator_Accessor<assembly::Operand_Size, Register, assembly::Stack>;

using ARM64_Instruction_Accessor =
    common::memory::Instruction_Accessor<assembly::Instructions>;

using ARM64_Vector_Accessor = common::memory::Vector_Accessor<Size>;

using ARM64_Register_Accessor = common::memory::Register_Accessor<Register>;

using Buffer_Accessor = common::memory::Buffer_Accessor;

using Table_Accessor = common::memory::Table_Accessor;

using Operand_Lambda = std::function<bool(RValue)>;

using Flag_Accessor = common::Flag_Accessor;

// ---

struct Accumulator_Accessor : public ARM64_Accumulator_Accessor
{
    explicit Accumulator_Accessor(Register* signal_register)
        : ARM64_Accumulator_Accessor(signal_register)
    {
    }

    assembly::Operand_Size get_operand_size_from_immediate(
        Immediate const& immediate) override
    {
        return assembly::get_operand_size_from_rvalue_datatype(immediate);
    }

    Register get_accumulator_register_from_size(
        assembly::Operand_Size size) override;
};

struct Instruction_Accessor : public ARM64_Instruction_Accessor
{};

struct Register_Accessor : public ARM64_Register_Accessor
{
    explicit Register_Accessor(Register* signal_register)
        : ARM64_Register_Accessor(signal_register)
    {
        d_size_registers = registers::available_doubleword;
        w_size_registers = registers::available_word;
    }

    inline registers::general_purpose get_register_list_by_size(
        assembly::Operand_Size size)
    {
        if (size == assembly::Operand_Size::Doubleword)
            return d_size_registers;
        else
            return w_size_registers;
    }

    inline void reset_available_registers()
    {
        d_size_registers = registers::available_doubleword;
        w_size_registers = registers::available_word;
    }
    std::deque<Register> stack{};
};

struct Address_Accessor : public ARM64_Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        ARM64_Address_Accessor::Stack_Pointer_Type& stack,
        Flag_Accessor& flag_accessor,
        Register_Accessor& register_accessor)
        : ARM64_Address_Accessor(table, stack, flag_accessor)
        , register_accessor_(register_accessor)
    {
    }

    friend class Device_Accessor;

    Address_Accessor::Address get_arm64_lvalue_and_insertion_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        Device_Accessor& device_accessor);

  private:
    Address_Accessor::Address get_lvalue_address_and_from_unary_and_vectors(
        Address& instructions,
        LValue const& lvalue,
        std::size_t instruction_index);

  private:
    Register_Accessor& register_accessor_;
};

class Device_Accessor
{
  public:
    using Device = Storage;

  public:
    Device_Accessor(Address_Accessor& address_accessor,
        Stack_Frame& stack_frame,
        Stack_Pointer& stack)
        : address_accessor_(address_accessor)
        , register_accessor_(address_accessor.register_accessor_)
        , stack_frame_(stack_frame)
        , stack_(stack)
    {
    }

    friend struct Address_Accessor;

    using Operand_Size = assembly::Operand_Size;

    void reset_storage_devices()
    {
        register_id.clear();
        id_index = 0;
    }

  public:
    bool is_lvalue_allocated_in_memory(LValue const& lvalue);
    Device get_operand_rvalue_device(RValue const& rvalue);
    Device get_device_by_lvalue(LValue const& lvalue);
    inline Device get_device_by_lvalue_reference(RValue const& rvalue)
    {
        return get_device_by_lvalue(rvalue);
    }
    constexpr void set_current_frame_symbol(Label const& label)
    {
        frame_symbol = label;
    }
    constexpr Label get_current_frame_name() { return frame_symbol; }

  public:
    void save_and_allocate_before_instruction_jump(
        assembly::Instructions& instructions);
    void restore_and_deallocate_after_instruction_jump(
        assembly::Instructions& instructions);

  public:
    inline Operand_Size get_word_size_from_lvalue(LValue const& lvalue)
    {
        return get_word_size_from_storage(
            get_device_by_lvalue(lvalue), stack_, stack_frame_);
    }

  public:
    Register get_available_storage_register(Operand_Size size);
    void insert_lvalue_to_device(LValue const& lvalue, Operand_Size size);
    void insert_lvalue_to_device(LValue const& lvalue, SET_INLINE_DEBUG);
    void set_vector_offset_to_storage_space(LValue const& lvalue);
    Size get_size_from_rvalue_reference(RValue const& rvalue);
    Size get_size_from_rvalue_data_type(LValue const& lvalue,
        Immediate const& rvalue);

  private:
    void set_word_or_doubleword_register(LValue const& lvalue,
        Operand_Size size);
    Size get_size_of_address_table();

  private:
    Address_Accessor& address_accessor_;
    Register_Accessor& register_accessor_;
    Stack_Frame& stack_frame_;
    Stack_Pointer& stack_;

  private:
    Label frame_symbol{ "main" };
    Ordered_Map<LValue, Storage> address_table{};
    std::set<unsigned int> register_id{};
    unsigned int id_index{ 0 };

    unsigned int vector_index{ 0 };
};

struct Vector_Accessor : public ARM64_Vector_Accessor
{
  public:
    explicit Vector_Accessor(Table_Pointer& table)
        : ARM64_Vector_Accessor(table)
    {
    }
    Entry_Size get_size_from_vector_offset(Immediate const& immediate) override
    {
        return type::get_size_from_rvalue_data_type(immediate);
    }
};

} // namespace detail

/**
 * @brief The Memory registry and mediator that orchestrates access to memory
 */
class Memory_Accessor final : public ARM64_Memory_Accessor
{
  public:
    Memory_Accessor() = delete;

    explicit Memory_Accessor(Table_Pointer table, Stack_Pointer stack_pointer)
        : ARM64_Memory_Accessor(table)
        , table_(std::move(table))
        , stack(std::move(stack_pointer))
        , table_accessor(table_)
        , accumulator_accessor{ &signal_register }
        , vector_accessor{ table_ }
        , register_accessor{ &signal_register }
        , address_accessor{ table_, stack, flag_accessor, register_accessor }
    {
        instruction_accessor = std::make_shared<detail::Instruction_Accessor>();
    }

  public:
    constexpr void set_signal_register(Register signal_)
    {
        signal_register = signal_;
    }

  public:
    Register get_accumulator_with_rvalue_context(Storage const& device);
    Register get_accumulator_with_rvalue_context(assembly::Operand_Size size);

  private:
    Register signal_register = assembly::Register::w0;
    Table_Pointer table_;

  public:
    Stack_Pointer stack;

  public:
    detail::Flag_Accessor flag_accessor{};
    detail::Table_Accessor table_accessor{ table_ };
    detail::Accumulator_Accessor accumulator_accessor{ &signal_register };
    detail::Vector_Accessor vector_accessor{ table_ };
    detail::Register_Accessor register_accessor{ &signal_register };
    detail::Address_Accessor address_accessor{ table_,
        stack,
        flag_accessor,
        register_accessor };
    detail::Device_Accessor device_accessor{ address_accessor,
        stack_frame,
        stack };
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace arm64