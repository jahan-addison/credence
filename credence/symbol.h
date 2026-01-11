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

/****************************************************************************
 *
 * Symbol table for variables, functions, and globals
 *
 * Maintains a mapping of identifiers to their types, values, and addresses.
 * Handles local variables (auto), external globals (extrn), and vector
 * (array) declarations. Symbol resolution respects scope rules.
 *
 * Example - symbol table entries:
 *
 *   main() {
 *     auto x, y, *z;    // Local symbols: x, y, z
 *     extrn numbers;    // External symbol reference
 *     x = 10;
 *     y = numbers[0];
 *     z = &x;
 *   }
 *
 *   add(a, b) {         // Function symbol with parameters
 *     return(a + b);
 *   }
 *
 *   numbers [5] 1, 2, 3, 4, 5;  // Global vector symbol
 *
 * Symbol table tracks:
 * - Variable names and their inferred types
 * - Function names and signatures
 * - Array sizes and element types
 * - Memory addresses for code generation
 * - Scope information (local vs global)
 *
 *****************************************************************************/

#pragma once

#include <algorithm>         // for any_of
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

    inline Symbol get_symbol_by_name(std::string const& name,
        std::source_location const& trace =
            std::source_location::current()) const
    {
        credence_assert_message_trace(table_.find(name) != table_.end(),
            fmt::format("symbol not found `{}`", name),
            trace);
        return table_.at(name);
    }

    inline Pointer get_pointer_by_name(std::string const& name,
        std::source_location const& trace =
            std::source_location::current()) const
    {
        credence_assert_message_trace(addr_.find(name) != addr_.end(),
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

    constexpr bool is_pointer_address(Pointer const& addr) const noexcept
    {
        return std::any_of(addr_.begin(), addr_.end(), [&](auto const& entry) {
            return addr == entry.second;
        });
    }

    /* clang-format off */
  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::map<std::string, Symbol> table_{};
    std::map<std::string, Pointer> addr_{};
    /* clang-format on*/
};

} // namespace credence
