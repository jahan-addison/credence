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

#include <credence/language/hir/binding.h>
#include <credence/types.h>
#include <credence/util.h>
#include <matchit.h>
#include <memory>
#include <string>

/****************************************************************************
 * Instruction Inserters
 *
 * The interfaces of the inserters each target implements. An inserter
 * translates one kind of IR operand or expression, a binary or unary
 * expression, an operand, an expression, or a call, into instructions. The
 * IR is already in three-address form, so each inserter translates one
 * operation, and the order of operations is the order of the IR.
 *
 *  Example:
 *
 *    _t4 = b * c;
 *    _t5 = a + _t4;
 *    r = _t5;
 *    _t6 = - r;
 *
 *  Becomes, on x86-64:
 *
 *    mov eax, dword ptr [rbp - 8]      // the binary operator inserter
 *    imul eax, dword ptr [rbp - 12]
 *    mov r11d, eax                     // _t4 is the right-hand-side
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, r11d
 *    mov dword ptr [rbp - 16], eax     // the operand inserter
 *    mov eax, dword ptr [rbp - 16]     // the unary operator inserter
 *    neg eax
 *
 ****************************************************************************/

namespace credence::target::common::assembly {

/**
 * @brief Translate a comparison of two operands into a conditional jump
 *
 *  Example:
 *
 *    JMP_E _t2 (1:int:4) _L5;
 *
 *  Becomes, on x86-64:
 *
 *    cmp eax, 1
 *    je ._L5__main
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
 * @brief Translate a bitwise expression of two operands
 *
 * The result is in the accumulator register
 *
 *  Example:
 *
 *    _t2 = x & (3:int:4);
 *
 *  Becomes, on arm64:
 *
 *    mov w8, w9
 *    mov w7, #3
 *    and w8, w8, w7
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
 * @brief Translate a call and its arguments
 *
 * The arguments are on the argument stack of the frame, pushed by PUSH, and
 * are loaded into the argument registers before the call. A call is to a
 * function of the program, the standard library, or a syscall, and the
 * arguments of `print` and `printf` are type checked as buffers.
 *
 *  Example:
 *
 *    PUSH _p3_2;
 *    PUSH _p2_1;
 *    CALL add;
 *
 *  Becomes, on x86-64:
 *
 *    mov edi, dword ptr [rbp - 20]
 *    mov r8d, 4
 *    call add
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
 * @brief Translate an arithmetic expression of two operands
 *
 * The result is in the accumulator register
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
 * @brief Translate the binary expression of a temporary
 *
 * The left-hand-side is loaded into the accumulator and the right-hand-side
 * is its operand, and an expression of two immediates is folded into one
 * immediate
 *
 *  Example:
 *
 *    _t5 = a + _t4;
 *
 *  Becomes, on x86-64:
 *
 *    mov r11d, eax
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, r11d
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
 * @brief Translate a unary expression
 *
 * The operators are `-`, `+`, `~`, `!`, `++`, `--`, `*`, and `&`
 *
 *  Example:
 *
 *    _t6 = - r;
 *
 *  Becomes, on x86-64:
 *
 *    mov eax, dword ptr [rbp - 16]
 *    neg eax
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
 * @brief Translate the rvalue of a temporary or a return
 *
 * The rvalue is a literal of the data section, an element of a global
 * vector, the return value of a call, or an expression whose result is
 * stored at the address of its temporary
 *
 *  Example:
 *
 *    s = ("hello":string:5);
 *
 *  Becomes, on arm64:
 *
 *    adrp x6, ._L_str1__@PAGE          // the expression inserter
 *    add x6, x6, ._L_str1__@PAGEOFF
 *    mov x11, x6                       // the operand inserter
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
        language::hir::binding::Function::Return_RValue const& ret) = 0;

  protected:
    memory::Memory_Access<Accessor> accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Translate the IR instructions of a program in order
 *
 * Each IR instruction is passed to its visitor, and the stack frame of a
 * function is set up at its `BeginFunc`
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
 * @brief Load and store the operands of an assignment
 *
 * An operand is resolved to its storage device: a parameter register, a
 * stack offset, the return register, or an immediate. An assignment of a
 * string, float, or double literal stores the address of its label.
 *
 *  Example:
 *
 *    r = _t5;
 *
 *  Becomes, on x86-64:
 *
 *    mov dword ptr [rbp - 16], eax
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