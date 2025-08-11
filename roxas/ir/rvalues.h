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

#include <roxas/ir/types.h> // for Instructions, Value_Type
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table
#include <roxas/util.h>     // for ROXAS_PRIVATE_UNLESS_TESTED
#include <string>           // for basic_string, string
#include <string_view>      // for string_view
#include <vector>           // for vector

namespace roxas {
namespace ir {

using namespace type;

/**
 * @brief
 *
 * Parse expression AST nodes into a table of symbols and algebraic types
 *
 */
class RValue_Table
{
  public:
    RValue_Table(RValue_Table const&) = delete;
    RValue_Table& operator=(RValue_Table const&) = delete;

  public:
    using DataType = Value_Type;

  public:
    /**
     * @brief Construct a new RValue_Table object
     *
     */
    explicit RValue_Table(json::JSON const& symbols)
        : internal_symbols_(symbols)
    {
    }
    /**
     * @brief Destroy the RValue_Table object
     *
     */
    ~RValue_Table() = default;

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

  public:
    /**
     * @brief
     * Parse auto statements and declare in symbol table
     *
     * @param node
     */
    void from_auto_statement(Node& node);

  public:
    /***************/
    /* Expressions */
    /***************/

    /**
     * @brief
     * Parse rvalue expression data types
     *
     * @param node
     * @return DataType
     */
    RValue from_rvalue_expression(Node& node);
    /**
     * @brief
     * Parse lvalue expression data types
     *
     * @param node
     * @return DataType
     */
    LValue from_lvalue_expression(Node& node);
    /**
     * @brief
     * Parse assignment expression into pairs of left-hand-side and
     * right-hand-side
     *
     * @param node
     * @return std::pair<DataType, DataType>
     */
    RValue from_assignment_expression(Node& node);
    /**
     * @brief
     * Parse constant expression data types
     *
     * @param node
     * @return DataType
     */
    DataType from_constant_expression(Node& node);
    DataType from_unary_expression(Node& node);

  public:
    /*************/
    /* Lvalues   */
    /*************/

    /*
     * @brief
     *
     *  Check verify identifier is declared in symbol table
     *
     * @param node
     */
    inline bool is_symbol(Node& node)
    {
        auto lvalue = node["root"].ToString();
        return symbols_.get_symbol_defined(lvalue) ||
               globals_.get_symbol_defined(lvalue);
    }
    /**
     * @brief
     * Parse lvalue to pointer data type
     *
     * @param node
     * @return DataType
     */
    DataType from_indirect_identifier(Node& node);
    /**
     * @brief
     * Parse fixed-size vector (array) lvalue
     *
     * @param node
     */
    DataType from_vector_idenfitier(Node& node);

  public:
    /**************/
    /* Constants  */
    /**************/

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
};
} // namespace ir
} // namespace roxas
