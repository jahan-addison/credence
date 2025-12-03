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

#include <credence/error.h>  // for credence_assert_message
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <credence/values.h> // for Literal, Array, ...
#include <fmt/format.h>      // for format
#include <map>               // for map
#include <set>               // for set
#include <source_location>   // for source_location
#include <string>            // for basic_string, string
#include <utility>           // for make_pair, move

namespace credence {

/**
 * @brief
 * Symbol table template class
 *
 * Constructs a symbol table from a template data structure
 * An example table may be a map to `std::array<std::string, 5>':
 *
 */
template<typename Symbol = value::Literal, typename Pointer = value::Array>
class Symbol_Table
{
  public:
    Symbol_Table& operator=(Symbol_Table const&) = delete;
    Symbol_Table() = default;
    ~Symbol_Table() = default;

  private:
    using Keys = std::set<std::string>;

  public:
    constexpr auto begin() const { return table_.begin(); }
    constexpr auto end() const { return table_.end(); }
    constexpr auto end_t() { return addr_.end(); }
    constexpr auto begin_t() { return addr_.begin(); }

  public:
    inline void set_symbol_by_name(std::string const& name, Symbol const& entry)
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

    inline void clear() noexcept
    {
        addr_.clear();
        table_.clear();
    }

    inline bool empty() const noexcept
    {
        return table_.empty() and addr_.empty();
    }

    inline constexpr std::size_t size() const noexcept
    {
        return table_.size() + addr_.size();
    }

    inline Keys get_symbols()
    {
        Keys keys{};
        for (auto const& key : table_)
            keys.insert(key.first);
        return keys;
    }

    inline Keys get_pointers()
    {
        Keys keys{};
        for (auto const& key : addr_)
            keys.insert(key.first);
        return keys;
    }

    inline Symbol get_symbol_by_name(
        std::string const& name,
        std::source_location const& trace =
            std::source_location::current()) const
    {
        credence_assert_message_trace(
            table_.find(name) != table_.end(),
            fmt::format("symbol not found `{}`", name),
            trace);
        return table_.at(name);
    }

    inline Pointer get_pointer_by_name(
        std::string const& name,
        std::source_location const& trace =
            std::source_location::current()) const
    {
        credence_assert_message_trace(
            addr_.find(name) != addr_.end(),
            fmt::format("address symbol not found `{}`", name),
            trace);
        return addr_.at(name);
    }

    constexpr bool is_defined(std::string const& name) const noexcept
    {
        return table_.contains(name) or addr_.contains(name);
    }

    constexpr bool is_pointer(std::string const& name) const noexcept
    {
        return addr_.contains(name);
    }

    /* clang-format off */
  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::map<std::string, Symbol> table_{};
    std::map<std::string, Pointer> addr_{};
    /* clang-format on*/
};

} // namespace credence
