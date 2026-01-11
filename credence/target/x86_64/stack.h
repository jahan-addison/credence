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

/****************************************************************************
 *
 * x86-64 Stack Management
 *
 * Manages the System V ABI-compliant stack for x86-64. Stack grows downward
 * from high to low addresses. Must maintain 16-byte alignment before calls.
 *
 * Example - function with locals:
 *
 *   B code:
 *     compute(a) {
 *       auto x, y, z;
 *       x = a * 2;
 *       y = x + 10;
 *       z = y - 5;
 *       return(z);
 *     }
 *
 * Stack layout:
 *   [rbp + 16] parameter 'a'
 *   [rbp + 8]  return address (pushed by call)
 *   [rbp + 0]  saved rbp (pushed by function prologue)
 *   [rbp - 8]  local 'x'
 *   [rbp - 16] local 'y'
 *   [rbp - 24] local 'z'
 *   [rbp - 32] alignment padding (16-byte aligned)
 *
 *****************************************************************************/

#pragma once

#include "assembly.h"                     // for Operand_Size, Immediate
#include "credence/ir/object.h"           // for LValue, Size, Vector (ptr ...
#include <credence/target/common/types.h> // for Stack_Offset, base_stack_p...
#include <memory>                         // for unique_ptr
#include <string>                         // for string
#include <utility>                        // for pair

namespace credence::target::x86_64::assembly {

/**
 * @brief
 * A push-down stack for the x86-64 architecture
 *
 * Provides a means to allocate, traverse, and verify offsets
 * that auto-align on the stack by lvalues and vice-versa.
 *
 * Uses the PIMPL (Pointer to Implementation) idiom to reduce compilation
 * dependencies and improve build times.
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
        ir::object::Vector const& vector);
    Size get_stack_size_from_table_vector(ir::object::Vector const& vector);
    std::string get_lvalue_from_offset(Offset offset) const;

  public:
    void clear();

  private:
    class Stack_IMPL;
    std::unique_ptr<Stack_IMPL> pimpl;
};

} // namespace credence::target::x86_64::assembly
