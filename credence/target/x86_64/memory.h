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
#include <credence/object.h>                    // for RValue, LValue, Object
#include <credence/target/common/accessor.h>    // for Accumulator_Accessor
#include <credence/target/common/flags.h>       // for Flag_Accessor
#include <credence/target/common/memory.h>      // for Operand_Type, is_imm...
#include <credence/target/common/stack_frame.h> // for Stack_Frame
#include <credence/target/common/types.h>       // for Storage_T
#include <cstddef>                              // for size_t
#include <deque>                                // for deque
#include <functional>                           // for function
#include <matchit.h>                            // for pattern, PatternHelper
#include <memory>                               // for shared_ptr, make_shared
#include <optional>                             // for optional
#include <string>                               // for basic_string, string
#include <utility>                              // for move

/****************************************************************************
 * x86-64 Memory Accessor
 *
 * The memory accessor facade of the x86-64 target, and the address of each
 * lvalue. A local is on the stack below rbp, a global is relative to rip,
 * and a vector element is an offset in its vector.
 *
 *  Example:
 *
 *    main() {
 *      auto x, arr[5];
 *      extrn value;
 *      x = value;
 *      arr[2] = 42;
 *    }
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rip + value]   // a global, from rip
 *    mov dword ptr [rbp - 4], eax       // x
 *    mov dword ptr [rbp - 12], 42       // arr[2]
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   rax      = The default "accumulator" register for expression expansion,
 *              the return value, and the number of a syscall
 *   rdi, r8, r9, rsi, rdx, rcx
 *            = The arguments of a call to a function of the program
 *   rdi, rsi, rdx, rcx, r8, r9
 *            = The arguments of a call to the standard library, System V
 *   rdi, rsi, rdx, r10, r8, r9
 *            = The arguments of a syscall
 *   xmm0 - xmm7
 *            = The float and double arguments of the standard library
 *   xmm7     = The store register of a float or double literal
 *   rcx      = The address register of the address-of operator, and the
 *              shift count of a shift by a register, in cl
 *   rdx      = The high half of the dividend and the remainder of `idiv`
 *   r10      = The address register of a dereference
 *   r11      = The scratch register of the right-hand-side of a binary
 *              expression, the divisor of `idiv`, and the value of a
 *              global argument
 *   r15      = The address of argc, argv on the stack
 *   rbp      = The frame pointer
 *
 *   Locals, parameters, vectors, and vector offsets are always on the
 *   stack below rbp
 *
 *****************************************************************************/

namespace credence::target::x86_64::memory {
class Memory_Accessor;
}
namespace credence::target::x86_64::memory::detail {
struct Instruction_Accessor;
} // namespace credence::target::x86_64::memory::detail

namespace credence::target::x86_64 {

using Register = assembly::Register;
using Directive = assembly::Directive;
using Mnemonic = assembly::Mnemonic;
using Storage = common::Storage_T<assembly::Register>;
using Operator_Symbol = std::string;
using Operand_Size = assembly::Operand_Size;
using Immediate = assembly::Immediate;
using Directives = assembly::Directives;

/**
 * Short-form helpers for matchit predicate pattern matching
 */
// Re-export common predicates from common::memory
using common::memory::is_immediate;
using common::memory::is_parameter;
using common::memory::is_temporary;
using common::memory::is_vector_offset;

namespace memory {

using Operand_Type = common::memory::Operand_Type;

namespace registers {

using general_purpose = std::deque<Register>;

const general_purpose available_qword_register = { Register::rdi,
    Register::r8,
    Register::r9,
    Register::rsi,
    Register::rdx,
    Register::rcx };
const general_purpose available_dword_register = { Register::edi,
    Register::r8d,
    Register::r9d,
    Register::esi,
    Register::edx,
    Register::ecx };

} // namespace registers

using Memory_Access = std::shared_ptr<Memory_Accessor>;
using Instruction_Pointer = std::shared_ptr<detail::Instruction_Accessor>;
using Stack_Pointer = std::shared_ptr<assembly::Stack>;
using Table_Pointer = std::shared_ptr<object::Object_Table>;
using Stack_Frame = common::memory::Stack_Frame;

using X8664_Memory_Accessor = common::memory::Memory_Accessor;

/**
 * @brief Get the Intel syntax size prefix of a memory operand
 *
 *  Examples:
 *
 *    storage_prefix_from_operand_size(Operand_Size::Dword)   // dword ptr
 *    storage_prefix_from_operand_size(Operand_Size::Qword)   // qword ptr
 */
constexpr std::string storage_prefix_from_operand_size(
    assembly::Operand_Size size)
{
    namespace m = matchit;
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return "qword ptr"; },
        m::pattern | Operand_Size::Dword = [&] { return "dword ptr"; },
        m::pattern | Operand_Size::Word = [&] { return "word ptr"; },
        m::pattern | Operand_Size::Byte = [&] { return "byte ptr"; },
        m::pattern | m::_ = [&] { return "dword ptr"; });
}

