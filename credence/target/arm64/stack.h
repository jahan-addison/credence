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
#include <credence/object.h>              // for LValue, Size, Vector (ptr ...
#include <credence/target/common/types.h> // for base_stack_pointer
#include <memory>                         // for unique_ptr
#include <string>                         // for string
#include <utility>                        // for pair

/****************************************************************************
 * ARM64 Stack
 *
 * The stack slots of a function's frame, as offsets from the stack pointer.
 * The frame record, x29 and x30, is at the bottom of the frame, each slot is
 * aligned to its size above it, and the frame size is aligned up to 16
 * bytes.
 *
 *  Example:
 *
 *    main() {
 *      auto a, *p, c;
 *      a = 1;
 *      p = &a;
 *      c = 3;
 *      printf("%d\n", a + c);
 *    }
 *
 *  Becomes:
 *
 *    [sp, #0]    the frame record, x29 and x30
 *    [sp, #16]   a, a word
 *    [sp, #24]   p, a doubleword aligned to 8
 *    [sp, #32]   c
 *    [sp, #36]   the argument a + c
 *
 *    stp x29, x30, [sp, #-48]!
 *
 ****************************************************************************/

namespace credence::target::arm64::assembly {

/**
 * @brief The stack slots of the function under translation
 *
 * Each lvalue on the stack has an offset from the stack pointer and an
 * operand size. The allocation of each function is recorded at its end, for
 * its prologue and epilogue. The implementation is a pimpl to improve build
 * times.
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

    Offset allocate(Operand_Size operand);
    Offset allocate(Size alloc);

    void set_aad_local_size(Size alloc);
    Size get_aad_local_size();

    Operand_Size get_operand_size_from_offset(Offset offset) const;

    void set_stack_frame_allocation_size(Label const& label);
    Size get_stack_frame_allocation_size(Label const& label);
    common::Stack_Offset get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        object::Vector const& vector);
    Size get_stack_size_from_table_vector(object::Vector const& vector);

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