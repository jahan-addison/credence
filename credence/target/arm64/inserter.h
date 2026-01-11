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

#include "assembly.h"                           // for Assignment_Operands
#include "credence/ir/ita.h"                    // for Instructions
#include "credence/types.h"                     // for get_rvalue_datatype_...
#include "memory.h"                             // for Memory_Accessor, Sto...
#include "visitor.h"                            // for IR_Instruction_Visitor
#include <credence/ir/object.h>                 // for RValue, LValue, Func...
#include <credence/target/common/inserter.h>    // for Arithemtic_Operator_...
#include <credence/target/common/stack_frame.h> // for Locals
#include <deque>                                // for deque
#include <string>                               // for basic_string, string
#include <string_view>                          // for string_view
#include <utility>                              // for make_pair

namespace credence::target::arm64 {

namespace {

using Instructions = assembly::Instructions;
using Instruction = assembly::Instruction;
using Operand_Size = assembly::Operand_Size;
using Instruction_Pair = assembly::Instruction_Pair;
using Mnemonic = assembly::Mnemonic;
using Storage = assembly::Storage;
using Immediate = assembly::Immediate;
using Operand_Stack = std::deque<Storage>;

} // namespace

using ARM64_Relational_Operator_Inserter =
    common::assembly::Relational_Operator_Inserter<memory::Memory_Accessor,
        Instructions,
        assembly::Assignment_Operands>;

using ARM64_Bitwise_Operator_Inserter =
    target::common::assembly::Bitwise_Operator_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        assembly::Ternary_Operands>;

using ARM64_Invocation_Inserter =
    target::common::assembly::Invocation_Inserter<memory::Memory_Accessor,
        Instructions,
        Register,
        Instruction_Pair>;

using ARM64_Expression_Inserter =
    target::common::assembly::Expression_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        Mnemonic,
        Register>;

using ARM64_Arithemtic_Operator_Inserter =
    common::assembly::Arithemtic_Operator_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        assembly::Assignment_Operands>;

using ARM64_Binary_Operator_Inserter =
    target::common::assembly::Binary_Operator_Inserter<memory::Memory_Accessor>;

using ARM64_Unary_Operator_Inserter = target::common::assembly::
    Unary_Operator_Inserter<memory::Memory_Accessor, Register, Operand_Size>;

using ARM64_Instruction_Inserter =
    target::common::assembly::Instruction_Inserter<memory::Memory_Accessor,
        IR_Instruction_Visitor>;

using ARM64_Operand_Inserter =
    target::common::assembly::Operand_Inserter<memory::Memory_Accessor,
        assembly::Assignment_Operands,
        Register>;

/************************/
// ---------------------
/************************/

inline auto get_rvalue_pair_as_immediate(RValue const& lhs, RValue const& rhs)
{
    return std::make_pair(type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

/**
 *  See target/common/inserter.h for details
 */

struct Relational_Operator_Inserter : public ARM64_Relational_Operator_Inserter
{
    explicit Relational_Operator_Inserter(memory::Memory_Access accessor)
        : ARM64_Relational_Operator_Inserter(accessor)
    {
    }
    Instructions from_relational_expression_operands(
        assembly::Assignment_Operands const& operands,
        std::string const& binary_op,
        Label const& jump_label) override;
};

struct Bitwise_Operator_Inserter : public ARM64_Bitwise_Operator_Inserter
{
    explicit Bitwise_Operator_Inserter(memory::Memory_Access accessor)
        : ARM64_Bitwise_Operator_Inserter(accessor)
    {
    }
    Instruction_Pair from_bitwise_expression_operands(
        assembly::Ternary_Operands const& operands,
        std::string const& binary_op) override;

    void from_bitwise_temporary_expression(RValue const& expr);

  private:
    void get_operand_stack_from_temporary_lvalue(LValue const& lvalue,
        Operand_Stack& stack);
};

struct Invocation_Inserter : public ARM64_Invocation_Inserter
{
    explicit Invocation_Inserter(memory::Memory_Access accessor)
        : ARM64_Invocation_Inserter(accessor)
    {
    }
    arguments_t get_operands_storage_from_argument_stack() override;
    void insert_from_standard_library_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_from_user_defined_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_from_syscall_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_type_check_stdlib_print_arguments(
        common::memory::Locals const& argument_stack,
        ARM64_Invocation_Inserter::arguments_t& operands) override;
    void insert_type_check_stdlib_printf_arguments(
        common::memory::Locals const& argument_stack,
        ARM64_Invocation_Inserter::arguments_t& operands) override;
};

struct Arithemtic_Operator_Inserter : public ARM64_Arithemtic_Operator_Inserter
{
    explicit Arithemtic_Operator_Inserter(memory::Memory_Access accessor)
        : ARM64_Arithemtic_Operator_Inserter(accessor)
    {
    }
    Instruction_Pair from_arithmetic_expression_operands(
        assembly::Assignment_Operands const& operands,
        std::string const& binary_op) override;
};

struct Binary_Operator_Inserter : ARM64_Binary_Operator_Inserter
{
    explicit Binary_Operator_Inserter(memory::Memory_Access accessor)
        : ARM64_Binary_Operator_Inserter(accessor)
    {
    }
    void from_binary_operator_expression(RValue const& rvalue) override;
};

struct Unary_Operator_Inserter : public ARM64_Unary_Operator_Inserter
{
    explicit Unary_Operator_Inserter(memory::Memory_Access accessor)
        : ARM64_Unary_Operator_Inserter(accessor)
    {
    }

    void insert_from_unary_to_unary_assignment(LValue const& lhs,
        LValue const& rhs);

    Operand_Size get_operand_size_from_lvalue_reference(LValue const& lvalue);
    void insert_from_unary_operator_operands(std::string const& op,
        Storage const& dest,
        Storage const& src = assembly::O_NUL) override;
    common::Stack_Offset from_lvalue_address_of_expression(RValue const& expr);
    Storage insert_from_unary_operator_rvalue(RValue const& expr) override;

  private:
    Storage get_temporary_storage_from_temporary_expansion(
        RValue const& rvalue);
};

struct Expression_Inserter : public ARM64_Expression_Inserter
{
    explicit Expression_Inserter(memory::Memory_Access accessor)
        : ARM64_Expression_Inserter(accessor)
    {
    }

    void insert_from_string(RValue const& str) override;
    void insert_from_float(RValue const& str) override;
    void insert_from_double(RValue const& str) override;
    void insert_from_global_vector_assignment(LValue const& lhs,
        LValue const& rhs) override;
    void insert_lvalue_at_temporary_object_address(
        LValue const& lvalue) override;
    void insert_from_temporary_rvalue(RValue const& rvalue) override;
    void insert_from_return_rvalue(
        ir::object::Function::Return_RValue const& ret) override;

  private:
    std::deque<LValue> stack{};
};

class Instruction_Inserter : public ARM64_Instruction_Inserter
{
  public:
    explicit Instruction_Inserter(memory::Memory_Access accessor)
        : ARM64_Instruction_Inserter(accessor)
    {
    }
    void from_ir_instructions(ir::Instructions const& ir_instructions) override;
    void setup_stack_frame_in_function(ir::Instructions const& ir_instructions,
        IR_Instruction_Visitor& visitor,
        int index) override;
};

class Operand_Inserter : public ARM64_Operand_Inserter
{
  public:
    explicit Operand_Inserter(memory::Memory_Access accessor)
        : ARM64_Operand_Inserter(accessor)
    {
    }

  public:
    Storage get_operand_storage_from_rvalue(RValue const& rvalue) override;

  public:
    void insert_from_immediate_rvalues(Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs) override;
    void insert_from_binary_operands(assembly::Assignment_Operands& operands,
        std::string const& op) override;
    void insert_from_mnemonic_operand(LValue const& lhs,
        RValue const& rhs) override;

  private:
    Storage get_operand_storage_from_parameter(RValue const& rvalue) override;
    Storage get_operand_storage_from_stack(RValue const& rvalue) override;
    Storage get_operand_storage_from_return() override;
    Storage get_operand_storage_from_immediate(RValue const& rvalue) override;

  private:
    void insert_from_string_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs) override;
    void insert_from_float_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs) override;
    void insert_from_double_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs) override;
};

}
