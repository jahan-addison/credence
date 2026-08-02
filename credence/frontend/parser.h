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

#include <credence/frontend/ast.h>   // for AST, Node, Node_Index, Span
#include <credence/frontend/lexer.h> // for Lexer, Token, Token_Type
#include <credence/util.h>           // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>                   // for size_t
#include <cstdint>                   // for uint32_t
#include <string>                    // for string
#include <string_view>               // for string_view
#include <unordered_map>             // for unordered_map
#include <vector>                    // for vector

/****************************************************************************
 *
 * B language parser
 *
 * Hand-written recursive descent over the token stream that emits the flat
 * AST in ast.h. Every parse entry point returns an index into the node
 * array and not a node by value, so no node is ever copied or heap
 * allocated.
 *
 * Precedence is not resolved here. A chain such as "a + b * c" is emitted
 * in source order as a right-leaning Binary_Expression spine that the
 * shunting-yard pass in the HIR reshapes. This keeps the parser a pure
 * syntax pass and puts precedence in one place.
 *
 *****************************************************************************/

namespace credence::frontend {

class Parser
{
  public:
    explicit Parser(std::string source);

    Parser(Parser const&) = delete;
    Parser& operator=(Parser const&) = delete;

  public:
    /**
     * @brief Parse the token stream into a complete tree
     */
    ast::AST parse_program();

    /**
     * @brief Parse a whole source program in one call
     */
    static ast::AST parse(std::string source);

  private:
    Token const& current() const;
    Token const& peek(std::size_t ahead = 1) const;
    Token const& advance();
    bool check(Token_Type type) const;
    bool check_ahead(std::size_t ahead, Token_Type type) const;
    bool match(Token_Type type);
    Token const& expect(Token_Type type, std::string_view what);
    void error(std::string_view message) const;

  private:
    /**
     * @brief Intern text and return a stable handle for equal text
     *
     * The text must be a view into the lexer's source buffer, either a
     * lexeme or a slice of one, since interned_ keys on it directly.
     */
    ast::String_Index intern(std::string_view text);

    /**
     * @brief Append a node with the source span of `token`
     */
    ast::Node_Index add(ast::Node node, Token const& token);

    /**
     * @brief Append a node spanning from `token` to the previous token
     */
    ast::Node_Index add_spanning(ast::Node node, Token const& token);

    /**
     * @brief Move a scratch run of children into AST::extra
     */
    ast::Span commit(std::size_t scratch_base);

    /**
     * @brief Reserve `count` slots in AST::extra, returning the first
     */
    std::uint32_t reserve_extra(std::uint32_t count);

    // clang-format off

  CREDENCE_PRIVATE_UNLESS_TESTED:
    ast::Node_Index parse_definition();
    ast::Node_Index parse_function_definition(Token const& name);
    ast::Node_Index parse_vector_definition(Token const& name);
    ast::Node_Index parse_union_definition();
    ast::Node_Index parse_call_arguments_or_parameters();
    ast::Node_Index parse_vector_symbol();
    ast::Node_Index parse_vector_size();
    ast::Node_Index parse_function_body();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    ast::Node_Index parse_statement(bool allow_block_and_return);
    ast::Node_Index parse_block_statement();
    ast::Node_Index parse_label_statement();
    ast::Node_Index parse_auto_statement();
    ast::Node_Index parse_extrn_statement();
    ast::Node_Index parse_case_statement();
    ast::Node_Index parse_goto_statement();
    ast::Node_Index parse_if_statement();
    ast::Node_Index parse_return_statement();
    ast::Node_Index parse_rvalue_statement();
    ast::Node_Index parse_while_statement();
    ast::Node_Index parse_switch_statement();
    ast::Node_Index parse_break_statement();
    ast::Node_Index parse_expression();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    ast::Node_Index parse_rvalue();
    ast::Node_Index parse_rvalue_primary();
    ast::Node_Index parse_unary_operand();
    ast::Node_Index parse_lvalue();
    ast::Node_Index parse_constant();

    // clang-format on

  private:
    bool at_statement_keyword() const;
    bool at_label() const;
    bool at_expression_start() const;
    bool at_binary_operator() const;
    bool is_lvalue_shaped(ast::Node_Index index) const;

  private:
    Lexer lexer_;
    std::vector<Token> tokens_;
    std::size_t pos_{ 0 };

    ast::AST ast_{};

    /**
     * Child indices are gathered here while a list is being parsed, then
     * moved into AST::extra in one contiguous run. Nested lists share the
     * buffer as a stack, so it allocates a handful of times for the whole
     * parse instead of once per list.
     */
    std::vector<ast::Node_Index> scratch_{};

    /**
     * @brief Interning table: text -> handle
     */
    std::unordered_map<std::string_view, ast::String_Index> interned_{};
};

} // namespace credence::frontend
