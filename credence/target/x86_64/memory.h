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
#include <credence/ir/object.h>                 // for RValue, LValue, Object
#include <credence/target/common/accessor.h>    // for Accumulator_Accessor
#include <credence/target/common/flags.h>       // for Flag_Accessor
#include <credence/target/common/memory.h>      // for Operand_Type, is_imm...
#include <credence/target/common/stack_frame.h> // for Stack_Frame
#include <credence/target/common/types.h>       // for Storage_T
#include <credence/util.h>                      // for overload
#include <cstddef>                              // for size_t
#include <deque>                                // for deque
#include <functional>                           // for function
#include <matchit.h>                            // for pattern, PatternHelper
#include <memory>                               // for shared_ptr, make_shared
#include <string>                               // for basic_string, string
#include <utility>                              // for move
#include <variant>                              // for monostate, visit
namespace credence {
namespace target {
namespace x86_64 {
namespace memory {
class Memory_Accessor;
}
}
}
} // lines 38-38
namespace credence {
namespace target {
namespace x86_64 {
namespace memory {
namespace detail {
struct Instruction_Accessor;
}
}
}
}
} // lines 41-41

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
using Table_Pointer = std::shared_ptr<ir::object::Object>;

/**
 * @brief Stack frame object that keeps a stack of function calls and arguments
 */
using Stack_Frame = target::common::memory::Stack_Frame;

/**
 * @brief Get the intel-format prefix for storage device sizes
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

struct Address_Accessor : public X8664_Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        X8664_Address_Accessor::Stack_Pointer_Type& stack,
        Flag_Accessor& flag_accessor)
        : X8664_Address_Accessor(table, stack, flag_accessor)
    {
    }
    bool is_qword_storage_size(assembly::Storage const& storage,
        memory::Stack_Frame& stack_frame);

    Address_Accessor::Address get_lvalue_address_and_insertion_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true);
};

struct Vector_Accessor : public X8664_Vector_Accessor
{
  public:
    explicit Vector_Accessor(Table_Pointer& table)
        : X8664_Vector_Accessor(table)
    {
    }
    Entry_Size get_size_from_vector_offset(Immediate const& immediate) override
    {
        return assembly::get_operand_size_from_rvalue_datatype(immediate);
    }
};

struct Instruction_Accessor : public X8664_Instruction_Accessor
{};

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
     * @brief Get a second accumulator register from an Operand Size
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
 * @brief The Memory registry and mediator that orchestrates access to memory
 */
class Memory_Accessor
{
  public:
    Memory_Accessor() = delete;

    explicit Memory_Accessor(Table_Pointer table, Stack_Pointer stack_pointer)
        : table_(std::move(table))
        , stack(std::move(stack_pointer))
        , stack_frame{ table_ }
        , table_accessor(table_)
        , accumulator_accessor{ &signal_register }
        , vector_accessor{ table_ }
        , address_accessor{ table_, stack, flag_accessor }
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

  public:
    Stack_Pointer stack;
    Stack_Frame stack_frame;

  public:
    detail::Flag_Accessor flag_accessor{};
    detail::Table_Accessor table_accessor{ table_ };
    detail::Accumulator_Accessor accumulator_accessor{ &signal_register };
    detail::Vector_Accessor vector_accessor{ table_ };
    detail::Address_Accessor address_accessor{ table_, stack, flag_accessor };
    detail::Register_Accessor register_accessor{ &signal_register,
        address_accessor };
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace x86_64