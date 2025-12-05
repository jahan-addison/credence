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
#include <credence/util.h>     // for AST_Node
#include <ostream>             // for ostream
#include <utility>             // for move

namespace credence::target {

void add_stdlib_functions_to_symbols(util::AST_Node& symbols);

/**
 * @brief
 *  Abstract pure virtual target architecture class
 *
 *  * Storage_Type_Variant should encapsulate all devices that may fit in a
 * mnemonic operand
 *  * IR is the type of the data structure of IR instructions, most likely
 * ir::Quadruple
 *
 * The pure virtual methods construct a visitor of ir::ITA instructions
 */
template<typename Storage_Type_Variant, typename IR>
class Backend
{
  public:
    Backend(Backend const&) = delete;
    Backend& operator=(Backend const&) = delete;

  protected:
    virtual ~Backend() = default;
    Backend(ir::Table::Table_PTR table)
        : table(std::move(table))
    {
    }

  public:
    virtual void emit(std::ostream& os) = 0;

  protected:
    using Storage = Storage_Type_Variant;

  private:
    virtual void from_func_start_ita(type::semantic::Label const& name) = 0;
    virtual void from_func_end_ita() = 0;
    virtual void from_cmp_ita(IR const& inst) = 0;
    virtual void from_mov_ita(IR const& inst) = 0;
    virtual void from_return_ita(Storage const& storage) = 0;
    virtual void from_leave_ita() = 0;
    virtual void from_locl_ita(IR const& inst) = 0;
    virtual void from_label_ita(IR const& inst) = 0;
    virtual void from_push_ita(IR const& inst) = 0;
    // virtual void from_goto_ita() = 0;
    // virtual void from_globl_ita() = 0;
    // virtual void from_if_ita() = 0;
    // virtual void from_jmp_e_ita() = 0;
    // virtual void from_pop_ita() = 0;
    // virtual void from_call_ita() = 0;
  protected:
    ir::Table::Table_PTR table;
};

} // namespace credence::target