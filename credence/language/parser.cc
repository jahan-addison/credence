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

#include <credence/language/parser.h>

#include <cctype>           // for isspace
#include <credence/error.h> // for credence_error
#include <easyjson.h>       // for JSON
#include <fmt/format.h>     // for format
#include <matchit.h>        // for match, pattern, or_
#include <string>
#include <utility>

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

namespace m = matchit;

namespace {

using util::AST_Node;

AST_Node json_string(std::string s)
{
    return AST_Node(std::move(s));
}

AST_Node json_int(long v)
{
    return AST_Node(v);
}

AST_Node json_float(double v)
{
    return AST_Node(v);
}

AST_Node operator_root(std::string_view op)
{
    auto arr = util::AST::array();
    arr.append(json_string(std::string(op)));
    return arr;
}

bool is_lvalue_shaped(AST_Node const& node)
{
    auto type = node["node"].to_string();
    return type == "lvalue" || type == "vector_lvalue" ||
           type == "indirect_lvalue";
}

} // namespace

Parser::Parser(std::string source)
    : lexer_(std::move(source))
{
    tokens_ = lexer_.tokenize();
}

/**
 * @brief Parse a whole source program in one call
 */
util::AST_Node Parser::parse(std::string source)
{
    Parser parser{ std::move(source) };
    return parser.parse_program();
}

/**
 * @brief The token at the current parse position
 */
Token const& Parser::current() const
{
    return tokens_[pos_];
}

/**
 * @brief The token `ahead` positions past the current one, clamped to EOF
 */
Token const& Parser::peek(std::size_t ahead) const
{
    auto index = pos_ + ahead;
    if (index >= tokens_.size())
        return tokens_.back();
    return tokens_[index];
}

/**
 * @brief Consume and return the current token
 */
Token const& Parser::advance()
{
    auto const& token = current();
    if (pos_ + 1 < tokens_.size())
        ++pos_;
    return token;
}

bool Parser::check(Token_Type type) const
{
    return current().type == type;
}

bool Parser::check_ahead(std::size_t ahead, Token_Type type) const
{
    return peek(ahead).type == type;
}

/**
 * @brief Consume the current token if it matches type
 */
