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

#include <array>
#include <map>
#include <roxas/util.h>
#include <string>

namespace roxas {

class Symbol_Table
{
    /**
     * @brief Symbol table template class
     *
     * Constructs a symbol table from a template data structure
     *
     * Name
     *     \
     *     |
     *   ------------------------------------------------------
     *   | Type | Size | Line Declare | Line Usage |  Address |
     *   ------------------------------------------------------
     * ...
     * ...
     * Name
     *     \
     *     |
     *   ------------------------------------------------------
     *   | Type | Size | Line Declare | Line Usage |  Address |
     *   ------------------------------------------------------
     * ...
     * ...
     */
  public:
    Symbol_Table& operator=(Symbol_Table const&) = delete;
    Symbol_Table(Symbol_Table const&) = delete;

    /**
     * @brief Construct a new Symbol_Table object
     *
     * @param ast simdjson::ondemand::value
     */
    explicit Symbol_Table(std::string_view ast);

  public:
    /**
     * @brief Get a symbol by name in the symbol table
     *
     * @param name
     * @return symbol_data
     */
    T get_symbol_by_name(std::string_view name);

  private:
    std::map < std::string, std::array<std::string, 5> table_;
};

} // namespace roxas
