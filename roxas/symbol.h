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

#include <format>
#include <map>
#include <roxas/types.h>
#include <roxas/util.h>
#include <string>
#include <tuple>

namespace roxas {
/*
 *.
 *   ------------------------
 *   | Value | Type | Size |
 *   ------------------------
 */

template<typename T = type::Value_Type>
class Symbol_Table
{
    /**
     * @brief Symbol table template class
     *
     * Constructs a symbol table from a template data structure
     *
     * An example table may be a map to `std::array<std::string, 5>':
     *
     * Name
     *     \
     *
     *   ------------------------------------------------------
     *   | Type | Size | Line Declare | Line Usage |  Address |
     *   ------------------------------------------------------
     * ...
     * ...
     */
  public:
    Symbol_Table& operator=(Symbol_Table const&) = delete;

    /**
     * @brief Construct a new Symbol_Table object
     *
     */
    Symbol_Table() = default;
    ~Symbol_Table() = default;

  public:
    /**
     * @brief Get a symbol by name in the symbol table
     *
     * @param name
     * @return symbol_data
     */
    inline T get_symbol_by_name(std::string const& name)
    {
        return table_.at(name);
    }
    /**
     * @brief Check if a symbol exists
     *
     * @param name
     */
    inline bool is_defined(std::string const& name) const
    {
        return table_.contains(name);
    }
    /**
     * @brief Get a symbol by name in the symbo table
     *
     * @param name
     * @return symbol_data
     */
    inline void set_symbol_by_name(std::string const& name, T entry)
    {
        table_.insert_or_assign(name, std::move(entry));
    }

    /* clang-format off */
  ROXAS_PRIVATE_UNLESS_TESTED:
    std::map<std::string, T> table_{};
    /* clang-format on*/
};

} // namespace roxas