bool Parser::match(Token_Type type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

/**
 * @brief Consume the current token, or raise a syntax error naming `what`
 */
Token const& Parser::expect(Token_Type type, std::string_view what)
{
    if (!check(type)) {
        error(
            fmt::format("expected {} but found '{}'", what, current().lexeme));
    }
    return advance();
}

/**
 * @brief Raise a syntax error at the current token's source position
 */
void Parser::error(std::string_view message) const
{
    auto const& token = current();
    credence_error(fmt::format("syntax error at line {} column {}: {}",
        token.line,
        token.column,
        message));
}

/**
 * @brief AST_Node factory matching augur's transformer __construct_node
 */
util::AST_Node Parser::make_node(std::string_view node_type,
    util::AST_Node root,
    std::optional<util::AST_Node> left,
    std::optional<util::AST_Node> right)
{
    auto node = util::AST::object();
    node["node"] = json_string(std::string(node_type));
    node["root"] = root;
    if (left.has_value())
        node["left"] = *left;
    if (right.has_value())
        node["right"] = *right;
    return node;
}

/**
 * @brief Program root: a run of function and vector definitions
 */
util::AST_Node Parser::parse_program()
{
    auto definitions = util::AST::array();
    while (!check(Token_Type::END_OF_FILE)) {
        definitions.append(parse_definition());
    }
    return make_node(
        "program", json_string("definitions"), definitions, std::nullopt);
}

/**
 * @brief Dispatch a top-level definition by whether '(' follows its name
 */
util::AST_Node Parser::parse_definition()
{
    auto name = expect(Token_Type::IDENTIFIER, "an identifier");
    if (check(Token_Type::LPAREN))
        return parse_function_definition(name);
    return parse_vector_definition(name);
}

/**
 * @brief Parse a function definition's parameter list and body
 */
util::AST_Node Parser::parse_function_definition(Token const& name)
{
    advance(); // '('
    auto parameters = parse_call_arguments_or_parameters();
    expect(Token_Type::RPAREN, "')'");
    auto body = parse_function_body();
    return make_node("function_definition",
        json_string(std::string(name.lexeme)),
        parameters,
        body);
}

/**
 * @brief Parse a vector definition's optional size and initial values
 */
util::AST_Node Parser::parse_vector_definition(Token const& name)
{
    auto size = parse_v_size();
    auto values = util::AST::array();
    if (!check(Token_Type::SEMICOLON)) {
        values.append(parse_v_symbol());
        while (!check(Token_Type::SEMICOLON)) {
            match(Token_Type::COMMA); // the separating comma is optional
            values.append(parse_v_symbol());
        }
    }
    expect(Token_Type::SEMICOLON, "';'");
    return make_node("vector_definition",
        json_string(std::string(name.lexeme)),
        size,
        values);
}

/**
 * @brief Parse a parenthesized rvalue list, shared by calls and definitions
 */
util::AST_Node Parser::parse_call_arguments_or_parameters()
{
    auto items = util::AST::array();
    if (check(Token_Type::RPAREN)) {
        // Matches Lark's maybe_placeholder for an entirely absent parameter
        // list: a single JSON null, not an empty array.
        items.append(util::AST_Node{});
        return items;
    }
    items.append(parse_rvalue());
    while (match(Token_Type::COMMA)) {
        items.append(parse_rvalue());
    }
    return items;
}

/**
 * @brief Parse one vector initial value: either a constant or a bare name
 */
util::AST_Node Parser::parse_v_symbol()
{
    if (check(Token_Type::IDENTIFIER)) {
        auto name = std::string(current().lexeme);
        advance();
        return json_string(name);
    }
    return parse_constant();
}

/**
 * @brief Parse a vector's optional "[" constant? "]" size
 */
std::optional<util::AST_Node> Parser::parse_v_size()
{
    if (!check(Token_Type::LBRACKET))
        return std::nullopt;
    advance(); // '['
    if (check(Token_Type::RBRACKET)) {
        // The original grammar's v_size rule crashes the reference
        // transformer on empty brackets (name[];); treat it the same as
        // brackets being absent entirely rather than reproduce the bug.
        advance();
        return std::nullopt;
    }
    auto size = parse_constant();
    expect(Token_Type::RBRACKET, "']'");
    return size;
}

/**
 * @brief Parse a function body: statement_2* followed by an optional return
 */
util::AST_Node Parser::parse_function_body()
{
    expect(Token_Type::LBRACE, "'{'");
    auto body = util::AST::array();
    while (!check(Token_Type::RBRACE) && !check(Token_Type::KEYWORD_RETURN)) {
        body.append(parse_statement(false));
    }
    if (check(Token_Type::KEYWORD_RETURN)) {
        body.append(parse_return_statement());
    }
    expect(Token_Type::RBRACE, "'}'");
    return make_node("statement", json_string("block"), body, std::nullopt);
}

/**
 * @brief Whether the current token starts a keyword statement
 *
 * 'case' and 'else' are not statements on their own -- 'case' only appears
 * inside a switch body, and 'else' is consumed directly by
 * parse_if_statement() -- but both must still terminate a run of
 * rvalue_statement expressions the same way the other statement keywords do
 * (e.g. "if (x) x = 1; else x = 2;").
 */
bool Parser::at_statement_keyword() const
{
    return m::match(current().type)(
        m::pattern | m::or_(Token_Type::KEYWORD_AUTO,
                         Token_Type::KEYWORD_EXTRN,
                         Token_Type::KEYWORD_IF,
                         Token_Type::KEYWORD_WHILE,
                         Token_Type::KEYWORD_SWITCH,
                         Token_Type::KEYWORD_BREAK,
                         Token_Type::KEYWORD_GOTO,
                         Token_Type::KEYWORD_RETURN,
                         Token_Type::KEYWORD_CASE,
                         Token_Type::KEYWORD_ELSE) = [] { return true; },
        m::pattern | m::_ = [] { return false; });
}

/**
 * @brief Whether the current token is a NAME immediately followed by ':'
 */
bool Parser::at_label() const
{
    return check(Token_Type::IDENTIFIER) && check_ahead(1, Token_Type::COLON);
}

/**
 * @brief Dispatch a statement by its leading token
 *
 * allow_block_and_return distinguishes `statement` (used inside blocks,
 * if/while/switch bodies) from `statement_2` (used directly in a function
 * body, where a block or a return would be ambiguous with the function's
 * own trailing return_statement).
 */
util::AST_Node Parser::parse_statement(bool allow_block_and_return)
{
    if (allow_block_and_return && check(Token_Type::LBRACE))
        return parse_block_statement();
    if (at_label())
        return parse_label_statement();
    return m::match(current().type)(
        m::pattern |
            Token_Type::KEYWORD_AUTO = [&] { return parse_auto_statement(); },
        m::pattern |
            Token_Type::KEYWORD_EXTRN = [&] { return parse_extrn_statement(); },
        m::pattern |
            Token_Type::KEYWORD_IF = [&] { return parse_if_statement(); },
        m::pattern |
            Token_Type::KEYWORD_WHILE = [&] { return parse_while_statement(); },
        m::pattern | Token_Type::KEYWORD_SWITCH =
            [&] { return parse_switch_statement(); },
        m::pattern |
            Token_Type::KEYWORD_BREAK = [&] { return parse_break_statement(); },
        m::pattern |
            Token_Type::KEYWORD_GOTO = [&] { return parse_goto_statement(); },
        m::pattern | Token_Type::KEYWORD_RETURN =
            [&] {
                if (!allow_block_and_return)
                    error("'return' is only allowed as the last statement of "
                          "a function body");
                return parse_return_statement();
            },
        m::pattern | m::_ = [&] { return parse_rvalue_statement(); });
}

/**
 * @brief Parse "{" statement* "}"
 */
util::AST_Node Parser::parse_block_statement()
{
    expect(Token_Type::LBRACE, "'{'");
    auto body = util::AST::array();
    while (!check(Token_Type::RBRACE)) {
        body.append(parse_statement(true));
    }
    expect(Token_Type::RBRACE, "'}'");
    return make_node("statement", json_string("block"), body, std::nullopt);
}

/**
 * @brief Parse a label: NAME ":"
 */
util::AST_Node Parser::parse_label_statement()
{
    auto name = std::string(current().lexeme);
    auto name_end = current().end_pos;
    advance(); // NAME
    auto colon_start = current().start_pos;
    expect(Token_Type::COLON, "':'");
    // The original grammar lexed a label as a single `NAME [\s]* ":"`
    // token and built its name by slicing off just the trailing ':', so
    // whitespace between the name and ':' (see examples/2.b's "loop :")
    // ends up preserved in the label's name. Match that by carrying over
    // whatever the source actually had there.
    if (colon_start > name_end)
        name += lexer_.source().substr(name_end, colon_start - name_end);
    auto left = util::AST::array();
    left.append(json_string(name));
    return make_node("statement", json_string("label"), left, std::nullopt);
}

/**
 * @brief Parse "auto" lvalue ("," lvalue)* ";"
 */
util::AST_Node Parser::parse_auto_statement()
{
    advance(); // 'auto'
    auto left = util::AST::array();
    left.append(parse_lvalue());
    while (match(Token_Type::COMMA)) {
        left.append(parse_lvalue());
    }
    expect(Token_Type::SEMICOLON, "';'");
    return make_node("statement", json_string("auto"), left, std::nullopt);
}

/**
 * @brief Parse "extrn" NAME ("," NAME)* ";"
 */
util::AST_Node Parser::parse_extrn_statement()
{
    advance(); // 'extrn'
    auto left = util::AST::array();
    auto name =
        std::string(expect(Token_Type::IDENTIFIER, "an identifier").lexeme);
    left.append(make_node("lvalue", json_string(name)));
    while (match(Token_Type::COMMA)) {
        name =
            std::string(expect(Token_Type::IDENTIFIER, "an identifier").lexeme);
        left.append(make_node("lvalue", json_string(name)));
    }
    expect(Token_Type::SEMICOLON, "';'");
    return make_node("statement", json_string("extrn"), left, std::nullopt);
}

/**
 * @brief Parse "case" constant ":" statement*
 */
util::AST_Node Parser::parse_case_statement()
{
    advance(); // 'case'
    auto value = parse_constant();
    expect(Token_Type::COLON, "':'");
    auto body = util::AST::array();
    while (!check(Token_Type::KEYWORD_CASE) && !check(Token_Type::RBRACE)) {
        body.append(parse_statement(true));
    }
    return make_node("statement", json_string("case"), value, body);
}

/**
 * @brief Parse "goto" NAME ";"
 */
util::AST_Node Parser::parse_goto_statement()
{
    advance(); // 'goto'
    auto name =
        std::string(expect(Token_Type::IDENTIFIER, "an identifier").lexeme);
    expect(Token_Type::SEMICOLON, "';'");
    auto left = util::AST::array();
    left.append(json_string(name));
    return make_node("statement", json_string("goto"), left, std::nullopt);
}

/**
 * @brief Parse "if" "(" rvalue ")" statement ["else" statement]
 */
util::AST_Node Parser::parse_if_statement()
{
    advance(); // 'if'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");
    auto branches = util::AST::array();
    branches.append(parse_statement(true));
    if (match(Token_Type::KEYWORD_ELSE)) {
        branches.append(parse_statement(true));
    } else {
        branches.append(util::AST_Node{});
    }
    return make_node("statement", json_string("if"), condition, branches);
}

/**
 * @brief Parse "return" ["(" rvalue ")"] ";"
 */
util::AST_Node Parser::parse_return_statement()
{
    advance(); // 'return'
    auto left = util::AST::array();
    if (match(Token_Type::LPAREN)) {
        left.append(parse_rvalue());
        expect(Token_Type::RPAREN, "')'");
    } else {
        left.append(util::AST_Node{});
    }
    expect(Token_Type::SEMICOLON, "';'");
    return make_node("statement", json_string("return"), left, std::nullopt);
}

/**
 * @brief Whether another expression can start here within a rvalue_statement
 */
bool Parser::at_expression_start() const
{
    return !at_statement_keyword() && !at_label() &&
           !check(Token_Type::LBRACE) && !check(Token_Type::RBRACE) &&
           !check(Token_Type::END_OF_FILE);
}

/**
 * @brief Parse one ";" or "rvalue ;", the unit rvalue_statement repeats
 */
util::AST_Node Parser::parse_expression()
{
    auto items = util::AST::array();
    if (!match(Token_Type::SEMICOLON)) {
        items.append(parse_rvalue());
        expect(Token_Type::SEMICOLON, "';'");
    }
    return items;
}

/**
 * @brief Parse expression+, greedily absorbing consecutive ";" statements
 */
util::AST_Node Parser::parse_rvalue_statement()
{
    auto left = util::AST::array();
    left.append(parse_expression());
    while (at_expression_start()) {
        left.append(parse_expression());
    }
    return make_node("statement", json_string("rvalue"), left, std::nullopt);
}

/**
 * @brief Parse "while" "(" rvalue ")" statement
 */
util::AST_Node Parser::parse_while_statement()
{
    advance(); // 'while'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");
    auto body = util::AST::array();
    body.append(parse_statement(true));
    return make_node("statement", json_string("while"), condition, body);
}

/**
 * @brief Parse "switch" "(" rvalue ")" "{" case_statement* "}"
 */
util::AST_Node Parser::parse_switch_statement()
{
    advance(); // 'switch'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");
    expect(Token_Type::LBRACE, "'{'");
    auto cases = util::AST::array();
    while (check(Token_Type::KEYWORD_CASE)) {
        cases.append(parse_case_statement());
    }
    expect(Token_Type::RBRACE, "'}'");
    return make_node("statement", json_string("switch"), condition, cases);
}

/**
 * @brief Parse "break" ";"
 */
util::AST_Node Parser::parse_break_statement()
{
    advance(); // 'break'
    expect(Token_Type::SEMICOLON, "';'");
    return make_node("statement", json_string("break"));
}

/**
 * @brief Whether the current token is one of the flat "binary" operators
 */
bool Parser::at_binary_operator() const
{
    return m::match(current().type)(
        m::pattern | m::or_(Token_Type::OR_OR,
                         Token_Type::AND_AND,
                         Token_Type::AMP,
                         Token_Type::PIPE,
                         Token_Type::EQ,
                         Token_Type::NEQ,
                         Token_Type::LT,
                         Token_Type::LE,
                         Token_Type::GT,
                         Token_Type::GE,
                         Token_Type::CARET,
                         Token_Type::SHL,
                         Token_Type::SHR,
                         Token_Type::MINUS,
                         Token_Type::PLUS,
                         Token_Type::PERCENT,
                         Token_Type::STAR,
                         Token_Type::SLASH) = [] { return true; },
        m::pattern | m::_ = [] { return false; });
}

/**
 * @brief Parse an rvalue
 *
 * Reproduces the original grammar's flat, always-right-associative
 * structure: parse one primary/prefixed operand, then check the single
 * next token for '=' (assignment), '?' (ternary), or a binary operator,
 * recursing into parse_rvalue() again for the right-hand side in each
 * case. See parser.h for why this, rather than real precedence, is the
 * correct thing to reproduce: the shunting-yard Shunting_Yard in
 * shunting_yard.h is what recovers correct precedence downstream, not this
 * parse tree's shape.
 */
util::AST_Node Parser::parse_rvalue()
{
    auto left = parse_rvalue_primary();

    // A non-lvalue-shaped left here isn't necessarily a syntax error: left
    // may just be an intermediate operand (e.g. the parenthesized operand
    // of '*' in "*(p + q) = 1") whose enclosing rvalue is the one this '='
    // actually belongs to. Leaving it unconsumed lets that caller see it;
    // if nothing ever claims it, expect(SEMICOLON, ...) reports the error.
    if (check(Token_Type::ASSIGN) && is_lvalue_shaped(left)) {
        advance();
        auto right = parse_rvalue();
        return make_node(
            "assignment_expression", operator_root("="), left, right);
    }

    if (check(Token_Type::QUESTION)) {
        advance();
        auto then_node = parse_rvalue();
        expect(Token_Type::COLON, "':'");
        auto else_node = parse_rvalue();
        return make_node("ternary_expression", left, then_node, else_node);
    }

    if (at_binary_operator()) {
        auto op = std::string(current().lexeme);
        advance();
        auto right = parse_rvalue();
        return make_node("relation_expression", operator_root(op), left, right);
    }

    return left;
}

/**
 * @brief Parse the '&' / unary_modifier prefixed forms, else fall through
 */
util::AST_Node Parser::parse_rvalue_primary()
{
    return m::match(current().type)(
        m::pattern | Token_Type::AMP =
            [&] {
                advance();
                auto operand = parse_lvalue();
                return make_node(
                    "address_of_expression", operator_root("&"), operand);
            },
        m::pattern | m::or_(Token_Type::MINUS,
                         Token_Type::PLUS,
                         Token_Type::BANG,
                         Token_Type::TILDE) =
            [&] {
                auto op = std::string(current().lexeme);
                advance();
                auto operand = parse_unary_operand();
                return make_node(
                    "unary_expression", operator_root(op), operand);
            },
        m::pattern | m::_ = [&] { return parse_unary_operand(); });
}

/**
 * @brief Parse the restricted unary_operand grammar
 *
 * Deliberately excludes address_of_expression and unary_expression: the
 * original grammar does not allow nesting them (no "- -x" or "-&x"), only
 * pre/post inc-dec, constants, parenthesized expressions, function calls,
 * and plain lvalues.
 */
util::AST_Node Parser::parse_unary_operand()
{
    return m::match(current().type)(
        m::pattern | m::or_(Token_Type::INC, Token_Type::DEC) =
            [&] {
                auto op = std::string(current().lexeme);
                advance();
                auto operand = parse_lvalue();
                return make_node(
                    "pre_inc_dec_expression", operator_root(op), operand);
            },
        m::pattern | Token_Type::LPAREN =
            [&] {
                advance();
                auto inner = parse_rvalue();
                expect(Token_Type::RPAREN, "')'");
                return make_node("evaluated_expression", inner);
            },
        m::pattern |
            m::or_(Token_Type::INTEGER,
                Token_Type::FLOAT,
                Token_Type::CHAR_LITERAL,
                Token_Type::STRING_LITERAL,
                Token_Type::BOOL_LITERAL) = [&] { return parse_constant(); },
        m::pattern | m::or_(Token_Type::IDENTIFIER, Token_Type::STAR) =
            [&] {
                auto lvalue_node = parse_lvalue();
                if (match(Token_Type::LPAREN)) {
                    auto args = parse_call_arguments_or_parameters();
                    expect(Token_Type::RPAREN, "')'");
                    return make_node("function_expression",
                        lvalue_node["root"],
                        lvalue_node,
                        args);
                }
                if (check(Token_Type::INC) || check(Token_Type::DEC)) {
                    auto op = std::string(current().lexeme);
                    advance();
                    return make_node("post_inc_dec_expression",
                        operator_root(op),
                        std::nullopt,
                        lvalue_node);
                }
                return lvalue_node;
            },
        m::pattern | m::_ =
            [&] {
                error(fmt::format(
                    "expected an expression, found '{}'", current().lexeme));
                return util::AST_Node{};
            });
}

/**
 * @brief Parse "*" rvalue | NAME, with any number of trailing "[" rvalue "]"
 */
util::AST_Node Parser::parse_lvalue()
{
    util::AST_Node node;
    if (check(Token_Type::STAR)) {
        advance();
        auto operand = parse_rvalue();
        node = make_node("indirect_lvalue", operator_root("*"), operand);
    } else {
        auto name =
            std::string(expect(Token_Type::IDENTIFIER, "an lvalue").lexeme);
        node = make_node("lvalue", json_string(name));
    }
    while (check(Token_Type::LBRACKET)) {
        advance();
        auto index = parse_rvalue();
        expect(Token_Type::RBRACKET, "']'");
        auto base_name = node["root"];
        node = make_node("vector_lvalue", base_name, index);
    }
    return node;
}

/**
 * @brief Parse a numeric, char, string, or boolean constant
 */
util::AST_Node Parser::parse_constant()
{
    return m::match(current().type)(
        m::pattern | Token_Type::FLOAT =
            [&] {
                auto text = current().lexeme;
                advance();
                auto value =
                    std::stod(std::string(text.substr(0, text.size() - 1)));
                return make_node("float_literal", json_float(value));
            },
        m::pattern | Token_Type::INTEGER =
            [&] {
                auto text = std::string(current().lexeme);
                advance();
                if (check(Token_Type::DOT) &&
                    check_ahead(1, Token_Type::INTEGER)) {
                    advance(); // '.'
                    auto frac = std::string(current().lexeme);
                    advance();
                    auto value = std::stod(text + "." + frac);
                    return make_node("double_literal", json_float(value));
                }
                return make_node("integer_literal", json_int(std::stol(text)));
            },
        m::pattern | Token_Type::CHAR_LITERAL =
            [&] {
                auto text = current().lexeme;
                advance();
                auto inner = text.substr(1, text.size() - 2);
                // The original grammar has a second, redundant-looking
                // char rule matching a quoted single whitespace character
                // (' ') that, unlike the general case, keeps its
                // surrounding quotes in the AST. Reproduce that quirk
                // rather than "fixing" it, since examples/1.b's ' ' case
                // relies on it.
                if (inner.size() == 1 &&
                    std::isspace(static_cast<unsigned char>(inner[0])))
                    return make_node(
                        "constant_literal", json_string(std::string(text)));
                return make_node(
                    "constant_literal", json_string(std::string(inner)));
            },
        m::pattern | Token_Type::STRING_LITERAL =
            [&] {
                auto text = std::string(current().lexeme);
                advance();
                return make_node("string_literal", json_string(text));
            },
        m::pattern | Token_Type::BOOL_LITERAL =
            [&] {
                auto text = std::string(current().lexeme);
                advance();
                return make_node("bool_literal", json_string(text));
            },
        m::pattern | m::_ =
            [&] {
                error(fmt::format(
                    "expected a constant, found '{}'", current().lexeme));
                return util::AST_Node{};
            });
}

} // namespace credence::language
