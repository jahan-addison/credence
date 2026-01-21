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
 *
 * IR Visitor
 *
 * Abstract interface for traversing ITA (Instruction Tuple Abstraction)
 * intermediate representation and generating platform-specific assembly.
 * Each architecture implements this visitor to emit its own ISA.
 *
 *  See ir/readme.md for details on the IR.
 *
 * Example - visiting ITA instructions:
 *
 *   ITA:       x = 5;  (x is first local variable)
 *              y = x + 10;  (y is second local)
 *
 * Visitor calls:
 *   1. from_mov_ita({lvalue: "x", rvalue: "5"})
 *   2. from_mov_ita({lvalue: "y", rvalue: "x + 10"})
 *
 * x86-64 emits:  mov dword ptr [rbp - 4], 5
 *                mov eax, dword ptr [rbp - 4]
 *                add eax, 10
 *                mov dword ptr [rbp - 8], eax
 *
 * ARM64 emits:   mov w9, #5           ; x in register w9
 *                mov w8, w9
 *                add w8, w8, #10
 *                mov w10, w8          ; y in register w10
 *
 ****************************************************************************/

namespace credence::target::common {

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

} // namespace credence::target::common