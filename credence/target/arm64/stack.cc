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
#include "assembly.h"                      // for Operand_Size, get_size_fr...
#include <algorithm>                       // for find_if, min, __find_if
#include <credence/map.h>                  // for Ordered_Map
#include <credence/object.h>               // for LValue, Size, Vector, Type
#include <credence/target/common/memory.h> // for align_up_to
#include <credence/target/common/types.h>  // for base_stack_pointer
#include <credence/types.h>                // for Size
#include <credence/util.h>                 // for align_up_to_16
#include <cstddef>                         // for size_t
#include <fmt/base.h>                      // for println
#include <fmt/compile.h>                   // for format, operator""_cf
#include <iterator>                        // for distance
#include <numeric>                         // for accumulate
#include <string>                          // for basic_string, string, ope...
#include <utility>                         // for pair

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

class Stack::Stack_IMPL
{
  public:
    using Entry = std::pair<Offset, Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

    constexpr void clear()
    {
        size = 0;
        stack_address.clear();
    }

    constexpr bool empty_at(LValue const& lvalue)
    {
        return stack_address[lvalue].second == Operand_Size::Empty;
    }
    constexpr bool contains(LValue const& lvalue)
    {
        return stack_address.contains(lvalue);
    }
    constexpr bool is_allocated(LValue const& lvalue)
    {
        return stack_address.contains(lvalue) and not empty_at(lvalue);
    }

    constexpr void set_aad_local_size(Size alloc) { aad_local_size = alloc; }
    constexpr Size get_aad_local_size() { return aad_local_size; }

    /**
     * @brief Get the offset and size of the slot of an lvalue
     */
    Entry get(LValue const& lvalue)
    {

        auto address = stack_address[lvalue];
        address.first += aad_local_size;
        return address;
    }

    /**
     * @brief Get the offset and size of the slot at an offset
     *
     * An offset with no slot is empty
     */
    Entry get(Offset offset) const
    {
        auto find = std::find_if(stack_address.begin(),
            stack_address.end(),
            [&](Pair const& entry) { return entry.second.first == offset; });

        if (find == stack_address.end())
            return { 0, Operand_Size::Empty };

        return { find->second.first + aad_local_size, find->second.second };
    }

    /**
     * @brief Set the operand size of an offset with no lvalue
     *
     * The size of an allocation, a vector element, or an argument is kept by
     * offset, so a load from the offset has its size
     */
    constexpr void set(Offset offset, Operand_Size operand)
    {
        using namespace fmt::literals;
        stack_address.insert(fmt::format("__internal_offset_{}"_cf, ++vectors),
            Entry{ offset, operand });
    }

    /**
     * @brief Allocate space on the stack
     *
     * The offset is the start of the allocation, aligned to its size up to a
     * doubleword, so a load or store at the offset stays inside it
     *
     *  Example:
     *
     *    allocate(4)   // 16, after the frame record
     *    allocate(8)   // 24, aligned to 8
     */
    constexpr Offset allocate(Operand_Size operand)
    {
        return allocate(get_size_from_operand_size(operand));
    }

    constexpr Offset allocate(Size alloc)
    {
        auto offset = common::memory::align_up_to(size, std::min(alloc, 8UL));
        size = offset + alloc;
        set(offset, assembly::get_operand_size_from_size(alloc));
        return offset;
    }

