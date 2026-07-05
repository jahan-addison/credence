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

// GENERATED FILE -- do not edit lexer.cc by hand.
//
// Regenerate with:
//   re2c -o credence/language/lexer.cc credence/language/lexer.re
//
// Token rules are a direct port of the lexical grammar in grammar.lark

#include <credence/language/lexer.h>

namespace credence::language {

std::string_view token_type_to_string(Token_Type type)
{
    switch (type) {
        case Token_Type::END_OF_FILE:
            return "END_OF_FILE";
        case Token_Type::INVALID:
            return "INVALID";
        case Token_Type::IDENTIFIER:
            return "IDENTIFIER";
        case Token_Type::INTEGER:
            return "INTEGER";
        case Token_Type::FLOAT:
            return "FLOAT";
        case Token_Type::CHAR_LITERAL:
            return "CHAR_LITERAL";
        case Token_Type::STRING_LITERAL:
            return "STRING_LITERAL";
        case Token_Type::BOOL_LITERAL:
            return "BOOL_LITERAL";
        case Token_Type::KEYWORD_AUTO:
            return "KEYWORD_AUTO";
        case Token_Type::KEYWORD_EXTRN:
            return "KEYWORD_EXTRN";
        case Token_Type::KEYWORD_CASE:
            return "KEYWORD_CASE";
        case Token_Type::KEYWORD_GOTO:
            return "KEYWORD_GOTO";
        case Token_Type::KEYWORD_IF:
            return "KEYWORD_IF";
        case Token_Type::KEYWORD_ELSE:
            return "KEYWORD_ELSE";
        case Token_Type::KEYWORD_RETURN:
            return "KEYWORD_RETURN";
        case Token_Type::KEYWORD_WHILE:
            return "KEYWORD_WHILE";
        case Token_Type::KEYWORD_SWITCH:
            return "KEYWORD_SWITCH";
        case Token_Type::KEYWORD_BREAK:
            return "KEYWORD_BREAK";
        case Token_Type::LPAREN:
            return "LPAREN";
        case Token_Type::RPAREN:
            return "RPAREN";
        case Token_Type::LBRACE:
            return "LBRACE";
        case Token_Type::RBRACE:
            return "RBRACE";
        case Token_Type::LBRACKET:
            return "LBRACKET";
        case Token_Type::RBRACKET:
            return "RBRACKET";
        case Token_Type::COMMA:
            return "COMMA";
        case Token_Type::QUESTION:
            return "QUESTION";
        case Token_Type::COLON:
            return "COLON";
        case Token_Type::DOT:
            return "DOT";
        case Token_Type::SEMICOLON:
            return "SEMICOLON";
        case Token_Type::ASSIGN:
            return "ASSIGN";
        case Token_Type::PLUS:
            return "PLUS";
        case Token_Type::MINUS:
            return "MINUS";
        case Token_Type::STAR:
            return "STAR";
        case Token_Type::SLASH:
            return "SLASH";
        case Token_Type::PERCENT:
            return "PERCENT";
        case Token_Type::AMP:
            return "AMP";
        case Token_Type::PIPE:
            return "PIPE";
        case Token_Type::CARET:
            return "CARET";
        case Token_Type::TILDE:
            return "TILDE";
        case Token_Type::BANG:
            return "BANG";
        case Token_Type::EQ:
            return "EQ";
        case Token_Type::NEQ:
            return "NEQ";
        case Token_Type::LT:
            return "LT";
        case Token_Type::LE:
            return "LE";
        case Token_Type::GT:
            return "GT";
        case Token_Type::GE:
            return "GE";
        case Token_Type::SHL:
            return "SHL";
        case Token_Type::SHR:
            return "SHR";
        case Token_Type::AND_AND:
            return "AND_AND";
        case Token_Type::OR_OR:
            return "OR_OR";
        case Token_Type::INC:
            return "INC";
        case Token_Type::DEC:
            return "DEC";
    }
    return "INVALID";
}

/*!max:re2c*/

namespace {
constexpr std::size_t PADDING = (YYMAXFILL > 0) ? YYMAXFILL : 1;
}

Lexer::Lexer(std::string source)
    : source_(std::move(source))
{
    source_.append(PADDING, '\0');
    cursor_ = source_.data();
    marker_ = source_.data();
    limit_ = source_.data() + source_.size();
    token_start_ = cursor_;
}

void Lexer::advance_line_column(const char* from, const char* to)
{
    for (const char* p = from; p < to; ++p) {
        if (*p == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
    }
}

Token Lexer::make_token(Token_Type type)
{
    Token token;
    token.type = type;
    token.lexeme = std::string_view(
        token_start_, static_cast<std::size_t>(cursor_ - token_start_));
    token.start_pos =
        static_cast<std::size_t>(token_start_ - source_.data());
    token.end_pos = static_cast<std::size_t>(cursor_ - source_.data());
    token.line = line_;
    token.column = column_;
    advance_line_column(token_start_, cursor_);
    token.end_column = column_;
    return token;
}

Token Lexer::next()
{
    for (;;) {
        token_start_ = cursor_;
        /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = cursor_;
        re2c:define:YYMARKER = marker_;
        re2c:define:YYLIMIT = limit_;
        re2c:yyfill:enable = 0;

        end           = "\x00";
        ws            = [ \t\r\n]+;
        line_comment  = "//" [^\r\n]*;
        block_comment = "/*" ([^*] | ("*" [^/]))* "*" "/";

        digit         = [0-9];
        int_lit       = digit+;
        float_lit     = int_lit ("." int_lit)? [fF];
        name          = [a-zA-Z_][a-zA-Z0-9_.]{0,7};
        char_lit      = "'" [^\x00] [^\x00]? "'";
        string_lit    = "\"" ( "\\" [^\x00] | [^"\\\r\n\x00] )* "\"";

        end             { return make_token(Token_Type::END_OF_FILE); }
        ws              { advance_line_column(token_start_, cursor_); continue; }
        line_comment    { advance_line_column(token_start_, cursor_); continue; }
        block_comment   { advance_line_column(token_start_, cursor_); continue; }

        "auto"          { return make_token(Token_Type::KEYWORD_AUTO); }
        "extrn"         { return make_token(Token_Type::KEYWORD_EXTRN); }
        "case"          { return make_token(Token_Type::KEYWORD_CASE); }
        "goto"          { return make_token(Token_Type::KEYWORD_GOTO); }
        "if"            { return make_token(Token_Type::KEYWORD_IF); }
        "else"          { return make_token(Token_Type::KEYWORD_ELSE); }
        "return"        { return make_token(Token_Type::KEYWORD_RETURN); }
        "while"         { return make_token(Token_Type::KEYWORD_WHILE); }
        "switch"        { return make_token(Token_Type::KEYWORD_SWITCH); }
        "break"         { return make_token(Token_Type::KEYWORD_BREAK); }
        "true"          { return make_token(Token_Type::BOOL_LITERAL); }
        "false"         { return make_token(Token_Type::BOOL_LITERAL); }

        float_lit       { return make_token(Token_Type::FLOAT); }
        int_lit         { return make_token(Token_Type::INTEGER); }
        char_lit        { return make_token(Token_Type::CHAR_LITERAL); }
        string_lit      { return make_token(Token_Type::STRING_LITERAL); }
        name            { return make_token(Token_Type::IDENTIFIER); }

        "("             { return make_token(Token_Type::LPAREN); }
        ")"             { return make_token(Token_Type::RPAREN); }
        "{"             { return make_token(Token_Type::LBRACE); }
        "}"             { return make_token(Token_Type::RBRACE); }
        "["             { return make_token(Token_Type::LBRACKET); }
        "]"             { return make_token(Token_Type::RBRACKET); }
        ","             { return make_token(Token_Type::COMMA); }
        "?"             { return make_token(Token_Type::QUESTION); }
        ":"             { return make_token(Token_Type::COLON); }
        "."             { return make_token(Token_Type::DOT); }
        ";"             { return make_token(Token_Type::SEMICOLON); }

        "=="            { return make_token(Token_Type::EQ); }
        "!="            { return make_token(Token_Type::NEQ); }
        "<="            { return make_token(Token_Type::LE); }
        ">="            { return make_token(Token_Type::GE); }
        "<<"            { return make_token(Token_Type::SHL); }
        ">>"            { return make_token(Token_Type::SHR); }
        "&&"            { return make_token(Token_Type::AND_AND); }
        "||"            { return make_token(Token_Type::OR_OR); }
        "++"            { return make_token(Token_Type::INC); }
        "--"            { return make_token(Token_Type::DEC); }

        "="             { return make_token(Token_Type::ASSIGN); }
        "+"             { return make_token(Token_Type::PLUS); }
        "-"             { return make_token(Token_Type::MINUS); }
        "*"             { return make_token(Token_Type::STAR); }
        "/"             { return make_token(Token_Type::SLASH); }
        "%"             { return make_token(Token_Type::PERCENT); }
        "&"             { return make_token(Token_Type::AMP); }
        "|"             { return make_token(Token_Type::PIPE); }
        "^"             { return make_token(Token_Type::CARET); }
        "~"             { return make_token(Token_Type::TILDE); }
        "!"             { return make_token(Token_Type::BANG); }
        "<"             { return make_token(Token_Type::LT); }
        ">"             { return make_token(Token_Type::GT); }

        *               { return make_token(Token_Type::INVALID); }
        */
    }
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;
    for (;;) {
        Token token = next();
        bool done = token.type == Token_Type::END_OF_FILE;
        tokens.push_back(token);
        if (done)
            break;
    }
    return tokens;
}

} // namespace credence::language
