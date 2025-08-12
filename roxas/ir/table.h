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

#include <roxas/ir/types.h> // for RValue
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table
#include <roxas/util.h>     // for ROXAS_PRIVATE_UNLESS_TESTED
#include <string>           // for basic_string
#include <string_view>      // for string_view

namespace roxas {
namespace ir {

using namespace type;

/**
 * @brief
 *
 * Parse expression nodes into a table of symbols and algebraic types
 *
 */
class Table
{
  public:
    Table(Table const&) = delete;
    Table& operator=(Table const&) = delete;

    using Node = json::JSON;

  public:
    /**
     * @brief Construct a new Table object
     *
     */
    explicit Table(json::JSON const& symbols)
        : internal_symbols_(symbols)
    {
    }
    /**
     * @brief Destroy the Table object
     *
     */
    ~Table() = default;

  public:
    void from_expression(Node& node);

  public:
    void from_auto_statement(Node& node);

  public:
    RValue from_function_expression(Node& node);
    RValue from_relation_expression(Node& node);
    RValue from_pre_inc_dec_expression(Node& node);
    RValue from_post_inc_dec_expression(Node& node);
    RValue from_address_of_expression(Node& node);
    RValue from_evaluated_expression(Node& node);
    RValue from_unary_expression(Node& node);
    RValue from_ternary_expression(Node& node);

  public:
    RValue from_rvalue_expression(Node& node);
    RValue::LValue from_lvalue_expression(Node& node);
    RValue::Value from_indirect_identifier(Node& node);
    RValue::Value from_vector_idenfitier(Node& node);

    inline bool is_symbol(Node& node)
    {
        auto lvalue = node["root"].ToString();
        return symbols_.get_symbol_defined(lvalue) ||
               globals_.get_symbol_defined(lvalue);
    }

  public:
    RValue from_assignment_expression(Node& node);

  public:
    RValue::Value from_constant_expression(Node& node);
    RValue::Value from_number_literal(Node& node);
    RValue::Value from_string_literal(Node& node);
    RValue::Value from_constant_literal(Node& node);

  private:
    void error(std::string_view message, std::string_view object);

    /* clang-format off */
  ROXAS_PRIVATE_UNLESS_TESTED:
    /* clang-format on*/
    json::JSON internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};
} // namespace ir
} // namespace roxas
