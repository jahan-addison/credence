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
 * B language Parser
 *
 * Hand-written recursive-descent parser that transforms a token stream (see
 * lexer.h) into the same AST and parse tree shape produced by the Python
 * frontend's Lark transformer:
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
    util::AST_Node parse_program();

    static util::AST_Node parse(std::string source);

  private:
    Token const& current() const;
    Token const& peek(std::size_t ahead = 1) const;
    Token const& advance();
    bool check(Token_Type type) const;
    bool check_ahead(std::size_t ahead, Token_Type type) const;
    bool match(Token_Type type);
    Token const& expect(Token_Type type, std::string_view what);
    void error(std::string_view message) const;

    static util::AST_Node make_node(std::string_view node_type,
        util::AST_Node root,
        std::optional<util::AST_Node> left = std::nullopt,
        std::optional<util::AST_Node> right = std::nullopt);

    // clang-format off

  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node parse_definition();
    util::AST_Node parse_function_definition(Token const& name);
    util::AST_Node parse_vector_definition(Token const& name);
    util::AST_Node parse_call_arguments_or_parameters();
    util::AST_Node parse_v_symbol();
    std::optional<util::AST_Node> parse_v_size();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node parse_function_body();
    util::AST_Node parse_statement(bool allow_block_and_return);
    util::AST_Node parse_block_statement();
    util::AST_Node parse_label_statement();
    util::AST_Node parse_auto_statement();
    util::AST_Node parse_extrn_statement();
    util::AST_Node parse_case_statement();
    util::AST_Node parse_goto_statement();
    util::AST_Node parse_if_statement();
    util::AST_Node parse_return_statement();
    util::AST_Node parse_rvalue_statement();
    util::AST_Node parse_while_statement();
    util::AST_Node parse_switch_statement();
    util::AST_Node parse_break_statement();
    util::AST_Node parse_expression();

  private:
    bool at_statement_keyword() const;
    bool at_label() const;
    bool at_expression_start() const;

  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node parse_rvalue();
    util::AST_Node parse_rvalue_primary();
    util::AST_Node parse_unary_operand();
    util::AST_Node parse_lvalue();
    util::AST_Node parse_constant();

  private:
    bool at_binary_operator() const;

    // clang-format on

  private:
    // lexer_ must outlive tokens_: Token::lexeme is a string_view into the
    // buffer it owns (see lexer.h).
    Lexer lexer_;
    std::vector<Token> tokens_;
    std::size_t pos_{ 0 };
};

} // namespace credence::language
