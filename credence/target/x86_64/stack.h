/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "instructions.h"      // for get_size_from_operand_size, Operand_Size
#include <algorithm>           // for find_if, __find_if
#include <credence/ir/table.h> // for Vector
#include <credence/map.h>      // for Ordered_Map
#include <credence/types.h>    // for Size, LValue, RValue, Type
#include <credence/util.h>     // for align_up_to_16, align_up_to_8
#include <numeric>             // for accumulate
#include <string>              // for basic_string, string, operator==
#include <utility>             // for pair

#pragma once

namespace credence::target::x86_64::detail {

/**
 * @brief
 * Encapsulation of a push-down stack for the x86-64 architecture
 *
 * Provides a means to allocate, traverse, and verify offsets
 * on the stack by lvalues and vice-versa.
 */
class Stack
{
  public:
    explicit Stack() = default;
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;

  public:
    using Type = type::semantic::Type;
    using Size = type::semantic::Size;
    using LValue = type::semantic::LValue;
    using RValue = type::semantic::RValue;
    using Offset = detail::Stack_Offset;
    using Entry = std::pair<Offset, detail::Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

  public:
    constexpr void clear() { stack_address.clear(); }

  public:
    constexpr bool empty_at(LValue const& lvalue)
    {
        return stack_address[lvalue].second == detail::Operand_Size::Empty;
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
     * @brief Allocate space on the stack from a word size
     *
     * See instructions.h for details
     */
    constexpr Offset allocate(Operand_Size operand)
    {
        auto alloc = get_size_from_operand_size(operand);
        size += alloc;
        return size;
    }

    /**
     * @brief Get the word size of an offset address
     *
     * See instructions.h for details
     */
    constexpr detail::Operand_Size get_operand_size_from_offset(
        Offset offset) const
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
        Immediate const& rvalue)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto operand_size = get_operand_size_from_rvalue_datatype(rvalue);
        auto value_size = get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    /**
     * @brief Set and allocate an address from an accumulator register size
     */
    constexpr void set_address_from_accumulator(LValue const& lvalue,
        Register acc)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto register_size = get_operand_size_from_register(acc);
        auto allocation = detail::get_size_from_operand_size(register_size);

        allocate_aligned_lvalue(lvalue, allocation, register_size);
    }

    /**
     * @brief Set and allocate an address from a type in the Table
     */
    constexpr void set_address_from_type(LValue const& lvalue, Type type)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        auto operand_size = get_operand_size_from_type(type);
        auto value_size = get_size_from_operand_size(operand_size);
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
            size += value_size;
            stack_address.insert(lvalue, { size, operand_size });
        } else {
            size = size + value_size + value_size;
        }
    }

    /**
     * @brief Set and allocate an address from another addres (pointer)
     *
     * Memory align to multiples of 8 bytes per the ABI
     */
    constexpr void set_address_from_address(LValue const& lvalue)
    {
        auto qword_size = Operand_Size::Qword;
        size = util::align_up_to_8(
            size + detail::get_size_from_operand_size(qword_size));
        stack_address.insert(lvalue, { size, qword_size });
    }

    /**
     * @brief Get the allocation size of the current frame, aligned up to 16
     * bytes
     */
    constexpr Size get_stack_frame_allocation_size()
    {
        return size > 16 ? util::align_up_to_16(size) - 8
                         : util::align_up_to_16(size);
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
        ir::detail::Vector const& vector)
    {
        bool search{ false };
        auto vector_offset = get(lvalue).first;
        return std::accumulate(vector.data.begin(),
            vector.data.end(),
            vector_offset,
            [&](type::semantic::Size offset,
                ir::detail::Vector::Entry const& entry) {
                if (entry.first == key)
                    search = true;
                if (!search)
                    offset -= detail::get_size_from_operand_size(
                        detail::get_operand_size_from_rvalue_datatype(
                            entry.second));
                return offset;
            });
    }

    /**
     * @brief Get the size of a vector (array)
     *
     * Memory align to multiples of 16 bytes per the ABI
     */
    constexpr Size get_stack_size_from_table_vector(
        ir::detail::Vector const& vector)
    {
        // clang-format off
    auto vector_size = size + std::accumulate(
            vector.data.begin(),
            vector.data.end(),
            0UL,
    [&](type::semantic::Size offset,
        ir::detail::Vector::Entry const& entry) {
        return offset +
            detail::get_size_from_operand_size(
                detail::get_operand_size_from_rvalue_datatype(
                entry.second));
    });

    return vector_size < 16UL ? vector_size :
        util::align_up_to_16(vector_size);
        // clang-format on
    }

    /**
     * @brief Set and allocate an address from an arbitrary offset
     */
    constexpr void set_address_from_size(LValue const& lvalue,
        Offset allocate,
        Operand_Size operand = Operand_Size::Dword)
    {
        if (stack_address[lvalue].second != Operand_Size::Empty)
            return;
        size += allocate;
        stack_address.insert(lvalue, { size, operand });
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
    Offset size{ 0 };
    Local stack_address{};
};

}