/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <credence/ir/table.h> // for Table
#include <credence/types.h>    // for LValue, RValue, Data_Type, ...
#include <credence/util.h>     // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <ostream>             // for ostream
#include <utility>             // for move

namespace credence {

namespace target {

constexpr unsigned int align_up_to_16(unsigned int n)
{
    const unsigned int ALIGNMENT = 16;
    const unsigned int MASK = ALIGNMENT - 1; // MASK = 15 (0xF)
    // The logic is: (n + 15) & ~15
    return (n + MASK) & (~MASK);
}

template<typename S>
class Backend
{
  public:
    Backend(Backend const&) = delete;
    Backend& operator=(Backend const&) = delete;

  protected:
    Backend(ir::Table::Table_PTR table)
        : table_(std::move(table))
    {
    }

    virtual ~Backend() = default;

  public:
    using IR_Instruction = ir::Quadruple;
    virtual void emit(std::ostream& os) = 0;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    virtual void from_func_start_ita(type::semantic::Label const& name) = 0;
    virtual void from_func_end_ita() = 0;
    virtual void from_cmp_ita(IR_Instruction const& inst) = 0;
    virtual void from_mov_ita(IR_Instruction const& inst) = 0;
    virtual void from_return_ita(S const& storage) = 0;
    virtual void from_leave_ita() = 0;
    virtual void from_locl_ita(IR_Instruction const& inst) = 0;
    virtual void from_label_ita(IR_Instruction const& inst) = 0;
    virtual void from_push_ita(IR_Instruction const& inst) = 0;
    // virtual void from_goto_ita() = 0;
    // virtual void from_globl_ita() = 0;
    // virtual void from_if_ita() = 0;
    // virtual void from_jmp_e_ita() = 0;
    // virtual void from_pop_ita() = 0;
    // virtual void from_call_ita() = 0;
  CREDENCE_PROTECTED_UNLESS_TESTED:
    ir::Table::Table_PTR table_;
};

} // namespace target

} // namespace credence