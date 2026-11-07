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
 * ARM64 IR Visitor
 *
 * Translates each ITA instruction into ARM64 instructions. A function's
 * locals are in registers when its frame makes no calls, and on the stack
 * otherwise, and the value of an expression is in the accumulator, w8 or x8.
 *
 *  Example:
 *
 *    _p4_3 = (1:int:4);
 *    _p5_4 = (2:int:4);
 *    PUSH _p5_4;
 *    PUSH _p4_3;
 *    CALL compute;
 *    POP 16;
 *    _t8 = RET;
 *
 *  Becomes:
 *
 *    mov x0, #1
 *    mov x1, #2
 *    bl compute
 *    mov w8, w0
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x0 - x7  = The arguments of a call, and x0 the return value
 *   x6       = The data section address register, and the second operand
 *              of a binary expression in a temporary
 *      s6    = floating point
 *      d6    = double
 *   x7       = The scratch register of the right-hand-side of a binary
 *              expression
 *   x8       = The default "accumulator" register for expression expansion
 *   x9, x11 - x14, x16, x17
 *            = If there are no function calls in a stack frame, local scope
 *              variables are stored in these registers, after which the
 *              stack is used
 *   x10      = The stack move register, and the address register of a
 *              pointer on the stack
 *   x15      = The address of a vector element on the stack
 *   x18      = The platform register on Darwin, never used
 *   x19      = The argc, argv scratch register
 *
 *   A local whose address is taken, vectors, and vector offsets are always
 *   on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64 {

using ARM64_IR_Visitor =
    common::IR_Visitor<ir::Quadruple, assembly::Instructions>;

/**
 * @brief The IR visitor of the ARM64 ISA
 *
 * The visitor of a MOV dispatches on the kind of its lvalue and rvalue to
 * an inserter, and the accumulator holds the value of a temporary until the
 * next instruction, so a temporary used later is stored on the stack
 */
class IR_Instruction_Visitor final : public ARM64_IR_Visitor
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

  private:
    bool is_temporary_used_after_next_instruction(LValue const& temporary);
    void insert_temporary_to_stack(LValue const& temporary, LValue const& slot);
    void insert_to_pointer(LValue const& pointer, RValue const& rvalue);

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
    std::size_t iterator_index_{ 0 };

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

}