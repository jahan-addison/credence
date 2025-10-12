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

#include <array>             // for array
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <memory>            // for allocator, make_shared
#include <simplejson.h>      // for JSON
#include <string>            // for basic_string, string
#include <string_view>       // for string_view
#include <vector>            // for vector

namespace credence {

/**
 * @brief LL(1) top-down Parser of expression ast nodes to
 * type::RValue data structures.
 *
 * See types.h for details.
 */
class RValue_Parser
{

  public:
    RValue_Parser(RValue_Parser const&) = delete;
    RValue_Parser& operator=(RValue_Parser const&) = delete;

  public:
    explicit RValue_Parser(
        util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols = {})
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
    {
    }

    explicit RValue_Parser(
        util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols,
        Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }

    ~RValue_Parser() = default;

  public:
    static inline type::RValue make_rvalue(
        util::AST_Node const& node,
        util::AST_Node const& internals,
        Symbol_Table<> const& symbols = {},
        Symbol_Table<> const& globals = {})
    {
        auto rvalue = RValue_Parser{ internals, symbols, globals };
        return rvalue.from_rvalue(node);
    }

  public:
    using Node = util::AST_Node;
    using Parameters = std::vector<type::RValue::RValue_Pointer>;
    type::RValue from_rvalue(Node const& node);

    inline type::RValue::RValue_Pointer shared_ptr_from_rvalue(Node const& node)
    {
        return std::make_shared<type::RValue>(from_rvalue(node));
    }

    inline type::RValue from_rvalue_expression(Node const& node)
    {
        return from_rvalue(node);
    }

  public:
    inline bool is_symbol(Node const& node)
    {
        auto lvalue = node["root"].to_string();
        return symbols_.is_defined(lvalue) or globals_.is_defined(lvalue);
    }

    inline bool is_defined(std::string const& label)
    {
        return internal_symbols_.has_key(label);
    }

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_evaluated_expression(Node const& node);
    type::RValue from_function_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_relation_expression(Node const& node);

  private:
    type::RValue from_ternary_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_unary_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue::LValue from_lvalue_expression(Node const& node);
    type::RValue::Value from_indirect_identifier(Node const& node);
    type::RValue::Value from_vector_idenfitier(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_assignment_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue::Value from_constant_expression(Node const& node);
    type::RValue::Value from_number_literal(Node const& node);
    type::RValue::Value from_string_literal(Node const& node);
    type::RValue::Value from_constant_literal(Node const& node);

  private:
    // clang-format on
    void error(std::string_view message, std::string_view symbol_name);
    const std::array<std::string, 5> unary_types_ = { "pre_inc_dec_expression",
                                                      "post_inc_dec_expression",
                                                      "indirect_lvalue",
                                                      "address_of_expression",
                                                      "unary_expression" };
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

// clang-format on

inline type::RValue::RValue_Pointer make_rvalue(
    util::AST_Node const& node,
    util::AST_Node const& internals,
    Symbol_Table<> const& symbols = {},
    Symbol_Table<> const& globals = {})
{
    return std::make_shared<type::RValue>(
        RValue_Parser::make_rvalue(node, internals, symbols, globals));
}

} // namespace credence