    /**
     * @brief Get the operand size of the slot at an offset
     */
    constexpr Operand_Size get_operand_size_from_offset(Offset offset) const
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            Operand_Size::Empty,
            [&](Operand_Size size, Pair const& entry) {
                if (entry.second.first == offset)
                    return entry.second.second;
                return size;
            });
    }

    /**
     * @brief Get the frame size of a function, aligned up to 16 bytes
     *
     * A frame with no allocation still has its frame record
     */
    Size get_stack_frame_allocation_size(Label const& label)
    {
        credence_assert(allocation_table.contains(label));
        auto allocation_size = allocation_table.at(label);
        if (allocation_size == 0)
            return 16UL;
        else
            return common::memory::align_up_to(allocation_size, 16);
    }

    /**
     * @brief Record the allocation of a function at its end
     */
    void set_stack_frame_allocation_size(Label const& label)
    {
        allocation_table.insert(label, size);
    }

    /**
     * @brief Get the offset of an element of a vector on the stack
     *
     * The vector is allocated in one chunk, and its elements are in
     * ascending 8 byte slots from its offset
     *
     *  Example:
     *
     *    get_stack_offset_from_table_vector_index("v", "2", vector)
     *    // the offset of v + 16
     */
    common::Stack_Offset get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        object::Vector const& vector)
    {
        // a pointer increment is the next element
        if (util::is_numeric(key))
            return get(lvalue).first + (8UL * std::stoul(key));
        auto search = std::ranges::find_if(
            vector.data, [&](object::Vector::Entry const& entry) {
                return entry.first == key;
            });
        return get(lvalue).first +
               (8UL * static_cast<std::size_t>(
                          std::distance(vector.data.begin(), search)));
    }

    /**
     * @brief Get the allocation of a vector on the stack
     *
     * Each element is an 8 byte slot, with 16 bytes after the elements
     */
    Size get_stack_size_from_table_vector(object::Vector const& vector)
    {
        return 16UL + (8UL * std::max(vector.size, vector.data.size()));
    }

    /**
     * @brief Allocate a slot of a size for an lvalue
     *
     * An lvalue that has a slot keeps it
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Offset offset_address,
        Operand_Size operand = Operand_Size::Word)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        stack_address.insert(lvalue, { allocate(offset_address), operand });
    }

    /**
     * @brief Allocate a doubleword slot for an address
     */
    constexpr void allocate_pointer_on_stack() { allocate(8); }

    /**
     * @brief Allocate a slot of an operand size for an lvalue
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Operand_Size operand = Operand_Size::Word)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto offset_address = static_cast<std::size_t>(operand);
        stack_address.insert(lvalue, { allocate(offset_address), operand });
    }

    /**
     * @brief Set the slot of an lvalue to the top of the stack, as a
     * doubleword
     */
    constexpr void add_address_location_to_stack(LValue const& lvalue)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        stack_address.insert(lvalue, { size, Operand_Size::Doubleword });
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
    Size aad_local_size{ 0 };

  private:
    Ordered_Map<Label, Offset> allocation_table{};
    Offset size{ 0 };
    Size vectors{ 0 };
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
Stack::Offset Stack::allocate(Operand_Size operand)
{
    return pimpl->allocate(operand);
}
Stack::Offset Stack::allocate(Size alloc)
{
    return pimpl->allocate(alloc);
}
Operand_Size Stack::get_operand_size_from_offset(Offset offset) const
{
    return pimpl->get_operand_size_from_offset(offset);
}
void Stack::set_aad_local_size(Size local_size)
{
    pimpl->set_aad_local_size(local_size);
}
Size Stack::get_aad_local_size()
{
    return pimpl->get_aad_local_size();
}
Size Stack::get_stack_frame_allocation_size(Label const& label)
{
    return pimpl->get_stack_frame_allocation_size(label);
}
void Stack::set_stack_frame_allocation_size(Label const& label)
{
    return pimpl->set_stack_frame_allocation_size(label);
}
common::Stack_Offset Stack::get_stack_offset_from_table_vector_index(
    LValue const& lvalue,
    std::string const& key,
    object::Vector const& vector)
{
    return pimpl->get_stack_offset_from_table_vector_index(lvalue, key, vector);
}
Size Stack::get_stack_size_from_table_vector(object::Vector const& vector)
{
    return pimpl->get_stack_size_from_table_vector(vector);
}
void Stack::allocate_pointer_on_stack()
{
    pimpl->allocate_pointer_on_stack();
}
void Stack::set_address_from_size(LValue const& lvalue,
    Offset offset_address,
    Operand_Size operand)
{
    pimpl->set_address_from_size(lvalue, offset_address, operand);
}
void Stack::set_address_from_size(LValue const& lvalue, Operand_Size operand)
{
    pimpl->set_address_from_size(lvalue, operand);
}
void Stack::add_address_location_to_stack(LValue const& lvalue)
{
    pimpl->add_address_location_to_stack(lvalue);
}
std::string Stack::get_lvalue_from_offset(Offset offset) const
{
    return pimpl->get_lvalue_from_offset(offset);
}

} // namespace credence::target::arm64::assembly