namespace detail {

using X8664_Address_Accessor =
    common::memory::Address_Accessor<assembly::Register,
        assembly::Stack,
        assembly::Instruction_Pair>;

using X8664_Accumulator_Accessor =
    common::memory::Accumulator_Accessor<assembly::Operand_Size,
        assembly::Register,
        assembly::Stack>;

using X8664_Instruction_Accessor =
    common::memory::Instruction_Accessor<assembly::Instructions>;

using X8664_Vector_Accessor =
    common::memory::Vector_Accessor<assembly::Operand_Size>;

using Buffer_Accessor = common::memory::Buffer_Accessor;

using Table_Accessor = common::memory::Table_Accessor;

using X8664_Register_Accessor = common::memory::Register_Accessor<Register>;

using Operand_Lambda = std::function<bool(RValue)>;

using Flag_Accessor = common::Flag_Accessor;

/**
 * @brief The accumulator register of x86-64, al, ax, eax, or rax
 *
 * A signal register set by an inserter is returned once in place of the
 * accumulator, and the signal register is eax when it is not set
 */
struct Accumulator_Accessor : public X8664_Accumulator_Accessor
{
    explicit Accumulator_Accessor(Register* signal_register)
        : X8664_Accumulator_Accessor(signal_register)
    {
    }
    assembly::Operand_Size get_operand_size_from_immediate(
        Immediate const& immediate) override
    {
        return assembly::get_operand_size_from_rvalue_datatype(immediate);
    }

    Register get_accumulator_register_from_size(
        Operand_Size size = Operand_Size::Dword) override
    {
        namespace m = matchit;
        if (*signal_register_ != Register::eax) {
            auto designated = *signal_register_;
            *signal_register_ = Register::eax;
            return designated;
        }
        return m::match(size)(
            m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
            m::pattern | Operand_Size::Word = [&] { return Register::ax; },
            m::pattern | Operand_Size::Byte = [&] { return Register::al; },
            m::pattern | m::_ = [&] { return Register::eax; });
    }
};

/**
 * @brief The address of an lvalue and the instructions that load it
 *
 * A local is its offset below rbp, a global is relative to rip with its
 * size prefix, and a dereference is the address loaded into rax
 *
 *  Example:
 *
 *    x = value;
 *
 *  Becomes, where value is global:
 *
 *    dword ptr [rip + value]
 */
struct Address_Accessor : public X8664_Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        X8664_Address_Accessor::Stack_Pointer_Type& stack,
        Flag_Accessor& flag_accessor,
        Stack_Frame& stack_frame)
        : X8664_Address_Accessor(table, stack, flag_accessor, stack_frame)
    {
    }
    bool is_qword_storage_size(assembly::Storage const& storage) const;

    Address_Accessor::Address get_lvalue_address_and_insertion_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true);
};

/**
 * @brief The offset and operand size of an element of a global vector on
 * x86-64
 */
