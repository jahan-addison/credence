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
#include <credence/language/hir/binding.h>      // for RValue, LValue, Func...
#include <credence/target/common/inserter.h>    // for Arithemtic_Operator_...
#include <credence/target/common/stack_frame.h> // for Locals
#include <deque>                                // for deque
#include <optional>                             // for optional
#include <string>                               // for basic_string, string
#include <string_view>                          // for string_view
#include <utility>                              // for make_pair

/****************************************************************************
 * x86-64 Instruction Inserters
 *
 * Translates each kind of IR expression into x86-64 instructions. See
 * common/inserter.h for the interfaces. The value of an expression is in the
 * accumulator, eax or rax, and a right-hand-side temporary is in r11d or
 * r11.
 *
 *  Example:
 *
 *    _t2 = y * (2:int:4);
 *    _t3 = x + _t2;
 *    z = _t3;
 *    _t7 = z > (10:int:4);
 *    IF _t7 GOTO _L6;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 8]
 *    imul eax, 2
 *    mov r11d, eax                    // _t2 is the right-hand-side
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, r11d
 *    mov dword ptr [rbp - 12], eax
 *    mov eax, dword ptr [rbp - 12]
 *    cmp eax, 10
 *    jg ._L6__main                    // the IF is fused with its comparison
 *
 ****************************************************************************/

namespace credence::target::x86_64 {

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

using X8664_Relational_Operator_Inserter =
    common::assembly::Relational_Operator_Inserter<memory::Memory_Accessor,
        Instructions,
        assembly::Binary_Operands>;

using X8664_Bitwise_Operator_Inserter =
    target::common::assembly::Bitwise_Operator_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        assembly::Binary_Operands>;

using X8664_Invocation_Inserter =
    target::common::assembly::Invocation_Inserter<memory::Memory_Accessor,
        Instructions,
        Register,
        Instruction_Pair>;

using X8664_Expression_Inserter =
    target::common::assembly::Expression_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        Mnemonic,
        Register>;

using X8664_Arithemtic_Operator_Inserter =
    common::assembly::Arithemtic_Operator_Inserter<memory::Memory_Accessor,
        Instruction_Pair,
        assembly::Binary_Operands>;

using X8664_Binary_Operator_Inserter =
    target::common::assembly::Binary_Operator_Inserter<memory::Memory_Accessor>;

using X8664_Unary_Operator_Inserter = target::common::assembly::
    Unary_Operator_Inserter<memory::Memory_Accessor, Register, Operand_Size>;

using X8664_Instruction_Inserter =
    target::common::assembly::Instruction_Inserter<memory::Memory_Accessor,
        IR_Instruction_Visitor>;

using X8664_Operand_Inserter =
    target::common::assembly::Operand_Inserter<memory::Memory_Accessor,
        assembly::Binary_Operands,
        Register>;

/**
 * @brief Get the operand size of a storage device
 */
assembly::Operand_Size get_operand_size_from_storage(
    assembly::Storage const& storage,
    memory::Stack_Pointer& stack);

/**
 * @brief Get the immediates of two literal operands
 */
inline auto get_rvalue_pair_as_immediate(RValue const& lhs, RValue const& rhs)
{
    return std::make_pair(type::get_rvalue_datatype_from_string(lhs),
        type::get_rvalue_datatype_from_string(rhs));
}

/**
 * @brief Translate a comparison of two operands into a conditional jump
 *
 * See common/inserter.h for details
 */
struct Relational_Operator_Inserter : public X8664_Relational_Operator_Inserter
{
    explicit Relational_Operator_Inserter(memory::Memory_Access accessor)
        : X8664_Relational_Operator_Inserter(accessor)
    {
    }
    Instructions from_relational_expression_operands(
        assembly::Binary_Operands const& operands,
        std::string const& binary_op,
        Label const& jump_label) override;
};

/**
 * @brief Translate a bitwise expression of two operands
 */
struct Bitwise_Operator_Inserter : public X8664_Bitwise_Operator_Inserter
{
    explicit Bitwise_Operator_Inserter(memory::Memory_Access accessor)
        : X8664_Bitwise_Operator_Inserter(accessor)
    {
    }
    Instruction_Pair from_bitwise_expression_operands(
        assembly::Binary_Operands const& operands,
        std::string const& binary_op) override;
};

/**
 * @brief Translate a call and its arguments
 *
 * The arguments are resolved to their devices before any is loaded into
 * its register, so an argument that would be overwritten, such as a
 * parameter in an argument register, is stored in an argument slot first
 */
struct Invocation_Inserter : public X8664_Invocation_Inserter
{
    explicit Invocation_Inserter(memory::Memory_Access accessor)
        : X8664_Invocation_Inserter(accessor)
    {
    }
    arguments_t get_operands_storage_from_argument_stack() override;
    bool is_global_argument(RValue const& rvalue);
    common::Stack_Offset insert_global_argument_to_stack(RValue const& rvalue);
    common::Stack_Offset insert_parameter_argument_to_stack(
        RValue const& rvalue);
    void insert_from_standard_library_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_from_user_defined_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_from_syscall_function(std::string_view routine,
        Instructions& instructions) override;
    void insert_type_check_stdlib_print_arguments(
        common::memory::Locals const& argument_stack,
        X8664_Invocation_Inserter::arguments_t& operands) override;
    void insert_type_check_stdlib_printf_arguments(
        common::memory::Locals const& argument_stack,
        X8664_Invocation_Inserter::arguments_t& operands) override;

