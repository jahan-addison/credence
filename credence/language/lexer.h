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

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/****************************************************************************
 *
 * B language lexer
 *
 * Hand-written tokenizer for the B language grammar (see grammar.lark),
 * with a re2c-generated scanner.
 *
 *****************************************************************************/

namespace credence::language {

enum class Token_Type
{
    END_OF_FILE,
    INVALID,

    // literals
    IDENTIFIER,
    INTEGER,
    FLOAT,
    CHAR_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,

    // keywords
    KEYWORD_AUTO,
    KEYWORD_EXTRN,
    KEYWORD_CASE,
    KEYWORD_GOTO,
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_RETURN,
    KEYWORD_WHILE,
    KEYWORD_SWITCH,
    KEYWORD_BREAK,

    // punctuation
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    QUESTION,
    COLON,
    DOT,
    SEMICOLON,

    // operators
    ASSIGN,  // =
    PLUS,    // +
    MINUS,   // -
    STAR,    // *
    SLASH,   // /
    PERCENT, // %
    AMP,     // &
    PIPE,    // |
    CARET,   // ^
    TILDE,   // ~
    BANG,    // !
    EQ,      // ==
    NEQ,     // !=
    LT,      // <
    LE,      // <=
    GT,      // >
    GE,      // >=
    SHL,     // <<
    SHR,     // >>
    AND_AND, // &&
    OR_OR,   // ||
    INC,     // ++
    DEC,     // --
};

std::string_view token_type_to_string(Token_Type type);

struct Token
{
    Token_Type type{ Token_Type::INVALID };
    std::string_view lexeme{};
    std::size_t line{ 1 };
    std::size_t column{ 1 };
    std::size_t start_pos{ 0 };
    std::size_t end_pos{ 0 };
    std::size_t end_column{ 1 };
};

/**
 * @brief re2c tokenizer over an in-memory source buffer.
 *
 * The Lexer owns the source buffer for its lifetime. Token::lexeme
 * is a string_view into it, so a Lexer (or its tokenize() result) must
 * outlive any Token taken from it.
 */
class Lexer
{
  public:
    explicit Lexer(std::string source);

    /**
     * @brief Scan and return the next token, including END_OF_FILE
     */
    Token next();

    /**
     * @brief SIMD-powered scanning to skip and count whitespace in chunks
     */
    void skip_whitespace();

    /**
     * @brief Scan the entire source, ending with one END_OF_FILE token
     */
    std::vector<Token> tokenize();

    std::string_view source() const { return source_; }

  private:
    std::string source_;
    const char* cursor_;
    const char* marker_;
    const char* limit_;
    const char* token_start_;
    std::size_t line_{ 1 };
    std::size_t column_{ 1 };

    Token make_token(Token_Type type);
    void advance_line_column(const char* from, const char* to);
};

} // namespace credence::language
