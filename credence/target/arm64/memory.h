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
// Re-export common predicates from common::memory
using common::memory::is_immediate;
using common::memory::is_parameter;
using common::memory::is_temporary;
using common::memory::is_vector_offset;

namespace memory {

using Operand_Type = common::memory::Operand_Type;

namespace registers {

using general_purpose = std::deque<Register>;

const general_purpose available_doubleword = {
    Register::x0,
    Register::x1,
    Register::x2,
    Register::x3,
    Register::x4,
    Register::x5,
    Register::x6,
    Register::x7,
};
const general_purpose available_word = {
    Register::w0,
    Register::w1,
    Register::w2,
    Register::w3,
    Register::w4,
    Register::w5,
    Register::w6,
    Register::w7,
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

    /**
     * @brief Get the accumulator register from size
     */
    Register get_accumulator_register_from_size(
        assembly::Operand_Size size = assembly::Operand_Size::Word) override
    {
        namespace m = matchit;
        if (*signal_register_ != Register::w0) {
            auto designated = *signal_register_;
            *signal_register_ = Register::w0;
            return designated;
        }
        return m::match(size)(
            m::pattern | assembly::Operand_Size::Doubleword =
                [&] { return Register::x0; },
            m::pattern | assembly::Operand_Size::Halfword =
                [&] { return Register::w0; }, // No 16-bit direct
            m::pattern | assembly::Operand_Size::Byte =
                [&] { return Register::w0; }, // No 8-bit direct
            m::pattern | m::_ = [&] { return Register::w0; });
    }
};

struct Address_Accessor : public ARM64_Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        ARM64_Address_Accessor::Stack_Pointer_Type& stack,
        Flag_Accessor& flag_accessor)
        : ARM64_Address_Accessor(table, stack, flag_accessor)
    {
    }

    bool is_doubleword_storage_size(assembly::Storage const& storage,
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
                           result =
                               type::is_rvalue_data_type_string(s) or
                               assembly::is_immediate_pc_relative_address(s);
                       },
                   },
            storage);

        return result;
    }

    Address_Accessor::Address get_lvalue_address_and_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true) override;
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

struct Instruction_Accessor : public ARM64_Instruction_Accessor
{};

struct Register_Accessor : public ARM64_Register_Accessor
{
    explicit Register_Accessor(Register* signal_register)
        : ARM64_Register_Accessor(signal_register)
    {
    }

    Storage get_register_for_binary_operator(RValue const& rvalue,
        Stack_Pointer& stack);
    Storage get_available_register(assembly::Operand_Size size,
        Stack_Pointer& stack);

    void reset_available_registers()
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
        , address_accessor{ table_, stack, flag_accessor }
        , register_accessor{ &signal_register }
    {
        instruction_accessor = std::make_shared<detail::Instruction_Accessor>();
    }
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
    detail::Address_Accessor address_accessor{ table_, stack, flag_accessor };
    detail::Register_Accessor register_accessor{ &signal_register };
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace x86_64