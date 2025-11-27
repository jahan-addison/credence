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

#include <credence/error.h> // for credence_assert_message
#include <credence/util.h>  // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <credence/value.h> // for Literal, Array, ...
#include <fmt/format.h>     // for format
#include <map>              // for map
#include <string>           // for basic_string, string
#include <utility>          // for make_pair, move

namespace credence {

/**
 * @brief
 * Symbol table template class
 *
 * Constructs a symbol table from a template data structure
 * An example table may be a map to `std::array<std::string, 5>':
 *
 *  Example:
 *
 *    Name
 *     |
 *   ------------------------------------------------------
 *   | Type | Size | Line Declare | Line Usage |  Address |
 *   ------------------------------------------------------
 *
 *  Default:
 *
 *    Name
 *     |
 *   ------------------------
 *   | Value | Type | Size |
 *   ------------------------
 */
template<
    typename T = internal::value::Literal,
    typename Pointer = internal::value::Array>
class Symbol_Table
{
  public:
    Symbol_Table& operator=(Symbol_Table const&) = delete;

    Symbol_Table() = default;
    ~Symbol_Table() = default;

    auto begin() const { return table_.begin(); }
    auto end() const { return table_.end(); }
    auto end_t() { return addr_.end(); }
    auto begin_t() { return addr_.begin(); }

  public:
    inline void set_symbol_by_name(std::string const& name, T const& entry)
    {
        table_.insert_or_assign(name, entry);
    }

    // cppcheck-suppress passedByValue
    inline void set_symbol_by_name(std::string const& name, Pointer entry)
    {
        addr_.insert_or_assign(name, entry);
    }

    inline void remove_symbol_by_name(std::string const& name)
    {
        table_.erase(name);
    }

    inline void clear()
    {
        addr_.clear();
        table_.clear();
    }

    inline constexpr std::size_t size() { return table_.size(); }

    inline T get_symbol_by_name(std::string const& name) const
    {
        credence_assert_message(
            table_.find(name) != table_.end(),
            fmt::format("symbol not found `{}`", name));
        return table_.at(name);
    }

    inline Pointer get_pointer_by_name(std::string const& name) const
    {
        credence_assert_message(
            addr_.find(name) != addr_.end(),
            fmt::format("address symbol not found `{}`", name));
        return addr_.at(name);
    }

    constexpr bool is_defined(std::string const& name) const
    {
        return table_.contains(name) or addr_.contains(name);
    }

    constexpr bool is_pointer(std::string const& name) const
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
