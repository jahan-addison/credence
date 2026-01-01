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

class Stack : public common::detail::base_stack_pointer
{
  public:
    explicit Stack() = default;
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;

  public:
    using Entry = std::pair<Offset, assembly::Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

  public:
    constexpr void clear() { stack_address.clear(); }

  public:
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

    constexpr Entry get(LValue const& lvalue) { return stack_address[lvalue]; }

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

    constexpr void set(Offset offset, Operand_Size size)
    {
        using namespace fmt::literals;
        stack_address.insert(
            fmt::format("__internal_vector_offset_{}"_cf, ++vectors),
            Entry{ offset, size });
    }

    constexpr Offset allocate(assembly::Operand_Size operand)
    {
        auto alloc = get_size_from_operand_size(operand);
        size += alloc;
        return size;
    }

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

    constexpr void set_address_from_accumulator(LValue const& lvalue,
        assembly::Register acc)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        auto register_size = assembly::get_operand_size_from_register(acc);
        auto allocation = assembly::get_size_from_operand_size(register_size);

        allocate_aligned_lvalue(lvalue, allocation, register_size);
    }

    constexpr void set_address_from_type(LValue const& lvalue, Type type)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        auto operand_size = assembly::get_operand_size_from_type(type);
        auto value_size = assembly::get_size_from_operand_size(operand_size);
        allocate_aligned_lvalue(lvalue, value_size, operand_size);
    }

    constexpr void allocate_aligned_lvalue(LValue const& lvalue,
        Size value_size,
        assembly::Operand_Size operand_size)
    {
        if (get_lvalue_from_offset(size + value_size).empty()) {
            size += value_size;
            stack_address.insert(lvalue, { size, operand_size });
        } else {
            size = size + value_size + value_size;
        }
    }

    constexpr void set_address_from_address(LValue const& lvalue)
    {
        auto qword_size = assembly::Operand_Size::Doubleword;
        size = common::memory::align_up_to(
            size + assembly::get_size_from_operand_size(qword_size), 8);
        stack_address.insert(lvalue, { size, qword_size });
    }

    constexpr Size get_stack_frame_allocation_size()
    {
        return common::memory::align_up_to(size + 16, 16);
    }

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

    constexpr Size get_stack_size_from_table_vector(
        ir::object::Vector const& vector)
    {
        auto vector_size =
            size + std::accumulate(vector.data.begin(),
                       vector.data.end(),
                       0UL,
                       [&](type::semantic::Size offset,
                           ir::object::Vector::Entry const& entry) {
                           return offset +
                                  assembly::get_size_from_rvalue_datatype(
                                      entry.second);
                       });

        return util::align_up_to_16(vector_size);
    }

    constexpr void set_address_from_size(LValue const& lvalue,
        Offset allocate,
        assembly::Operand_Size operand = assembly::Operand_Size::Word)
    {
        if (stack_address[lvalue].second != assembly::Operand_Size::Empty)
            return;
        size += allocate;
        stack_address.insert(lvalue, { size, operand });
    }

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