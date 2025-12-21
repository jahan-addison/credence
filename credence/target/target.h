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

enum class Platform
{
    credence_x86_64_platform,
    credence_arm64_platform,
    credence_z80_platform
};

void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    Platform platform);

/**
 * @brief
 * The pure virtual methods construct a visitor of ir::ITA::Instructions
 */
template<typename IR, typename Instructions>
class IR_Visitor
{
  public:
    IR_Visitor() = default;
    IR_Visitor(IR_Visitor const&) = delete;
    IR_Visitor& operator=(IR_Visitor const&) = delete;

  public:
    virtual ~IR_Visitor() = default;

  public:
    virtual void from_func_start_ita(std::string const& name) = 0;
    virtual void from_func_end_ita() = 0;
    virtual void from_cmp_ita(IR const& inst) = 0;
    virtual void from_mov_ita(IR const& inst) = 0;
    virtual void from_return_ita() = 0;
    virtual void from_leave_ita() = 0;
    virtual void from_label_ita(IR const& inst) = 0;
    virtual void from_call_ita(IR const& inst) = 0;
    virtual void from_goto_ita(IR const& inst) = 0;
    virtual void from_if_ita(IR const& inst) = 0;
    virtual void from_jmp_e_ita(IR const& inst) = 0;
    virtual void from_push_ita(IR const& inst) = 0;
    virtual void from_locl_ita(IR const& inst) = 0;
    virtual void from_pop_ita() = 0;
};

} // namespace credence::target