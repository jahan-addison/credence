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
#include <algorithm>                       // for find_if, __find_if
#include <credence/ir/object.h>            // for LValue, Size, Vector, Type
#include <credence/map.h>                  // for Ordered_Map
#include <credence/target/common/memory.h> // for align_up_to
#include <credence/target/common/types.h>  // for base_stack_pointer
#include <credence/types.h>                // for Size
#include <credence/util.h>                 // for align_up_to_16
#include <cstddef>                         // for size_t
#include <fmt/base.h>                      // for println
#include <fmt/compile.h>                   // for format, operator""_cf
#include <numeric>                         // for accumulate
#include <string>                          // for basic_string, string, ope...
#include <utility>                         // for pair

/****************************************************************************
 *
 * ARM64 Stack
 *
 * A push-down stack that grows downward and maintains 16-byte alignment.
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
 * Register allocation (if no call jumps):
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

/****************************************************************************
 * Register selection table:
 *
 *   x6  = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x15      = Second data section register
 *   x7       = multiplication scratch register
 *   x8       = The default "accumulator" register for expression expansion
 *   x10      = The stack move register; additional scratch register
 *   x9 - x18 = If there are no function calls in a stack frame, local scope
 *             variables are stored in x9-x18, after which the stack is used
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

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
     * @brief Get the stack location offset and size from an lvalue
     */
    Entry get(LValue const& lvalue)
    {

        auto address = stack_address[lvalue];
        address.first += aad_local_size;
        return address;
    }

    /**
     * @brief Get the stack location lvalue and size from an offset
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
     * @brief Dynamically set an operand size for which pushes downward on a
     * chunk
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
     * See assembly.h for details
     */
    constexpr Offset allocate(Operand_Size operand)
    {
        auto alloc = get_size_from_operand_size(operand);
        size += alloc;
        set(size, operand);
        return size;
    }

    constexpr Offset allocate(Size alloc)
    {
        size += alloc;
        set(size, assembly::get_operand_size_from_size(alloc));
        return size;
    }

    /**
     * @brief Deallocate space on the stack
     */
    constexpr void deallocate(Size alloc)
    {
        if ((size - alloc) <= 0) {
            size = 0;
        } else {
            size = 0;
        }
    }

    /**
     * @brief Get the word size of an offset address
     *
     * See assembly.h for details
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
     * @brief Set and allocate an address from an immediate
     */
    constexpr void set_address_from_immediate(LValue const& lvalue,
        assembly::Immediate const& rvalue)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto operand_size =
            assembly::get_operand_size_from_rvalue_datatype(rvalue);
        auto value_size = assembly::get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    /**
     * @brief Set and allocate an address from an accumulator register size
     */
    inline void set_address_from_accumulator(LValue const& lvalue,
        assembly::Register acc)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto register_size = assembly::get_operand_size_from_register(acc);
        auto allocation = assembly::get_size_from_operand_size(register_size);

        allocate_aligned_lvalue(lvalue, allocation, register_size);
    }

    /**
     * @brief Set and allocate an address from a type in the Table
     */
    constexpr void set_address_from_type(LValue const& lvalue, Type type)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto operand_size = assembly::get_operand_size_from_type(type);
        auto value_size = assembly::get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    /**
     * @brief
     * In some cases address space was loaded in chunks for memory alignment
     *
     * So skip any previously allocated offsets as we push downwards
     */
    constexpr void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        Operand_Size operand_size)
    {
        if (get_lvalue_from_offset(size + value_size).empty()) {
            size -= value_size;
            stack_address.insert(lvalue, { size, operand_size });
        } else {
            size = size - value_size + value_size;
        }
    }

    /**
     * @brief Set and allocate an address from another address (pointer)
     */
    constexpr void set_address_from_address(LValue const& lvalue)
    {
        auto qword_size = Operand_Size::Doubleword;
        size = common::memory::align_up_to(
            size - assembly::get_size_from_operand_size(qword_size), 8);
        stack_address.insert(lvalue, { size, qword_size });
    }

    /**
     * @brief Get the allocation size of the current frame, aligned up to 16
     * bytes
     */
    Size get_stack_frame_allocation_size(Label const& label)
    {
        credence_assert(allocation_table.contains(label));
        auto allocation_size = allocation_table.at(label);
        if (allocation_size < 16)
            return 16UL;
        else
            return common::memory::align_up_to(allocation_size, 16);
    }

    void set_stack_frame_allocation_size(Label const& label)
    {
        allocation_table.insert(label, size);
    }

    /**
     * @brief Get the stack address of an index in a vector (array)
     *
     * The vector was allocated in a chunk and we allocate each index
     * downward
     */
    common::Stack_Offset get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        ir::object::Vector const& vector)
    {
        bool search{ false };
        auto vector_offset = get(lvalue).first;
        return std::accumulate(vector.get_data().begin(),
            vector.get_data().end(),
            vector_offset,
            [&](type::semantic::Size offset,
                ir::object::Vector::Entry const& entry) {
                if (entry.first == key)
                    search = true;
                if (!search)
                    offset -= 8UL;
                return offset;
            });
    }

    /**
     * @brief Get the size of a vector
     */
    Size get_stack_size_from_table_vector(ir::object::Vector const& vector)
    {
        return 16UL + std::accumulate(vector.get_data().begin(),
                          vector.get_data().end(),
                          0UL,
                          [&](type::semantic::Size offset,
                              [[maybe_unused]] ir::object::Vector::Entry const&
                                  entry) { return offset + 8UL; });
    }

    constexpr Size get_offset_from_pushdown_stack()
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            size,
            [&](type::semantic::Size offset, Pair const& pair) {
                return offset -=
                       assembly::get_size_from_operand_size(pair.second.second);
            });
    }

    /**
     * @brief Set and allocate an address from an arbitrary offset
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Offset offset_address,
        Operand_Size operand = Operand_Size::Word)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        allocate(offset_address);
        stack_address.insert(lvalue, { size, operand });
    }

    /**
     * @brief Allocate 8 bytes to store the pointer of an address on the stack
     */
    constexpr void allocate_pointer_on_stack() { allocate(8); }

    constexpr void set_address_from_size(LValue const& lvalue,
        Operand_Size operand = Operand_Size::Word)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto offset_address = static_cast<std::size_t>(operand);

        allocate(offset_address);
        stack_address.insert(lvalue, { size, operand });
    }

    constexpr void add_address_location_to_stack(LValue const& lvalue)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        stack_address.insert(lvalue, { size, Operand_Size::Doubleword });
    }

    /**
     * @brief Get the lvalue of a local variable allocated at an offset
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
void Stack::deallocate(Size alloc)
{
    pimpl->deallocate(alloc);
}
Operand_Size Stack::get_operand_size_from_offset(Offset offset) const
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
    Operand_Size operand_size)
{
    pimpl->allocate_aligned_lvalue(lvalue, value_size, operand_size);
}
void Stack::set_aad_local_size(Size local_size)
{
    pimpl->set_aad_local_size(local_size);
}
Size Stack::get_aad_local_size()
{
    return pimpl->get_aad_local_size();
}
void Stack::set_address_from_address(LValue const& lvalue)
{
    pimpl->set_address_from_address(lvalue);
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
    ir::object::Vector const& vector)
{
    return pimpl->get_stack_offset_from_table_vector_index(lvalue, key, vector);
}
Size Stack::get_stack_size_from_table_vector(ir::object::Vector const& vector)
{
    return pimpl->get_stack_size_from_table_vector(vector);
}
Size Stack::get_offset_from_pushdown_stack()
{
    return pimpl->get_offset_from_pushdown_stack();
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
