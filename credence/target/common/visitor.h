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

#include <string>

/****************************************************************************
 * IR Visitor
 *
 * The interface each target implements to translate the ITA, one IR
 * instruction at a time, into the instructions of its ISA. See ir/README.md
 * for details on the IR.
 *
 *  Example:
 *
 *    x = (5:int:4);
 *    _t2 = x + (10:int:4);
 *    y = _t2;
 *
 *  Becomes, with one from_mov_ita call for each instruction, on x86-64:
 *
 *    mov dword ptr [rbp - 4], 5
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, 10
 *    mov dword ptr [rbp - 8], eax
 *
 *  And on arm64, where x and y are in w9 and w10:
 *
 *    mov w9, #5
 *    mov w8, w9
 *    add w8, w8, #10
 *    mov w10, w8
 *
 ****************************************************************************/

namespace credence::target::common {

/**
 * @brief A visitor of each ITA instruction kind
 *
 * The code generator calls the visitor of each instruction in order, and
 * the visitor appends the target's instructions through its memory accessor
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
    virtual void from_return_ita(IR const& inst) = 0;
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

} // namespace credence::target::common