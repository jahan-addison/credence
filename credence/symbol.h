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

#pragma once

#include <credence/types.h>
#include <credence/util.h>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace credence {
/**
 * @brief Symbol table template class
 *
 * Constructs a symbol table from a template data structure
 *
 * An example table may be a map to `std::array<std::string, 5>':
 *
 * Name
 *     \
 *     |
 *   ------------------------------------------------------
 *   | Type | Size | Line Declare | Line Usage |  Address |
 *   ------------------------------------------------------
 * ...
 *  Default:
 *
 *   ------------------------
 *   | Value | Type | Size |
 *   ------------------------
 */
template<typename T = type::Value_Type, typename Pointer = type::Value_Pointer>
class Symbol_Table
{
  public:
    Symbol_Table& operator=(Symbol_Table const&) = delete;

    Symbol_Table() = default;
    ~Symbol_Table() = default;

  public:
    inline void set_symbol_by_name(std::string const& name, T entry)
    {
        table_.insert_or_assign(name, std::move(entry));
    }

    inline void set_symbol_by_name(std::string const& name, Pointer entry)
    {
        addr_.insert_or_assign(name, std::move(entry));
    }

    inline void set_symbol_by_name(std::string const& name,
                                   Symbol_Table<> symbol)
    {
        table_.insert_or_assign(name, symbol.get_symbol_by_name(name));
    }

    inline T get_symbol_by_name(std::string const& name)
    {
        return table_.at(name);
    }

    inline Pointer get_pointer_by_name(std::string const& name)
    {
        return addr_.at(name);
    }

    inline bool is_defined(std::string const& name) const
    {
        return table_.contains(name) or addr_.contains(name);
    }

    inline bool is_pointer(std::string const& name) const
    {
        return addr_.contains(name);
    }

    /* clang-format off */
  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::map<std::string, T> table_{};
    std::map<std::string, Pointer> addr_{};
    /* clang-format on*/
};

} // namespace credence
