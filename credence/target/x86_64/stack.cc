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

#include "stack.h"
#include "assembly.h"                     // for get_size_from_operand_size
#include <algorithm>                      // for find_if, __find_if
#include <credence/map.h>                 // for Ordered_Map
#include <credence/object.h>              // for LValue, Vector, Size, Type
#include <credence/target/common/types.h> // for Stack_Offset, base_stack_p...
#include <credence/types.h>               // for Size
#include <credence/util.h>                // for align_up_to_16, align_up_to_8
#include <fmt/compile.h>                  // for format, operator""_cf
#include <numeric>                        // for accumulate
#include <string>                         // for basic_string, string, oper...
#include <utility>                        // for pair

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

class Stack::Stack_IMPL
{
  public:
    using Offset = common::Stack_Offset;
    using Entry = std::pair<Offset, assembly::Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

    constexpr void clear() { stack_address.clear(); }

    constexpr bool empty_at(LValue const& lvalue)
    {
        return stack_address[lvalue].second == assembly::Operand_Size::Empty;
    }
    constexpr bool contains(LValue const& lvalue)
    {
        return stack_address.contains(lvalue);
    }
    constexpr bool is_allocated(LValue const& lvalue)
    {
        return stack_address.contains(lvalue) and not empty_at(lvalue);
    }
    /**
     * @brief Get the offset and size of the slot of an lvalue
     */
    constexpr Entry get(LValue const& lvalue) { return stack_address[lvalue]; }

    /**
     * @brief Get the offset and size of the slot at an offset
     *
     * An offset with no slot is empty
     */
    constexpr Entry get(Offset offset) const
    {
        auto find = std::find_if(stack_address.begin(),
            stack_address.end(),
            [&](Pair const& entry) { return entry.second.first == offset; });
        if (find == stack_address.end())
            return { 0, assembly::Operand_Size::Empty };
        else
            return find->second;
    }

    /**
     * @brief Set the operand size of an offset with no lvalue, such as a
     * vector element
     */
    constexpr void set(Offset offset, Operand_Size size)
    {
        using namespace fmt::literals;
        stack_address.insert(
            fmt::format("__internal_vector_offset_{}"_cf, ++vectors),
            Entry{ offset, size });
    }

    /**
     * @brief Allocate an operand size below the last allocation
     *
     *  Example:
     *
     *    allocate(Operand_Size::Dword)   // 4, [rbp - 4]
     *    allocate(Operand_Size::Qword)   // 12, [rbp - 12]
     */
    constexpr Offset allocate(assembly::Operand_Size operand)
    {
        auto alloc = get_size_from_operand_size(operand);
        size += alloc;
        return size;
    }

    /**
     * @brief Get the operand size of the slot at an offset
     */
    constexpr assembly::Operand_Size get_operand_size_from_offset(
        Offset offset) const
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            assembly::Operand_Size::Empty,
            [&](assembly::Operand_Size size, Pair const& entry) {
                if (entry.second.first == offset)
                    return entry.second.second;
                return size;
            });
    }

    /**
     * @brief Allocate a slot for an lvalue of the size of an immediate
     *
     * An lvalue that has a slot keeps it
     */
    constexpr void set_address_from_immediate(LValue const& lvalue,
        assembly::Immediate const& rvalue)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        auto operand_size =
            assembly::get_operand_size_from_rvalue_datatype(rvalue);
        auto value_size = assembly::get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    /**
     * @brief Allocate a slot for an lvalue of the size of a register
     */
    constexpr void set_address_from_accumulator(LValue const& lvalue,
        assembly::Register acc)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        auto register_size = assembly::get_operand_size_from_register(acc);
        auto allocation = assembly::get_size_from_operand_size(register_size);

        allocate_aligned_lvalue(lvalue, allocation, register_size);
    }

    /**
     * @brief Allocate a slot for an lvalue of the size of a type
     */
    constexpr void set_address_from_type(LValue const& lvalue, Type type)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        auto operand_size = assembly::get_operand_size_from_type(type);
        auto value_size = assembly::get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    /**
     * @brief Allocate a slot for an lvalue below the last allocation
     *
     * An offset may already be taken by a vector element set in a chunk, so
     * the slot is the next offset that is free
     */
    constexpr void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        assembly::Operand_Size operand_size)
    {
        size += value_size;
        while (!get_lvalue_from_offset(size).empty())
            size += value_size;
        stack_address.insert(lvalue, { size, operand_size });
    }

    /**
     * @brief Allocate a quadword slot for a pointer, aligned to 8 bytes
     */
    constexpr void set_address_from_address(LValue const& lvalue)
    {
        auto qword_size = assembly::Operand_Size::Qword;
        size = util::align_up_to_8(
            size + assembly::get_size_from_operand_size(qword_size));
        stack_address.insert(lvalue, { size, qword_size });
    }

    /**
     * @brief Get the size of the frame below rbp
     *
     * The return address and rbp are pushed before the frame, so the frame
     * is 8 bytes off a 16 byte boundary and rsp is aligned at each call
     */
    constexpr Size get_stack_frame_allocation_size()
    {
        // the frame is 8 bytes off a 16 byte boundary, and covers every local
        return size > 16 ? util::align_up_to_16(size + 8UL) - 8UL : 16UL;
    }

    /**
     * @brief Get the offset of an element of a vector on the stack
     *
     * The vector is allocated in one chunk, and each element is below the
     * one before it by the size of its value
     */
    Size get_stack_offset_from_table_vector_index(LValue const& lvalue,
        std::string const& key,
        object::Vector const& vector)
    {
        bool search{ false };
        auto vector_offset = get(lvalue).first;
        return std::accumulate(vector.data.begin(),
            vector.data.end(),
            vector_offset,
            [&](type::semantic::Size offset,
                object::Vector::Entry const& entry) {
                if (entry.first == key)
                    search = true;
                if (!search)
                    offset -= assembly::get_size_from_operand_size(
                        assembly::get_operand_size_from_rvalue_datatype(
                            entry.second));
                return offset;
            });
    }

    /**
     * @brief Get the allocation of a vector on the stack
     *
     * The allocation is the sum of the sizes of its elements, aligned to 16
     * bytes when it is larger than 16
     */
    Size get_stack_size_from_table_vector(object::Vector const& vector)
    {
        auto vector_size =
            size +
            std::accumulate(vector.data.begin(),
                vector.data.end(),
                0UL,
                [&](type::semantic::Size offset,
                    object::Vector::Entry const& entry) {
                    return offset +
                           assembly::get_size_from_operand_size(
                               assembly::get_operand_size_from_rvalue_datatype(
                                   entry.second));
                });

        return vector_size < 16UL ? vector_size
                                  : util::align_up_to_16(vector_size);
    }

    /**
     * @brief Allocate a slot of a size for an lvalue
     *
     * An lvalue that has a slot keeps it
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Offset allocate,
        assembly::Operand_Size operand = assembly::Operand_Size::Dword)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        size += allocate;
        stack_address.insert(lvalue, { size, operand });
    }

    /**
     * @brief Get the lvalue of the slot at an offset, or empty
     */
    constexpr std::string get_lvalue_from_offset(Offset offset) const
    {
        auto search = std::ranges::find_if(stack_address.begin(),
            stack_address.end(),
            [&](Pair const& pair) { return pair.second.first == offset; });
        if (search != stack_address.end())
            return search->first;
        else
            return "";
    }

  private:
    int vectors{ 0 };
    Offset size{ 0 };
    Local stack_address{};
};

