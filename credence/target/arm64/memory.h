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

#include <credence/ir/checker.h>                // for Type_Checker
#include <credence/ir/object.h>                 // for RValue, Size, LValue
#include <credence/target/arm64/assembly.h>     // for Register, Directive
#include <credence/target/arm64/stack.h>        // for Stack
#include <credence/target/common/accessor.h>    // for Accumulator_Accessor
#include <credence/target/common/flags.h>       // for Flag_Accessor
#include <credence/target/common/memory.h>      // for Operand_Type, is_imm...
#include <credence/target/common/stack_frame.h> // for Stack_Frame
#include <credence/target/common/types.h>       // for Storage_T
#include <credence/types.h>                     // for get_size_from_rvalue...
#include <cstddef>                              // for size_t
#include <deque>                                // for deque
#include <functional>                           // for function
#include <matchit.h>                            // for pattern, Wildcard
#include <memory>                               // for shared_ptr, make_shared
#include <string>                               // for basic_string, string
#include <utility>                              // for move
namespace credence {
namespace target {
namespace arm64 {
namespace memory {
class Memory_Accessor;
}
}
}
} // lines 39-39
namespace credence {
namespace target {
namespace arm64 {
namespace memory {
namespace detail {
struct Instruction_Accessor;
}
}
}
}
} // lines 42-42

namespace credence::target::arm64::memory {
class Memory_Accessor;
}
namespace credence::target::arm64::memory::detail {
struct Instruction_Accessor;
} // namespace credence::target::arm64::memory::detail

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

/**
 * @brief Stack frame object that keeps a stack of function calls and arguments
 */
using Stack_Frame = target::common::memory::Stack_Frame<Memory_Accessor>;

namespace detail {

/**
 *  Forward declarations for the Memory_Accessor dependent accessors
 */

using ARM64_Address_Accessor = common::memory::Address_Accessor<Register,
    assembly::Stack,
    assembly::Instruction_Pair,
    memory::Stack_Frame>;

using ARM64_Accumulator_Accessor = common::memory::
    Accumulator_Accessor<assembly::Operand_Size, Register, assembly::Stack>;

using ARM64_Instruction_Accessor =
    common::memory::Instruction_Accessor<assembly::Instructions>;

using ARM64_Vector_Accessor = common::memory::Vector_Accessor<Size>;

using Buffer_Accessor = common::memory::Buffer_Accessor<memory::Stack_Frame>;

using Table_Accessor = common::memory::Table_Accessor;

using ARM64_Register_Accessor = common::memory::Register_Accessor<Register>;

using Operand_Lambda = std::function<bool(RValue)>;

/**
 * @brief Flag accessor for bit flags set on instruction indices for emission
 */
using Flag_Accessor = common::Flag_Accessor;

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
        assembly::Operand_Size size = assembly::Operand_Size::Word) override;
};

struct Instruction_Accessor : public ARM64_Instruction_Accessor
{};

struct Register_Accessor : public ARM64_Register_Accessor
{
    explicit Register_Accessor(Register* signal_register)
        : ARM64_Register_Accessor(signal_register)
    {
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

    /**
     * @brief Get a second accumulator register from a size
     */
    static constexpr Register get_second_register_from_size(
        type::semantic::Size size)
    {
        namespace m = matchit;
        return m::match(size)(
            m::pattern | 8UL = [&] { return Register::x1; },
            m::pattern | m::_ = [&] { return Register::w1; });
    }

    registers::general_purpose d_size_registers =
        registers::available_doubleword;
    registers::general_purpose w_size_registers = registers::available_word;
};

/**
 * @brief
 *
 * Notes:
 *   x26      = address-of temporary storage register
 *   x23      = The multiplication register
 *   x8       = The default "accumulator" register for expression expansion
 *   x9       = A second arithemtic scratch register
 *   x9 - x18 = Local scope variables, after which the stack is used
 *
 *   w0, x0 = Return results
 *
 *   Vectors and vector offsets are on the stack
 *
 */
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

    void reset_storage()
    {
        register_id.clear();
        id_index = 0;
    }

  private:
    void set_word_or_doubleword_register(LValue const& lvalue,
        assembly::Operand_Size size);
    Size get_size_from_temporary_rvalue_data_type(LValue const& lvalue,
        Immediate const& rvalue,
        ir::object::Function_PTR& frame);

  public:
    bool is_lvalue_allocated_in_memory(LValue const& lvalue);
    Storage get_storage_for_binary_rvalue(RValue const& rvalue);
    Storage get_storage_from_lvalue(LValue const& lvalue);
    inline Storage get_storage_from_lvalue_reference(RValue const& rvalue)
    {
        return get_storage_from_lvalue(rvalue);
    }

    Register get_available_storage_register(assembly::Operand_Size size);
    void set_lvalue_to_storage_space(LValue const& lvalue,
        Stack_Frame& stack_frame);
    void set_vector_offset_to_storage_space(LValue const& lvalue,
        Stack_Frame& stack_frame);

    inline assembly::Operand_Size get_word_size_from_storage(
        assembly::Storage const& storage,
        Stack_Frame& stack_frame)
    {
        return is_doubleword_storage_size(storage, stack_frame)
                   ? assembly::Operand_Size::Doubleword
                   : assembly::Operand_Size::Word;
    }

    constexpr void set_current_frame_symbol(Label const& label)
    {
        current_frame_symbol = label;
    }

    constexpr Label get_current_frame_name() { return current_frame_symbol; }

    inline assembly::Operand_Size get_word_size_from_lvalue(
        LValue const& lvalue,
        Stack_Frame& stack_frame)
    {
        return get_word_size_from_storage(
            get_storage_from_lvalue(lvalue), stack_frame);
    }

    bool is_doubleword_storage_size(assembly::Storage const& storage,
        Stack_Frame& stack_frame);

    Address_Accessor::Address get_lvalue_address_and_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true) override;

  private:
    Label current_frame_symbol{ "main" };
    Ordered_Map<LValue, Storage> address_table{};
    std::set<unsigned int> register_id{};
    unsigned int id_index{ 0 };
    unsigned int vector_index{ 0 };
    Register_Accessor& register_accessor_;
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
class Memory_Accessor
{
  public:
    Memory_Accessor() = delete;

    explicit Memory_Accessor(Table_Pointer table, Stack_Pointer stack_pointer)
        : table_(std::move(table))
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

  private:
    Register signal_register = assembly::Register::w0;

  private:
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
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace x86_64