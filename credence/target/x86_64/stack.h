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

#include "assembly.h"                      // for Operand_Size, Immediate
#include <credence/language/hir/binding.h> // for LValue, Size, Vector (ptr ...
#include <credence/target/common/types.h>  // for Stack_Offset, base_stack_p...
#include <memory>                          // for unique_ptr
#include <string>                          // for string
#include <utility>                         // for pair

/****************************************************************************
 * x86-64 Stack
 *
 * The stack slots of a function's frame, as offsets below rbp. The stack
 * grows down, and the frame size keeps rsp 16-byte aligned at each call, as
 * the System V ABI requires.
 *
 *  Example:
 *
 *    compute(a, b) {
 *      auto x, y, z;
 *      x = a + b;
 *      y = x * 2;
 *      z = y - a;
 *      return(z);
 *    }
 *
 *  Becomes:
 *
 *    [rbp + 8]     the return address
 *    [rbp + 0]     the saved rbp
 *    [rbp - 4]     x
 *    [rbp - 8]     y
 *    [rbp - 12]    z
 *
 *    edi, r8d      the parameters a and b, in their argument registers
 *
 ****************************************************************************/

namespace credence::target::x86_64::assembly {

/**
 * @brief The stack slots of the function under translation
 *
 * Each lvalue on the stack has an offset below rbp and an operand size. An
 * offset is the bottom of its slot, so each allocation is below the last.
 * The implementation is a pimpl to improve build times.
 */
class Stack : public common::detail::base_stack_pointer
{
  public:
    explicit Stack();
    Stack(Stack&&) noexcept;
    Stack(Stack const&) = delete;
    Stack& operator=(Stack&&) noexcept;
    Stack& operator=(Stack const&) = delete;
    ~Stack();

  public:
    using Offset = common::Stack_Offset;
    using Entry = std::pair<Offset, Operand_Size>;

  public:
    bool empty_at(LValue const& lvalue);
    bool contains(LValue const& lvalue);
    bool is_allocated(LValue const& lvalue);
    Entry get(LValue const& lvalue);
    Entry get(Offset offset) const;

  public:
    Offset allocate(assembly::Operand_Size operand);

  public:
    void set(Offset offset, Operand_Size size);
    void set_address_from_immediate(LValue const& lvalue,
        assembly::Immediate const& rvalue);
    void set_address_from_accumulator(LValue const& lvalue,
        assembly::Register acc);
    void set_address_from_type(LValue const& lvalue, Type type);
    void set_address_from_size(LValue const& lvalue,
        Offset allocate,
        Operand_Size operand = Operand_Size::Dword);
    void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        Operand_Size operand_size);
    void set_address_from_address(LValue const& lvalue);

  public:
    Operand_Size get_operand_size_from_offset(Offset offset) const;
    Size get_stack_frame_allocation_size();
    Size get_stack_offset_from_table_vector_index(LValue const& lvalue,
        std::string const& key,
        language::hir::binding::Vector const& vector);
    Size get_stack_size_from_table_vector(
        language::hir::binding::Vector const& vector);
    std::string get_lvalue_from_offset(Offset offset) const;

  public:
    void clear();

  private:
    class Stack_IMPL;
    std::unique_ptr<Stack_IMPL> pimpl;
};

} // namespace credence::target::x86_64::assembly
