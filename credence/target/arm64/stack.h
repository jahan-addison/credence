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

#include "assembly.h"                     // for Operand_Size, Immediate
#include "credence/ir/object.h"           // for LValue, Size, Vector (ptr ...
#include <credence/target/common/types.h> // for base_stack_pointer
#include <memory>                         // for unique_ptr
#include <string>                         // for string
#include <utility>                        // for pair

/****************************************************************************
 *
 * ARM64 Stack
 *
 * A push-down stack that grows downward and maintains 16-byte alignment.
 * Since ARM64 has so many registers (x0-x30), we prioritize register allocation
 * before using the stack. Vectors and their elements will always be allocated
 * in whole on the stack.
 *
 * NOTE: We store x9-x18 on the stack before calling a function via the
 * Allocate, Access, Deallocate pattern
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
 * Register allocation (locals use x9-x18 first):
 *   w0 = parameter 'a'
 *   w9 = local 'x'
 *   w10 = local 'y'
 *   w11 = local 'z'
 *
 *   [sp + 16] saved w9 (before function calls)
 *   [sp + 12] saved w10
 *   [sp + 8]  saved w11
 *
 *****************************************************************************/

namespace credence::target::arm64::assembly {

/**
 * @brief
 * Uses the PIMPL to improve build times.
 */
class Stack : public common::detail::base_stack_pointer
{
  public:
    explicit Stack();
    ~Stack();
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;
    Stack(Stack&&) noexcept;
    Stack& operator=(Stack&&) noexcept;

  public:
    using Entry = std::pair<Offset, Operand_Size>;

  public:
    void clear();

  public:
    bool empty_at(LValue const& lvalue);
    bool contains(LValue const& lvalue);
    bool is_allocated(LValue const& lvalue);

    Entry get(LValue const& lvalue);
    Entry get(Offset offset) const;

    void set(Offset offset, Operand_Size size);

    void allocate(Operand_Size operand);
    void allocate(Size alloc);
    void deallocate(Size alloc);

    void set_aad_local_size(Size alloc);
    Size get_aad_local_size();

    Operand_Size get_operand_size_from_offset(Offset offset) const;

    void set_address_from_immediate(LValue const& lvalue,
        Immediate const& rvalue);
    void set_address_from_accumulator(LValue const& lvalue, Register acc);
    void set_address_from_type(LValue const& lvalue, Type type);

    void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        Operand_Size operand_size);
    void set_address_from_address(LValue const& lvalue);

    Size get_stack_frame_allocation_size();
    common::Stack_Offset get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        ir::object::Vector const& vector);
    Size get_stack_size_from_table_vector(ir::object::Vector const& vector);
    Size get_offset_from_pushdown_stack();

    void set_address_from_size(LValue const& lvalue,
        Offset offset_address,
        Operand_Size operand = Operand_Size::Word);
    void set_address_from_size(LValue const& lvalue,
        Operand_Size operand = Operand_Size::Word);

    void allocate_pointer_on_stack();
    void add_address_location_to_stack(LValue const& lvalue);

    std::string get_lvalue_from_offset(Offset offset) const;

  private:
    class Stack_IMPL;
    std::unique_ptr<Stack_IMPL> pimpl;
};

} // namespace credence::target::arm64::assembly