struct Vector_Accessor : public X8664_Vector_Accessor
{
  public:
    explicit Vector_Accessor(Table_Pointer& table, Stack_Frame& stack_frame)
        : X8664_Vector_Accessor(table, stack_frame)
    {
    }
    Entry_Size get_size_from_vector_offset(Immediate const& immediate) override
    {
        return assembly::get_operand_size_from_rvalue_datatype(immediate);
    }
};

/**
 * @brief The x86-64 instructions of the program
 */
struct Instruction_Accessor : public X8664_Instruction_Accessor
{};

/**
 * @brief The argument registers of a call to a function of the program
 *
 * Each argument takes the next register of its size, and the registers are
 * reset after the call
 */
struct Register_Accessor
{
    explicit Register_Accessor(Register* signal_register,
        Address_Accessor& address_accessor)
        : signal_register(signal_register)
        , address_accessor(address_accessor)
        , available_qword(registers::available_qword_register)
        , available_dword(registers::available_dword_register)
    {
    }

    Storage get_register_for_binary_operator(RValue const& rvalue,
        Stack_Pointer& stack);
    Storage get_available_register(Operand_Size size, Stack_Pointer& stack);

    void reset_available_registers()
    {
        available_qword = registers::available_qword_register;
        available_dword = registers::available_dword_register;
    }

    /**
     * @brief Get the second operand register of a size, edi or rdi
     */
    static constexpr Register get_second_register_from_size(Operand_Size size)
    {
        namespace m = matchit;
        return m::match(size)(
            m::pattern | Operand_Size::Qword = [&] { return Register::rdi; },
            m::pattern | Operand_Size::Word = [&] { return Register::di; },
            m::pattern | Operand_Size::Byte = [&] { return Register::dil; },
            m::pattern | m::_ = [&] { return Register::edi; });
    }

    Register* signal_register;
    Address_Accessor& address_accessor;
    registers::general_purpose available_qword;
    registers::general_purpose available_dword;
};

} // namespace detail

/**
 * @brief The memory accessor facade of the x86-64 target
 *
 * The inserters, visitor, and emitter share one facade, which holds each
 * accessor, the stack, the instructions of the program, and the state that
 * is passed from one IR instruction to the next
 */
class Memory_Accessor final : public X8664_Memory_Accessor
{
  public:
    Memory_Accessor() = delete;

    explicit Memory_Accessor(Table_Pointer table,
        ir::Instructions_PTR instructions,
        Stack_Pointer stack_pointer)
        : X8664_Memory_Accessor(table)
        , table_(std::move(table))
        , instructions_(std::move(instructions))
        , stack(std::move(stack_pointer))
        , table_accessor(table_, instructions_)
        , accumulator_accessor{ &signal_register }
        , vector_accessor{ table_, get_frame_in_memory() }
        , address_accessor{ table_,
            stack,
            flag_accessor,
            get_frame_in_memory() }
        , register_accessor{ &signal_register, address_accessor }
    {
        instruction_accessor = std::make_shared<detail::Instruction_Accessor>();
    }

    constexpr void set_signal_register(Register signal_)
    {
        signal_register = signal_;
    }

  private:
    Register signal_register = assembly::Register::eax;

  private:
    Table_Pointer table_;
    ir::Instructions_PTR instructions_;

  public:
    Stack_Pointer stack;

  public:
    // the temporary and the register that holds it after the instruction
    // that computes it, valid until the next instruction
    std::optional<std::pair<LValue, Register>> accumulator_temporary{};
    // a relational expression inserted the conditional jump of the next IF
    bool relational_jump{ false };

  public:
    detail::Flag_Accessor flag_accessor{};
    detail::Table_Accessor table_accessor{ table_, instructions_ };
    detail::Accumulator_Accessor accumulator_accessor{ &signal_register };
    detail::Vector_Accessor vector_accessor{ table_, get_frame_in_memory() };
    detail::Address_Accessor address_accessor{ table_,
        stack,
        flag_accessor,
        get_frame_in_memory() };
    detail::Register_Accessor register_accessor{ &signal_register,
        address_accessor };
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace x86_64