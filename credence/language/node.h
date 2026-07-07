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

#pragma once

#include "datatype.h"        // for Expression, Literal
#include <array>             // for array
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <easyjson.h>        // for JSON
#include <memory>            // for make_shared
#include <source_location>   // for source_location
#include <string>            // for basic_string, string
#include <string_view>       // for string_view
#include <vector>            // for vector

/****************************************************************************
 *
 * Node_Parser - second pass, AST_Node -> Datatype
 *
 * Parser (parser.cc) produces a right-associative AST_Node tree with no
 * real operator precedence. Node_Parser walks that tree's expression
 * nodes into the algebraic Datatype type, checking lvalues against
 * declared storage along the way. Statement and non-expression nodes are
 * out of scope here - Shunting_Yard (shunting_yard.h) is the next pass,
 * fixing precedence over the Datatype tree this class produces.
 *
 *   B source:  x = 5 + 3 * 2
 *
 *   ast node:  {"node": "assignment",
 *               "left": {"name": "x"},
 *               "right": {"node": "binary_op", "op": "+", ...}}
 *
 *   Datatype:  Assignment(lvalue="x",
 *                        rvalue=BinaryOp(ADD,
 *                                       Literal(5),
 *                                       BinaryOp(MUL, ...)))
 *
 *****************************************************************************/

namespace credence::language {

/**
 * @brief
 * Second-pass parser: AST_Node expression nodes to the algebraic
 * Datatype type, checking lvalue declarations along the way.
 *
 * See datatype.h for details.
 */
class Node_Parser
{

  public:
    Node_Parser(Node_Parser const&) = delete;
    Node_Parser& operator=(Node_Parser const&) = delete;

  private:
    using Expression = datatype::Datatype;
    using Literal = datatype::Literal;

  public:
    using Expression_PTR = datatype::Datatype::Pointer;
    using Node = util::AST_Node;
    using Parameters = std::vector<Expression_PTR>;

  public:
    explicit Node_Parser(util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols = {})
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
    {
    }

    explicit Node_Parser(util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols,
        Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }

    ~Node_Parser() = default;

  public:
    static inline Expression parse(util::AST_Node const& node,
        util::AST_Node const& internals,
        Symbol_Table<> const& symbols = {},
        Symbol_Table<> const& globals = {})
    {
        auto expression = Node_Parser{ internals, symbols, globals };
        return expression.parse_from_node(node);
    }

  public:
    Expression parse_from_node(Node const& node);

    inline Expression_PTR make_expression_pointer_from_ast(Node const& node)
    {
        return std::make_shared<datatype::Datatype>(parse_from_node(node));
    }

    inline Expression from_expression_node(Node const& node)
    {
        return parse_from_node(node);
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
    Expression from_evaluated_expression_node(Node const& node);
    Expression from_function_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Expression from_relation_expression_node(Node const& node);

  private:
    Expression from_ternary_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Expression from_unary_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Expression::LValue from_lvalue_expression_node(Node const& node);
    Literal from_indirect_identifier_node(Node const& node);
    Literal from_vector_idenfitier_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Expression from_assignment_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Literal from_constant_expression_node(Node const& node);
    Literal from_integer_literal_node(Node const& node);
    Literal from_float_literal_node(Node const& node);
    Literal from_bool_literal_node(Node const& node);
    Literal from_double_literal_node(Node const& node);
    Literal from_string_literal_node(Node const& node);
    Literal from_constant_literal_node(Node const& node);

  private:
    void expression_parser_error(
        std::string_view message,
        std::string_view symbol,
        std::source_location const& location = std::source_location::current());

  private:
    // clang-format on
    const std::array<std::string, 6> unary_types = { "pre_inc_dec_expression",
        "post_inc_dec_expression",
        "indirect_lvalue",
        "unary_indirection",
        "address_of_expression",
        "unary_expression" };
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

// clang-format on

inline Node_Parser::Expression_PTR parse_node_as_expression(
    util::AST_Node const& node,
    util::AST_Node const& internals,
    Symbol_Table<> const& symbols = {},
    Symbol_Table<> const& globals = {})
{
    return std::make_shared<datatype::Datatype>(
        Node_Parser::parse(node, internals, symbols, globals));
}

} // namespace language
