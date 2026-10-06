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

#include "hir.h"             // for Data_Kind
#include "literal.h"         // for Literal
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
 * AST Lowering
 *
 * The second pass, Parser::AST_Node -> HIR
 *
 * The Parser produces a right-associative AST_Node tree with no real
 * operator precedence. AST_Lowering lowers that tree's expression nodes into
 * the HIR - a tree of the algebraic Data_Kind type from hir.h - checking
 * lvalues against declared storage along the way. Statement and
 * non-expression nodes are out of scope here - Shunting_Yard (precedence.h)
 * is the next pass, fixing precedence over the HIR this class produces.
 *
 *   B source:  x = 5 + 3 * 2
 *
 *   ast node:  {"node": "assignment_expression",
 *               "root": ["="],
 *               "left": {"node": "lvalue", "root": "x"},
 *               "right": {"node": "relation_expression",
 *                         "root": ["+"], ...}}
 *
 *   HIR:  Symbol(LValue("x"),
 *                Relation(B_ADD,
 *                         Literal(5:int:4),
 *                         Relation(B_MUL, ...)))
 *
 *****************************************************************************/

namespace credence::language {

/**
 * @brief
 * Second pass: lowers AST_Node expression nodes into the HIR, the algebraic
 * Data_Kind type in hir.h, checking lvalue declarations along the way.
 *
 * See hir.h for details.
 */
class AST_Lowering
{

  public:
    AST_Lowering(AST_Lowering const&) = delete;
    AST_Lowering& operator=(AST_Lowering const&) = delete;
    ~AST_Lowering() = default;

  public:
    using Data_Kind = hir::Data_Kind;
    using Literal = literal::Literal;
    using Node = util::AST_Node;
    using Parameters = std::vector<Data_Kind::Pointer>;

  public:
    explicit AST_Lowering(util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols = {})
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
    {
    }

    explicit AST_Lowering(util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols,
        Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }

  public:
    static inline Data_Kind lower(util::AST_Node const& node,
        util::AST_Node const& internals,
        Symbol_Table<> const& symbols = {},
        Symbol_Table<> const& globals = {})
    {
        auto lowering = AST_Lowering{ internals, symbols, globals };
        return lowering.lower_from_node(node);
    }

  public:
    Data_Kind lower_from_node(Node const& node);

    inline Data_Kind::Pointer make_expression_pointer_from_ast(Node const& node)
    {
        return std::make_shared<Data_Kind>(lower_from_node(node));
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
    Data_Kind from_evaluated_expression_node(Node const& node);
    Data_Kind from_function_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Kind from_relation_expression_node(Node const& node);

  private:
    Data_Kind from_ternary_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Kind from_unary_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Kind::LValue from_lvalue_expression_node(Node const& node);
    Literal from_indirect_identifier_node(Node const& node);
    Literal from_vector_idenfitier_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Kind from_assignment_expression_node(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Literal from_constant_expression_node(Node const& node);
    Literal from_integer_literal_node(Node const& node);
    Literal from_float_literal_node(Node const& node);
    Literal from_bool_literal_node(Node const& node);
    Literal from_double_literal_node(Node const& node);
    Literal from_string_literal_node(Node const& node);
    Literal from_constant_literal_node(Node const& node);

  private:
    void lowering_error(
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

inline hir::Data_Kind::Pointer lower_node_to_hir(util::AST_Node const& node,
    util::AST_Node const& internals,
    Symbol_Table<> const& symbols = {},
    Symbol_Table<> const& globals = {})
{
    return std::make_shared<hir::Data_Kind>(
        AST_Lowering::lower(node, internals, symbols, globals));
}

} // namespace language
