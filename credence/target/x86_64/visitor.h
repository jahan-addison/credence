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

namespace credence::target::x86_64 {

using X8664_IR_Visitor =
    common::IR_Visitor<ir::Quadruple, assembly::Instructions>;

/**
 * @brief IR Visitor for the x86-64 architecture and ISA
 *
 * The Storage_Container is defined in assembly.h, and
 * each intermediate instruction is a quadruple defined in ita.h.
 *
 * Macros and helpers to compose mnemonics, registers, and immediate
 * values instructions are defined in assembly.h.
 *
 */
class IR_Instruction_Visitor final : public X8664_IR_Visitor
{
  public:
    explicit IR_Instruction_Visitor(memory::Memory_Access& accessor)
        : accessor_(accessor)
        , stack_frame_(accessor_->stack_frame)
    {
    }

    using Instructions = assembly::Instructions;
    void set_stack_frame_from_table(Label const& function_name);

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