Stack::Stack()
    : pimpl{ std::make_unique<Stack_IMPL>() }
{
}

Stack::~Stack() = default;

Stack::Stack(Stack&&) noexcept = default;

Stack& Stack::operator=(Stack&&) noexcept = default;

void Stack::clear()
{
    pimpl->clear();
}
bool Stack::empty_at(LValue const& lvalue)
{
    return pimpl->empty_at(lvalue);
}
bool Stack::contains(LValue const& lvalue)
{
    return pimpl->contains(lvalue);
}
bool Stack::is_allocated(LValue const& lvalue)
{
    return pimpl->is_allocated(lvalue);
}
Stack::Entry Stack::get(LValue const& lvalue)
{
    return pimpl->get(lvalue);
}
Stack::Entry Stack::get(Offset offset) const
{
    return pimpl->get(offset);
}
void Stack::set(Offset offset, Operand_Size size)
{
    pimpl->set(offset, size);
}
Stack::Offset Stack::allocate(assembly::Operand_Size operand)
{
    return pimpl->allocate(operand);
}
assembly::Operand_Size Stack::get_operand_size_from_offset(Offset offset) const
{
    return pimpl->get_operand_size_from_offset(offset);
}
void Stack::set_address_from_immediate(LValue const& lvalue,
    assembly::Immediate const& rvalue)
{
    pimpl->set_address_from_immediate(lvalue, rvalue);
}
void Stack::set_address_from_accumulator(LValue const& lvalue,
    assembly::Register acc)
{
    pimpl->set_address_from_accumulator(lvalue, acc);
}
void Stack::set_address_from_type(LValue const& lvalue, Type type)
{
    pimpl->set_address_from_type(lvalue, type);
}
void Stack::allocate_aligned_lvalue(LValue const& lvalue,
    Size value_size,
    assembly::Operand_Size operand_size)
{
    pimpl->allocate_aligned_lvalue(lvalue, value_size, operand_size);
}
void Stack::set_address_from_address(LValue const& lvalue)
{
    pimpl->set_address_from_address(lvalue);
}
Size Stack::get_stack_frame_allocation_size()
{
    return pimpl->get_stack_frame_allocation_size();
}
Size Stack::get_stack_offset_from_table_vector_index(LValue const& lvalue,
    std::string const& key,
    object::Vector const& vector)
{
    return pimpl->get_stack_offset_from_table_vector_index(lvalue, key, vector);
}
Size Stack::get_stack_size_from_table_vector(object::Vector const& vector)
{
    return pimpl->get_stack_size_from_table_vector(vector);
}
void Stack::set_address_from_size(LValue const& lvalue,
    Offset allocate,
    assembly::Operand_Size operand)
{
    pimpl->set_address_from_size(lvalue, allocate, operand);
}
std::string Stack::get_lvalue_from_offset(Offset offset) const
{
    return pimpl->get_lvalue_from_offset(offset);
}

} // namespace credence::target::x86_64::assembly
