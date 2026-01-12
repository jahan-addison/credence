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

namespace credence::target::arm64::assembly {

class Stack::Stack_IMPL
{
  public:
    using Entry = std::pair<Offset, Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

    constexpr void clear() { stack_address.clear(); }

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

    /**
     * @brief Get the stack location offset and size from an lvalue
     */
    constexpr Entry get(LValue const& lvalue) { return stack_address[lvalue]; }

    /**
     * @brief Get the stack location lvalue and size from an offset
     */
    constexpr Entry get(Offset offset) const
    {
        auto find = std::find_if(stack_address.begin(),
            stack_address.end(),
            [&](Pair const& entry) { return entry.second.first == offset; });
        if (find == stack_address.end())
            return { 0, Operand_Size::Empty };
        else
            return find->second;
    }

    /**
     * @brief Dynamically set an operand size for which pushes downward on a
     * chunk
     */
    constexpr void set(Offset offset, Operand_Size size)
    {
        using namespace fmt::literals;
        stack_address.insert(fmt::format("__internal_offset_{}"_cf, ++vectors),
            Entry{ offset, size });
    }

    /**
     * @brief Allocate space on the stack
     *
     * See assembly.h for details
     */
    constexpr void allocate(Operand_Size operand)
    {
        auto alloc = get_size_from_operand_size(operand);
        frame_size += alloc;
        size += alloc;
        set(size, operand);
    }

    constexpr void allocate(Size alloc)
    {
        frame_size += alloc;
        size += alloc;
        set(size, assembly::get_operand_size_from_size(alloc));
    }

    /**
     * @brief Deallocate space on the stack
     */
    constexpr void deallocate(Size alloc)
    {
        if ((frame_size - alloc) <= 0) {
            frame_size = 0;
            size = 0;
        } else {
            frame_size -= alloc;
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
     *
     * Memory align to multiples of 8 bytes per the ABI
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
    Size get_stack_frame_allocation_size(ir::object::Function_PTR& frame)
    {
        auto allocation_size = frame_size;
        // check callee saved special registers
        if (frame->get_tokens().contains("x23"))
            allocation_size += 8;
        if (frame->get_tokens().contains("x26"))
            allocation_size += 8;
        if (allocation_size < 16)
            return 16UL;
        else
            return common::memory::align_up_to(allocation_size, 16);
    }

    /**
     * @brief Get the stack address of an index in a vector (array)
     *
     * The vector was allocated in a chunk and we allocate each index
     * downward
     */
    Size get_stack_offset_from_table_vector_index(LValue const& lvalue,
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
                    offset -=
                        assembly::get_size_from_rvalue_datatype(entry.second);
                return offset;
            });
    }

    /**
     * @brief Get the size of a vector (array)
     *
     * Memory align to multiples of 16 bytes per the ABI
     */
    Size get_stack_size_from_table_vector(ir::object::Vector const& vector)
    {
        auto vector_size =
            size + std::accumulate(vector.get_data().begin(),
                       vector.get_data().end(),
                       0UL,
                       [&](type::semantic::Size offset,
                           ir::object::Vector::Entry const& entry) {
                           return offset +
                                  assembly::get_size_from_rvalue_datatype(
                                      entry.second);
                       });

        return util::align_up_to_16(vector_size);
    }

    constexpr Size get_offset_from_pushdown_stack()
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            frame_size,
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
        size -= offset_address;
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
        size -= static_cast<std::size_t>(offset_address);
    }

    constexpr void add_address_location_to_stack(LValue const& lvalue)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        stack_address.insert(lvalue, { frame_size, Operand_Size::Doubleword });
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
    int vectors{ 0 };
    Offset size{ 0 };
    Offset frame_size{ 0 };
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
void Stack::allocate(Operand_Size operand)
{
    pimpl->allocate(operand);
}
void Stack::allocate(Size alloc)
{
    pimpl->allocate(alloc);
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
void Stack::set_address_from_address(LValue const& lvalue)
{
    pimpl->set_address_from_address(lvalue);
}
Size Stack::get_stack_frame_allocation_size(ir::object::Function_PTR& frame)
{
    return pimpl->get_stack_frame_allocation_size(frame);
}
Size Stack::get_stack_offset_from_table_vector_index(LValue const& lvalue,
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
