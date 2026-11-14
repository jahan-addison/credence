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

#include <credence/language/lexer.h>
#include <credence/util.h> // for AST_Node
#include <easyjson.h>      // for JSON
#include <optional>
#include <string>
#include <vector>

/****************************************************************************
 *
 * Brocoli language Parser
 *
 * Hand-written recursive-descent parser that transforms a token stream (see
 * lexer.h) into the same AST and parse tree structure produced by the
 * Python frontend's Lark transformer:
 *
 * https://github.com/jahan-addison/augur/blob/master/augur/transformer.py
 *
 *****************************************************************************/

namespace credence::language {

class Parser
{
  public:
    explicit Parser(std::string source);

    Parser(Parser const&) = delete;
    Parser& operator=(Parser const&) = delete;

  public:
    using Node = util::AST_Node;
    using Maybe_Node = std::optional<Node>;
    Node parse_program();

    static Node parse(std::string source);

    // identifier max size
    static constexpr std::size_t k_identifier_max_length{ 16 };

  private:
    Token const& current() const;
    Token const& peek(std::size_t ahead = 1) const;
    Token const& advance();
    bool check(Token_Type type) const;
    bool check_ahead(std::size_t ahead, Token_Type type) const;
    bool match(Token_Type type);
    Token const& expect(Token_Type type, std::string_view what);
    void error(std::string_view message) const;

    static Node make_node(std::string_view node_type,
        Node root,
        Maybe_Node left = std::nullopt,
        Maybe_Node right = std::nullopt);

    // clang-format off

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Node parse_definition();
    Node parse_function_definition(Token const& name);
    Node parse_vector_definition(Token const& name);
    Node parse_union_definition();
    Node parse_call_arguments_or_parameters();
    Node parse_vector_symbol();
    Node parse_union_tag_symbol();
    Maybe_Node parse_vector_size();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Maybe_Node parse_declaration();
    Node parse_union_declaration();
    Node parse_variable_declaration();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Node parse_function_body();
    Node parse_statement(bool allow_block_and_return);
    Node parse_block_statement();
    Node parse_label_statement();
    Node parse_auto_statement();
    Node parse_extrn_statement();
    Node parse_union_statement();
    Node parse_case_statement();
    Node parse_goto_statement();
    Node parse_if_statement();
    Node parse_return_statement();
    Node parse_rvalue_statement();
    Node parse_while_statement();
    Node parse_switch_statement();
    Node parse_break_statement();
    Node parse_expression();

  private:
    bool at_statement_keyword() const;
    bool at_label() const;
    bool at_expression_start() const;

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Node parse_rvalue();
    Node parse_rvalue_primary();
    Node parse_union_tag();
    Node parse_unary_operand();
    Node parse_lvalue();
    Node parse_constant();

  private:
    bool at_binary_operator() const;

    // clang-format on

  private:
    // lexer_ must outlive tokens_: Token::lexeme is a string_view into the
    // buffer it owns
    Lexer lexer_;
    std::vector<Token> tokens_;
    std::size_t pos_{ 0 };
};

} // namespace credence::language
