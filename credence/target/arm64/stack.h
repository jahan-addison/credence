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
 * ARM64 Stack Management
 *
 * Manages the ARM64 PCS-compliant stack. Stack grows downward. Must maintain
 * 16-byte alignment. ARM64 has many registers (x0-x30), so prioritizes
 * register allocation before using stack. Callee-saved registers (x19-x28)
 * are preserved on stack when used.
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
 *   x0 = parameter 'a'
 *   x9 = local 'x'
 *   x10 = local 'y'
 *   x11 = local 'z'
 *
 * Stack only used if >10 locals, or for saved x9-x18:
 *   [sp + 24] saved x9 (before function calls)
 *   [sp + 16] saved x10
 *   [sp + 8]  saved x11
 *
 * ARM64 allocates stack space upfront: sub sp, sp, #32
 *
 *****************************************************************************/

#pragma once

#include "assembly.h"                     // for Operand_Size, Immediate
#include "credence/ir/object.h"           // for LValue, Size, Vector (ptr ...
#include <credence/target/common/types.h> // for base_stack_pointer
#include <memory>                         // for unique_ptr
#include <string>                         // for string
#include <utility>                        // for pair

namespace credence::target::arm64::assembly {

/**
 * @brief
 * A push-down stack for the arm64 architecture
 *
 * Since there are so many more registers available, we use as many as possible
 * before allocating on the stack. Then and only then do we grow the stack frame
 * allocation size.
 *
 * Vectors, both global and local, will use the stack.
 *
 * Designated callee-saved registers are saved on the stack
 *
 * This stack type works different from the x86864 one, in that you must
 * allocate the amount you need from a stack frame upfront, and then offsets
 * push downwards.
 *
 * Uses the PIMPL (Pointer to Implementation) idiom to reduce compilation
 * dependencies and improve build times.
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

    Operand_Size get_operand_size_from_offset(Offset offset) const;

    void set_address_from_immediate(LValue const& lvalue,
        Immediate const& rvalue);
    void set_address_from_accumulator(LValue const& lvalue, Register acc);
    void set_address_from_type(LValue const& lvalue, Type type);

    void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        Operand_Size operand_size);
    void set_address_from_address(LValue const& lvalue);

    Size get_stack_frame_allocation_size(ir::object::Function_PTR& frame);
    Size get_stack_offset_from_table_vector_index(LValue const& lvalue,
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