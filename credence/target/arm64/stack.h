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

#include "assembly.h"                      // for get_size_from_rvalue_data...
#include <algorithm>                       // for find_if, __find_if
#include <credence/ir/object.h>            // for Vector
#include <credence/map.h>                  // for Ordered_Map
#include <credence/target/common/memory.h> // for align_up_to
#include <credence/target/common/types.h>  // for LValue, Label, RValue, Size
#include <credence/types.h>                // for Size
#include <credence/util.h>                 // for align_up_to_16
#include <fmt/compile.h>                   // for format, operator""_cf
#include <numeric>                         // for accumulate
#include <string>                          // for basic_string, string, ope...
#include <utility>                         // for pair

#pragma once

namespace credence::target::arm64::assembly {

/**
 * @brief
 * A push-down stack for the ARM64 architecture
 *
 * Provides a means to allocate, traverse, and verify offsets
 * that auto-align on the stack by lvalues and vice-versa.
 */
class Stack : public common::detail::base_stack_pointer
{
  public:
    explicit Stack() = default;
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;

  public:
    using Type = common::Type;
    using Size = common::Size;
    using Label = common::Label;
    using LValue = common::LValue;
    using RValue = common::RValue;
    using Offset = common::Stack_Offset;
    using Entry = std::pair<Offset, Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

  public:
    constexpr void clear() { stack_address.clear(); }

  public:
    constexpr bool empty_at(LValue const& lvalue)
    {
        return stack_address[lvalue].second == 0;
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
            return { 0, 0 };
        else
            return find->second;
    }

    /**
     * @brief Dynamically set a size for vector indices, which pushes
     * downward on a chunk
     */
    constexpr void set(Offset offset, Size size)
    {
        using namespace fmt::literals;
        stack_address.insert(
            fmt::format("__internal_vector_offset_{}"_cf, ++vectors),
            Entry{ offset, size });
    }

    /**
     * @brief Allocate space on the stack from a word size
     */
    constexpr Offset allocate(Size alloc)
    {
        size += alloc;
        return size;
    }

    /**
     * @brief Get the size of an offset address
     */
    constexpr Size get_size_from_offset(Offset offset) const
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            0UL,
            [&](Size current_size, Pair const& entry) {
                if (entry.second.first == offset)
                    return entry.second.second;
                return current_size;
            });
    }

    /**
     * @brief Get the operand size of an offset address
     */
    constexpr assembly::Operand_Size get_operand_size_from_offset(
        Offset offset) const
    {
        return std::accumulate(stack_address.begin(),
            stack_address.end(),
            assembly::Operand_Size::Empty,
            [&](assembly::Operand_Size size, Pair const& entry) {
                if (entry.second.first == offset)
                    return assembly::get_operand_size_from_size(
                        entry.second.second);
                return size;
            });
    }

    /**
     * @brief Set and allocate an address from an immediate
     */
    constexpr void set_address_from_immediate(LValue const& lvalue,
        assembly::Immediate const& rvalue)
    {
        if (is_allocated(lvalue))
            return;
        auto value_size = assembly::get_size_from_rvalue_datatype(rvalue);
        allocate_aligned_lvalue(lvalue, value_size);
    }

    /**
     * @brief Set and allocate an address from an accumulator register size
     */
    constexpr void set_address_from_accumulator(LValue const& lvalue,
        assembly::Register acc)
    {
        if (is_allocated(lvalue))
            return;
        auto allocation = assembly::get_size_from_register(acc);
        allocate_aligned_lvalue(lvalue, allocation);
    }

    /**
     * @brief Set and allocate an address from a type in the Table
     */
    constexpr void set_address_from_type(LValue const& lvalue, Type type)
    {
        if (is_allocated(lvalue))
            return;
        auto value_size = assembly::get_size_from_type(type);
        allocate_aligned_lvalue(lvalue, value_size);
    }

    /**
     * @brief
     * In some cases address space was loaded in chunks for memory alignment
     *
     * So skip any previously allocated offsets as we push downwards
     */
    constexpr void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size)
    {
        if (get_lvalue_from_offset(size + value_size).empty()) {
            size += value_size;
            stack_address.insert(lvalue, { size, value_size });
        } else {
            size = size + value_size + value_size;
        }
    }

    /**
     * @brief Set and allocate an address for a pointer (always 8 bytes on
     * arm64)
     */
    constexpr void set_address_from_address(LValue const& lvalue)
    {
        const Size ptr_size = 8;
        size = common::memory::align_up_to(size + ptr_size, ptr_size);
        stack_address.insert(lvalue, { size, ptr_size });
    }

    /**
     * @brief Get the allocation size of the current frame, aligned up to 16
     * bytes
     */
    constexpr Size get_stack_frame_allocation_size()
    {
        return common::memory::align_up_to(size, 16);
    }

    /**
     * @brief Get the stack address of an index in a vector (array)
     *
     * The vector was allocated in a chunk and we allocate each index
     * downward
     */
    constexpr Size get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        ir::object::Vector const& vector)
    {
        bool search{ false };
        auto vector_offset = get(lvalue).first;
        return std::accumulate(vector.data.begin(),
            vector.data.end(),
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
    constexpr Size get_stack_size_from_table_vector(
        ir::object::Vector const& vector)
    {
        // clang-format off
    auto vector_size = size + std::accumulate(
            vector.data.begin(),
            vector.data.end(),
            0UL,
    [&](type::semantic::Size offset,
        ir::object::Vector::Entry const& entry) {
        return offset +
            assembly::get_size_from_rvalue_datatype(
                entry.second);
    });

    return util::align_up_to_16(vector_size);
        // clang-format on
    }

    /**
     * @brief Set and allocate an address from an arbitrary offset
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Offset allocate,
        Size value_size = 4)
    {
        if (is_allocated(lvalue))
            return;
        size += allocate;
        stack_address.insert(lvalue, { size, value_size });
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
    Local stack_address{};
};

} // namespace credence::target::arm64::assembly