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
#include <credence/ir/object.h>             // for Label
#include <credence/target/common/visitor.h> // for IR_Visitor
#include <cstddef>                          // for size_t

/****************************************************************************
 *
 * ARM64 IR Visitor
 *
 * Visits ITA intermediate representation instructions and emits ARM64
 * machine code. Implements the IR_Visitor interface for ARM64 ISA.
 *
 * Example - visiting assignment:
 *
 *   ITA:    x = 42;  (x is first local variable)
 *
 * Visitor generates:
 *   mov w9, #42              ; x in register w9
 *
 * Example - visiting function call:
 *
 *   ITA:    CALL add
 *
 * Visitor generates:
 *   bl add
 *
 *****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x6  = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x15      = Second data section register
 *   x7       = multiplication scratch register
 *   x8       = The default "accumulator" register for expression expansion
 *   x10      = The stack move register; additional scratch register
 *   x9 - x18 = If there are no function calls in a stack frame, local scope
 *             variables are stored in x9-x18, after which the stack is used
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64 {

using ARM64_IR_Visitor =
    common::IR_Visitor<ir::Quadruple, assembly::Instructions>;

/**
 * @brief IR Visitor for the arm64 architecture ISA
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
    void set_stack_frame_from_table(Label const& function_name);

  public:
    constexpr void set_iterator_index(std::size_t index)
    {
        iterator_index_ = index;
    }

  private:
    void set_pointer_address_of_lvalue(LValue const& lvalue);

  private:
    bool parameters_in_return_stack(Label const& label);
    unsigned int* get_function_frame_calls(Label const& label);

  public:
    void from_locl_ita(ir::Quadruple const& inst) override;
    void from_pop_ita() override;
    void from_push_ita(ir::Quadruple const& inst) override;
    void from_func_start_ita(Label const& name) override;
    void from_func_end_ita() override;
    void from_cmp_ita(ir::Quadruple const& inst) override;
    void from_mov_ita(ir::Quadruple const& inst) override;
    void from_return_ita() override;
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