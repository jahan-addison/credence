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

#include "accessor.h"
#include "assembly.h"
#include "flags.h"
#include "memory.h"
#include "stack_frame.h"
#include "syscall.h"
#include "types.h"

#include <credence/ir/object.h>
#include <credence/types.h>
#include <credence/util.h>
#include <matchit.h>
#include <memory>
#include <string>

/****************************************************************************
 *
 * Instruction inserters
 *
 * Translates algebraic data type operations such as relational, binary, and
 * unary operators into sequences of assembly instructions. Decomposes complex
 * expressions by generating the necessary instruction sub-sequences.
 *
 * Example - relational operator:
 *
 *   B code:    if (x > y) { ... }
 *
 * Inserter generates:
 *   1. Load x into register
 *   2. Compare with y
 *   3. Conditional jump based on flags
 *
 * Example - binary arithmetic:
 *
 *   B code:    result = a + b * c;
 *
 * Inserter generates instruction sequence respecting precedence:
 *   1. Multiply b * c
 *   2. Add result to a
 *   3. Store in result
 *
 ****************************************************************************/

namespace credence::target::common::assembly {

/**
 * @brief Relational Operator Inserter to translate relational operands
 */
template<memory::Memory_Accessor_T Accessor,
    Deque_T Instructions,
    typename Operands>
struct Relational_Operator_Inserter
{
    virtual ~Relational_Operator_Inserter() = default;
    explicit Relational_Operator_Inserter(
        memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }
    virtual Instructions from_relational_expression_operands(
        Operands const& operands,
        std::string const& binary_op,
        Label const& jump_label) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Bitwise Operator Inserter to translate bitwise expressions
 */
template<memory::Memory_Accessor_T Accessor,
    Pair_T Instruction_Pair,
    typename Operands>
struct Bitwise_Operator_Inserter
{
    virtual ~Bitwise_Operator_Inserter() = default;
    explicit Bitwise_Operator_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
    {
    }
    virtual Instruction_Pair from_bitwise_expression_operands(
        Operands const& operands,
        std::string const& binary_op) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
};

/**
 * @brief Invocation Inserter to translate function invocation and arguments
 */
template<memory::Memory_Accessor_T Accessor,
    Deque_T Instructions,
    Enum_T Register,
    Pair_T Instruction_Pair>
struct Invocation_Inserter
{
    virtual ~Invocation_Inserter() = default;
    explicit Invocation_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }

    using arguments_t = syscall_ns::syscall_arguments_t<Register>;

    virtual arguments_t get_operands_storage_from_argument_stack() = 0;
    virtual void insert_from_standard_library_function(std::string_view routine,
        Instructions& instructions) = 0;
    virtual void insert_from_user_defined_function(std::string_view routine,
        Instructions& instructions) = 0;
    virtual void insert_from_syscall_function(std::string_view routine,
        Instructions& instructions) = 0;
    virtual void insert_type_check_stdlib_print_arguments(
        common::memory::Locals const& argument_stack,
        arguments_t& operands) = 0;
    virtual void insert_type_check_stdlib_printf_arguments(
        common::memory::Locals const& argument_stack,
        arguments_t& operands) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Arithemtic Operator Inserter to translate arithemtic expressions
 */
template<memory::Memory_Accessor_T Accessor,
    Pair_T Instruction_Pair,
    typename Operands>
struct Arithemtic_Operator_Inserter
{
    virtual ~Arithemtic_Operator_Inserter() = default;
    explicit Arithemtic_Operator_Inserter(
        memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
    {
    }
    virtual Instruction_Pair from_arithmetic_expression_operands(
        Operands const& operands,
        std::string const& binary_op) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
};

/**
 * @brief Binary Operator Inserter to translate binary expressions
 */
template<memory::Memory_Accessor_T Accessor>
struct Binary_Operator_Inserter
{
    virtual ~Binary_Operator_Inserter() = default;
    explicit Binary_Operator_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }

    virtual void from_binary_operator_expression(RValue const& rvalue) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Unary Operator to translate unary operator expressions
 */
template<memory::Memory_Accessor_T Accessor, Enum_T R, Enum_T Size>
struct Unary_Operator_Inserter
{
    virtual ~Unary_Operator_Inserter() = default;
    explicit Unary_Operator_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }

    virtual void insert_from_unary_operator_operands(std::string const& op,
        Storage_T<R> const& dest,
        Storage_T<R> const& src = std::monostate{}) = 0;

    virtual Storage_T<R> insert_from_unary_operator_rvalue(
        RValue const& expr) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Expression Inserter to translate rvalue types into arm64 instructions
 */
template<memory::Memory_Accessor_T Accessor,
    Pair_T Instruction_Pair,
    Enum_T Mnemonic,
    Enum_T R>
struct Expression_Inserter
{
  public:
    virtual ~Expression_Inserter() = default;
    explicit Expression_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }
    virtual void insert_from_string(RValue const& str) = 0;
    virtual void insert_from_float(RValue const& str) = 0;
    virtual void insert_from_double(RValue const& str) = 0;
    virtual void insert_from_global_vector_assignment(LValue const& lhs,
        LValue const& rhs) = 0;
    virtual void insert_lvalue_at_temporary_object_address(
        LValue const& lvalue) = 0;
    virtual void insert_from_temporary_rvalue(RValue const& rvalue) = 0;
    virtual void insert_from_return_rvalue(
        ir::object::Function::Return_RValue const& ret) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Instruction Inserter used to map IR instructions to assembly
 */
template<memory::Memory_Accessor_T Accessor, typename Visitor>
class Instruction_Inserter
{
  public:
    virtual ~Instruction_Inserter() = default;
    explicit Instruction_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
    {
    }

    virtual void from_ir_instructions(
        ir::Instructions const& ir_instructions) = 0;
    virtual void setup_stack_frame_in_function(
        ir::Instructions const& ir_instructions,
        Visitor& visitor,
        int index) = 0;

  public:
    memory::Memory_Access<Accessor> accessor_;
};

/**
 * @brief Operand Inserter to insert instruction to decouple algebraic operands
 */
template<memory::Memory_Accessor_T Accessor, typename Operands, Enum_T R>
class Operand_Inserter
{
  public:
    virtual ~Operand_Inserter() = default;
    explicit Operand_Inserter(memory::Memory_Access<Accessor> accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }

  public:
    virtual Storage_T<R> get_operand_storage_from_rvalue(
        RValue const& rvalue) = 0;

  public:
    virtual void insert_from_immediate_rvalues(Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs) = 0;
    virtual void insert_from_binary_operands(Operands& operands,
        std::string const& op) = 0;
    virtual void insert_from_mnemonic_operand(LValue const& lhs,
        RValue const& rhs) = 0;

  private:
    virtual Storage_T<R> get_operand_storage_from_parameter(
        RValue const& rvalue) = 0;
    virtual Storage_T<R> get_operand_storage_from_stack(
        RValue const& rvalue) = 0;
    virtual Storage_T<R> get_operand_storage_from_return() = 0;
    virtual Storage_T<R> get_operand_storage_from_immediate(
        RValue const& rvalue) = 0;

  private:
    virtual void insert_from_string_address_operand(LValue const& lhs,
        Storage_T<R> const& storage,
        RValue const& rhs) = 0;
    virtual void insert_from_float_address_operand(LValue const& lhs,
        Storage_T<R> const& storage,
        RValue const& rhs) = 0;
    virtual void insert_from_double_address_operand(LValue const& lhs,
        Storage_T<R> const& storage,
        RValue const& rhs) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

} // namespace credence::target::common::assembly