  private:
    unsigned int argument_slot_index{ 0 };
};

/**
 * @brief Translate an arithmetic expression of two operands
 */
struct Arithemtic_Operator_Inserter : public X8664_Arithemtic_Operator_Inserter
{
    explicit Arithemtic_Operator_Inserter(memory::Memory_Access accessor)
        : X8664_Arithemtic_Operator_Inserter(accessor)
    {
    }
    Instruction_Pair from_arithmetic_expression_operands(
        assembly::Binary_Operands const& operands,
        std::string const& binary_op) override;
};

/**
 * @brief Translate the binary expression of a temporary
 *
 * The left-hand-side is in the accumulator and the right-hand-side is its
 * operand, and the result is in the accumulator, or a conditional jump when
 * the next instruction is the IF of a relational expression
 */
struct Binary_Operator_Inserter : X8664_Binary_Operator_Inserter
{
    explicit Binary_Operator_Inserter(memory::Memory_Access accessor)
        : X8664_Binary_Operator_Inserter(accessor)
    {
    }
    void from_binary_operator_expression(RValue const& rvalue) override;

  public:
    // the register that holds the value of the expression, empty when a
    // relational expression inserted a conditional jump instead
    std::optional<Register> result{};

  private:
    void insert_from_division_operands(Register accumulator,
        Storage divisor,
        std::string const& op);
    void insert_from_relational_operands(Register accumulator,
        Storage const& rhs,
        std::string const& op);
    void insert_from_logical_operands(Register accumulator,
        Storage rhs,
        std::string const& op);
    void insert_from_shift_operands(Register accumulator,
        Storage const& rhs,
        std::string const& op);
};

/**
 * @brief Translate a unary expression
 */
struct Unary_Operator_Inserter : public X8664_Unary_Operator_Inserter
{
    explicit Unary_Operator_Inserter(memory::Memory_Access accessor)
        : X8664_Unary_Operator_Inserter(accessor)
    {
    }

    void insert_from_unary_operator_operands(std::string const& op,
        Storage const& dest,
        Storage const& src = assembly::O_NUL) override;

    Storage insert_from_unary_operator_rvalue(RValue const& expr) override;

  private:
    Storage get_temporary_storage_from_temporary_expansion(
        RValue const& rvalue);
    void from_lvalue_address_of_expression(RValue const& expr);
};

/**
 * @brief Translate the rvalue of a temporary, a return, or a global vector
 * assignment
 */
struct Expression_Inserter : public X8664_Expression_Inserter
{
    explicit Expression_Inserter(memory::Memory_Access accessor)
        : X8664_Expression_Inserter(accessor)
    {
    }

    void insert_from_string(RValue const& str) override;
    void insert_from_float(RValue const& str) override;
    void insert_from_double(RValue const& str) override;
    void insert_from_global_vector_assignment(LValue const& lhs,
        LValue const& rhs) override;
    void insert_from_rvalue(RValue const& rvalue);
    void insert_lvalue_at_temporary_object_address(
        LValue const& lvalue) override;
    void insert_from_temporary_rvalue(
        [[maybe_unused]] RValue const& rvalue) override
    {
    }
    void insert_from_return_rvalue(
        language::hir::binding::Function::Return_RValue const& ret) override;

  private:
    std::optional<Register> insert_from_unary_value(RValue const& rvalue);
    std::optional<Register> insert_from_address_of_value(RValue const& rvalue);

  private:
    // the register that holds the value of the last temporary rvalue
    std::optional<Register> result_{};
};

/**
 * @brief Translate the IR instructions of a program in order
 */
class Instruction_Inserter : public X8664_Instruction_Inserter
{
  public:
    explicit Instruction_Inserter(memory::Memory_Access accessor)
        : X8664_Instruction_Inserter(accessor)
    {
    }
    void from_ir_instructions(ir::Instructions const& ir_instructions) override;
    void setup_stack_frame_in_function(ir::Instructions const& ir_instructions,
        IR_Instruction_Visitor& visitor,
        int index) override;
};

/**
 * @brief Load and store the operands of an assignment
 *
 * An operand is loaded into its device or the accumulator, and the
 * accumulator is stored to an lvalue at the size of its slot
 */
class Operand_Inserter : public X8664_Operand_Inserter
{
  public:
    explicit Operand_Inserter(memory::Memory_Access accessor)
        : X8664_Operand_Inserter(accessor)
    {
    }

  public:
    Storage get_operand_storage_from_rvalue(RValue const& rvalue) override;
    Storage get_operand_storage_from_expression_operand(RValue const& rvalue);
    Register insert_operand_into_accumulator(RValue const& rvalue);
    void insert_from_accumulator_to_lvalue(LValue const& lhs,
        Register accumulator);
    Size get_pointee_size(LValue const& pointer);

  public:
    void insert_from_immediate_rvalues(Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs) override;
    void insert_from_binary_operands(assembly::Binary_Operands& operands,
        std::string const& op) override;
    void insert_from_mnemonic_operand(LValue const& lhs,
        RValue const& rhs) override;
    Storage get_operand_storage_from_parameter(RValue const& rvalue) override;

  private:
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
