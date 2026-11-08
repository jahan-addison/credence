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

#include "assembly.h"                       // for Instructions
#include "credence/ir/ita.h"                // for Quadruple
#include "memory.h"                         // for Memory_Access, Memory_Ac...
#include <credence/language/hir/binding.h>  // for Label
#include <credence/target/common/visitor.h> // for IR_Visitor
#include <cstddef>                          // for size_t

/****************************************************************************
 * x86-64 IR Visitor
 *
 * Translates each ITA instruction into x86-64 instructions. Every local is
 * on the stack below rbp, and the value of an expression is in the
 * accumulator, eax or rax.
 *
 *  Example:
 *
 *    PUSH _p5_4;
 *    PUSH _p4_3;
 *    CALL add;
 *    POP 16;
 *    _t8 = RET;
 *
 *  Becomes:
 *
 *    mov edi, dword ptr [rbp - 4]
 *    mov r8d, 2
 *    call add
 *    mov dword ptr [rbp - 8], eax
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

namespace credence::target::x86_64 {

using X8664_IR_Visitor =
    common::IR_Visitor<ir::Quadruple, assembly::Instructions>;

/**
 * @brief The IR visitor of the x86-64 ISA
 *
 * The visitor of a MOV dispatches on the kind of its lvalue and rvalue to
 * an inserter, and the accumulator holds the value of a temporary until the
 * next instruction, so a temporary used later is stored on the stack
 */
class IR_Instruction_Visitor final : public X8664_IR_Visitor
{
  public:
    explicit IR_Instruction_Visitor(memory::Memory_Access& accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->get_frame_in_memory())
    {
    }

    using Instructions = assembly::Instructions;

  public:
    constexpr void set_iterator_index(std::size_t index)
    {
        iterator_index_ = index;
    }

  public:
    void from_locl_ita(ir::Quadruple const& inst) override;
    void from_pop_ita() override;
    void from_push_ita(ir::Quadruple const& inst) override;
    void from_func_start_ita(Label const& name) override;
    void from_func_end_ita() override;
    void from_cmp_ita(ir::Quadruple const& inst) override;
    void from_mov_ita(ir::Quadruple const& inst) override;
    void from_return_ita(ir::Quadruple const& inst) override;
    void from_leave_ita() override;
    void from_label_ita(ir::Quadruple const& inst) override;
    void from_call_ita(ir::Quadruple const& inst) override;
    void from_if_ita(ir::Quadruple const& inst) override;
    void from_jmp_e_ita(ir::Quadruple const& inst) override;
    void from_goto_ita(ir::Quadruple const& inst) override;

  private:
    bool is_temporary_used_after_next_instruction(LValue const& temporary);
    void insert_temporary_to_stack(LValue const& temporary, LValue const& slot);

  private:
    std::size_t iterator_index_{ 0 };

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

}