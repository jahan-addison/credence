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