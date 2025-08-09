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

#include <roxas/ir/types.h> // for Instructions
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table, Default_Table_Type
#include <roxas/util.h>     // for ROXAS_PRIVATE_UNLESS_TESTED
#include <string>           // for basic_string, string
#include <string_view>      // for string_view
#include <vector>           // for vector

namespace roxas {
namespace ir {

/**
 * @brief
 *
 *  Intermediate_Representation Class
 *
 * An implementation of the Three-address IR as Quintuple,
 * where there can be 4 or less arguments with an operator.
 * Notes:
 *  https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf
 *
 */
class Intermediate_Representation
{
  public:
    Intermediate_Representation(Intermediate_Representation const&) = delete;
    Intermediate_Representation& operator=(Intermediate_Representation const&) =
        delete;

  public:
    using DataType = Default_Table_Type;

  public:
    /**
     * @brief Construct a new Intermediate_Representation object
     *
     */
    explicit Intermediate_Representation(json::JSON const& symbols)
        : internal_symbols_(symbols)
    {
    }
    /**
     * @brief Destroy the Intermediate_Representation object
     *
     */
    ~Intermediate_Representation() = default;

  public:
    using Node = json::JSON;
    /**
     * @brief
     * Throw Parsing or program flow error
     *
     * Use source data provided by internal symbol table
     *
     * @param message
     * @param object
     */
    void parsing_error(std::string_view message, std::string_view object);

  public:
    /**
     * @brief
     *  Parse an ast node in recursive descent
     *
     * @param node
     */
    void parse_node(Node& node);
    /**
     * @brief
     * Get IR as list of ordered instructions
     *
     * @return constexpr Instructions
     */
    constexpr Instructions instructions() { return quintuples_; }

  public:
    /**
     * @brief
     * Parse auto statements and declare in symbol table
     *
     * @param node
     */
    void from_auto_statement(Node& node);

  public:
    /**
     * @brief
     *  Parse assignment expression
     *
     * @param node
     */
    void from_assignment_expression(Node& node);
    /**
     * @brief
     * Parse identifier lvalue
     *
     *  Parse and verify identifer is declared with auto or extern
     *
     * @param node
     */
    void check_identifier_symbol(Node& node);
    void from_indirect_identifier(Node& node);
    /**
     * @brief
     * Parse fixed-size vector (array) lvalue
     *
     * @param node
     */
    void from_vector_idenfitier(Node& node);
    /**
     * @brief
     * Parse number literal node into IR symbols
     *
     * @param node
     */
    DataType from_number_literal(Node& node);
    /**
     * @brief
     * Parse string literal node into IR symbols
     *
     * @param node
     */
    DataType from_string_literal(Node& node);
    /**
     * @brief
     * Parse constant literal node into IR symbols
     *
     * @param node
     */
    DataType from_constant_literal(Node& node);

    /* clang-format off */
  ROXAS_PRIVATE_UNLESS_TESTED:
    /* clang-format on*/
    json::JSON internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
    Instructions quintuples_{};
    std::vector<std::string> labels_{};
};
} // namespace ir
} // namespace roxas
