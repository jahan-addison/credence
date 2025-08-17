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

#include <array>          // for array
#include <roxas/json.h>   // for JSON
#include <roxas/symbol.h> // for Symbol_Table
#include <roxas/types.h>  // for RValue
#include <roxas/util.h>   // for ROXAS_PRIVATE_UNLESS_TESTED
#include <string>         // for basic_string, string
#include <string_view>    // for string_view

namespace roxas {
namespace ir {
using namespace roxas::type;
/* clang-format off */

/**
 * @brief
 *
 * Constructs a table of rvalues and temporaries as algebraic data types
 *
 */
class Table
{
  public:
    Table(Table const&) = delete;
    Table& operator=(Table const&) = delete;

    using Node = json::JSON;

  public:
    explicit Table(json::JSON const& internal_symbols,
                   Symbol_Table<> const& symbols = {})
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
    {
    }
    explicit Table(json::JSON const& internal_symbols,
                   Symbol_Table<> const& symbols,
                   Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }
    ~Table() = default;
  public:
    inline RValue from_rvalue_expression(Node& node) {
      return from_rvalue(node);
    }
    RValue from_rvalue(Node& node);

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue from_evaluated_expression(Node& node);
    RValue from_function_expression(Node& node);

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue from_relation_expression(Node& node);

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue from_unary_expression(Node& node);

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue::LValue from_lvalue_expression(Node& node);
    RValue::Value from_indirect_identifier(Node& node);
    RValue::Value from_vector_idenfitier(Node& node);

    inline bool is_symbol(Node& node)
    {
        auto lvalue = node["root"].ToString();
        return symbols_.is_defined(lvalue) or
               globals_.is_defined(lvalue);
    }

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue from_assignment_expression(Node& node);

  ROXAS_PRIVATE_UNLESS_TESTED:
    RValue::Value from_constant_expression(Node& node);
    RValue::Value from_number_literal(Node& node);
    RValue::Value from_string_literal(Node& node);
    RValue::Value from_constant_literal(Node& node);

  private:
    void error(std::string_view message, std::string_view symbol_name);
    std::array<std::string, 8> const unary_types_ = { "pre_inc_dec_expression",
                                                      "post_inc_dec_expression",
                                                      "address_of_expression",
                                                      "unary_expression" };
  ROXAS_PRIVATE_UNLESS_TESTED:
    json::JSON internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};
} // namespace ir
} // namespace roxas
