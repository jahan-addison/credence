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

#include <credence/frontend/parser.h>

#include <cctype>           // for isspace
#include <charconv>         // for from_chars
#include <credence/error.h> // for credence_error
#include <cstdlib>          // for strtod
#include <cstring>          // for memcpy
#include <fmt/format.h>     // for format
#include <matchit.h>        // for match, pattern, or_
#include <string>           // for string
#include <utility>          // for move

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

namespace m = matchit;

namespace {

using ast::Node;
using ast::Node_Index;
using ast::null_node_index;
using ast::null_string_index;
using ast::Operator;
using ast::Span;
using ast::Type;

/**
 * @brief A node with a child list
 */
Node span_node(Type type, Span span)
{
    Node node{};
    node.type = type;
    node.op = Operator::None;
    node.data.span = span;
    return node;
}

/**
 * @brief A node with exactly two children
 */
Node binary_node(Type type, Operator op, Node_Index lhs, Node_Index rhs)
{
    Node node{};
    node.type = type;
    node.op = op;
    node.data.binary.lhs = lhs;
    node.data.binary.rhs = rhs;
    return node;
}

/**
 * @brief A node with exactly one child
 */
Node unary_node(Type type, Operator op, Node_Index child)
{
    Node node{};
    node.type = type;
    node.op = op;
    node.data.unary = child;
    return node;
}

/**
 * @brief A node with interned text
 */
Node string_node(Type type, ast::String_Index handle)
{
    Node node{};
    node.type = type;
    node.op = Operator::None;
    node.data.string = handle;
    return node;
}

/**
 * @brief Read a base 10 integer literal without allocating
 *
 * Out of range and malformed input yield 0. The lexer has already
 * established that the token is an integer.
 */
std::int64_t parse_integer(std::string_view text)
{
    std::int64_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

/**
 * @brief Read a floating literal without allocating
 *
 * std::from_chars for floating point is not yet available across the
 * toolchains this builds on, so the digits are copied into a fixed stack
 * buffer and handed to strtod. Anything longer than the buffer is far past
 * the precision a double can carry.
 */
double parse_real(std::string_view text)
{
    char buffer[64];
    auto length =
        text.size() < sizeof(buffer) - 1 ? text.size() : sizeof(buffer) - 1;
    std::memcpy(buffer, text.data(), length);
    buffer[length] = '\0';
    return std::strtod(buffer, nullptr);
}

/**
 * @brief The binary operator a token denotes, or None
 */
Operator binary_operator_of(Token_Type type)
{
    return m::match(type)(
        m::pattern | Token_Type::OR_OR = [] { return Operator::Or; },
        m::pattern | Token_Type::AND_AND = [] { return Operator::And; },
        m::pattern | Token_Type::PIPE = [] { return Operator::Bit_Or; },
        m::pattern | Token_Type::AMP = [] { return Operator::Bit_And; },
        m::pattern | Token_Type::CARET = [] { return Operator::Xor; },
        m::pattern | Token_Type::EQ = [] { return Operator::Eq; },
        m::pattern | Token_Type::NEQ = [] { return Operator::Neq; },
        m::pattern | Token_Type::LT = [] { return Operator::Lt; },
        m::pattern | Token_Type::LE = [] { return Operator::Lte; },
        m::pattern | Token_Type::GT = [] { return Operator::Gt; },
        m::pattern | Token_Type::GE = [] { return Operator::Gte; },
        m::pattern | Token_Type::SHL = [] { return Operator::Lshift; },
        m::pattern | Token_Type::SHR = [] { return Operator::Rshift; },
        m::pattern | Token_Type::PLUS = [] { return Operator::Add; },
        m::pattern | Token_Type::MINUS = [] { return Operator::Sub; },
        m::pattern | Token_Type::STAR = [] { return Operator::Mul; },
        m::pattern | Token_Type::SLASH = [] { return Operator::Div; },
        m::pattern | Token_Type::PERCENT = [] { return Operator::Mod; },
        m::pattern | m::_ = [] { return Operator::None; });
}

/**
 * @brief The unary modifier a token denotes, or None
 */
Operator unary_operator_of(Token_Type type)
{
    return m::match(type)(
        m::pattern | Token_Type::MINUS = [] { return Operator::Minus; },
        m::pattern | Token_Type::PLUS = [] { return Operator::Plus; },
        m::pattern | Token_Type::BANG = [] { return Operator::Not; },
        m::pattern |
            Token_Type::TILDE = [] { return Operator::Ones_Complement; },
        m::pattern | m::_ = [] { return Operator::None; });
}

} // namespace

Parser::Parser(std::string source)
    : lexer_(std::move(source))
{
    tokens_ = lexer_.tokenize();

    // One node per token is a reasonable upper bound, and keeps the arenas
    // from reallocating during the parse
    ast_.nodes.reserve(tokens_.size());
    ast_.metadata.reserve(tokens_.size());
    ast_.extra.reserve(tokens_.size());
}

/**
 * @brief Parse a whole source program in one call
 */
ast::AST Parser::parse(std::string source)
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
 * @brief Intern text and return a stable handle for equal text
 *
 * The text must point into the lexer's source buffer, which outlives the
 * parse. The lookup key is that view and not a view of the copy in
 * string_text, because string_text reallocates as it grows and would leave
 * every previously inserted key dangling. Equality and hashing are by
 * content, so two occurrences of the same name at different offsets still
 * collapse to one handle.
 */
ast::String_Index Parser::intern(std::string_view text)
{
    if (auto found = interned_.find(text); found != interned_.end())
        return found->second;

    auto offset = static_cast<std::uint32_t>(ast_.string_text.size());
    ast_.string_text.insert(ast_.string_text.end(), text.begin(), text.end());

    auto handle = static_cast<ast::String_Index>(ast_.strings.size());
    ast_.strings.push_back(
        ast::String_Entry{ offset, static_cast<std::uint32_t>(text.size()) });

    interned_.emplace(text, handle);
    return handle;
}

/**
 * @brief Append a node with the source span of `token`
 */
Node_Index Parser::add(Node node, Token const& token)
{
    auto index = static_cast<Node_Index>(ast_.nodes.size());
    ast_.nodes.push_back(node);
    ast_.metadata.push_back(
        ast::Meta{ static_cast<std::uint32_t>(token.start_pos),
            static_cast<std::uint32_t>(token.end_pos - token.start_pos),
            static_cast<std::uint32_t>(token.line),
            static_cast<std::uint32_t>(token.column) });
    return index;
}

/**
 * @brief Append a node spanning from `token` through the previous token
 */
Node_Index Parser::add_spanning(Node node, Token const& token)
{
    auto const& last = pos_ > 0 ? tokens_[pos_ - 1] : token;
    auto end = last.end_pos > token.start_pos ? last.end_pos : token.end_pos;

    auto index = static_cast<Node_Index>(ast_.nodes.size());
    ast_.nodes.push_back(node);
    ast_.metadata.push_back(
        ast::Meta{ static_cast<std::uint32_t>(token.start_pos),
            static_cast<std::uint32_t>(end - token.start_pos),
            static_cast<std::uint32_t>(token.line),
            static_cast<std::uint32_t>(token.column) });
    return index;
}

/**
 * @brief Move a scratch run of children into AST::extra
 *
 * Everything pushed onto scratch_ at or above `scratch_base` becomes one
 * contiguous run, and the scratch buffer is popped back down so nested
 * lists can reuse it.
 */
Span Parser::commit(std::size_t scratch_base)
{
    auto count = static_cast<std::uint32_t>(scratch_.size() - scratch_base);
    auto start = static_cast<std::uint32_t>(ast_.extra.size());
    ast_.extra.insert(
        ast_.extra.end(), scratch_.begin() + scratch_base, scratch_.end());
    scratch_.resize(scratch_base);
    return Span{ start, count };
}

/**
 * @brief Reserve `count` slots in AST::extra, returning the first
 */
std::uint32_t Parser::reserve_extra(std::uint32_t count)
{
    auto start = static_cast<std::uint32_t>(ast_.extra.size());
    ast_.extra.resize(start + count, null_node_index);
    return start;
}

/**
 * @brief Program root: function, vector, union definitions
 */
ast::AST Parser::parse_program()
{
    auto const& first = current();
    auto base = scratch_.size();
    while (!check(Token_Type::END_OF_FILE)) {
        scratch_.push_back(parse_definition());
    }
    auto span = commit(base);
    ast_.root = add_spanning(span_node(Type::Program, span), first);
    return std::move(ast_);
}

/**
 * @brief A top-level definition, told apart by whether '(' follows its name
 */
Node_Index Parser::parse_definition()
{
    if (check(Token_Type::KEYWORD_UNION))
        return parse_union_definition();
    auto const& name = expect(Token_Type::IDENTIFIER, "an identifier");
    if (check(Token_Type::LPAREN))
        return parse_function_definition(name);
    return parse_vector_definition(name);
}

/**
 * @brief Parse a function definition's parameter list and body
 *
 * Laid out in extra as [name, parameters, body].
 */
Node_Index Parser::parse_function_definition(Token const& name)
{
    auto name_node =
        add(string_node(Type::Identifier, intern(name.lexeme)), name);

    advance(); // '('
    auto parameters = parse_call_arguments_or_parameters();
    expect(Token_Type::RPAREN, "')'");
    auto body = parse_function_body();

    auto base = scratch_.size();
    scratch_.push_back(name_node);
    scratch_.push_back(parameters);
    scratch_.push_back(body);
    auto span = commit(base);

    return add_spanning(span_node(Type::Function_Definition, span), name);
}

/**
 * @brief Parse a vector definition's optional size and initial values
 *
 * Laid out in extra as [name, size or null, values].
 */
Node_Index Parser::parse_vector_definition(Token const& name)
{
    auto name_node =
        add(string_node(Type::Identifier, intern(name.lexeme)), name);

    auto size = parse_vector_size();

    auto values_base = scratch_.size();
    if (!check(Token_Type::SEMICOLON)) {
        scratch_.push_back(parse_vector_symbol());
        while (!check(Token_Type::SEMICOLON)) {
            match(Token_Type::COMMA); // optional
            scratch_.push_back(parse_vector_symbol());
        }
    }
    auto values_span = commit(values_base);
    expect(Token_Type::SEMICOLON, "';'");
    auto values =
        add_spanning(span_node(Type::Block_Statement, values_span), name);

    auto base = scratch_.size();
    scratch_.push_back(name_node);
    scratch_.push_back(size);
    scratch_.push_back(values);
    auto span = commit(base);

    return add_spanning(span_node(Type::Vector_Definition, span), name);
}

/**
 * @brief Parse a tagged union definition
 *
 * Laid out in extra as [name, entry...], each entry an Assignment_Expression
 * pairing the member name with its tag.
 */
Node_Index Parser::parse_union_definition()
{
    auto const& keyword = current();
    advance(); // 'union'

    auto const& name = expect(Token_Type::IDENTIFIER, "an identifier");
    auto name_node =
        add(string_node(Type::Identifier, intern(name.lexeme)), name);

    expect(Token_Type::LBRACE, "'{'");

    auto base = scratch_.size();
    scratch_.push_back(name_node);
    while (!check(Token_Type::RBRACE)) {
        auto const& member = expect(Token_Type::IDENTIFIER, "an identifier");
        auto member_node =
            add(string_node(Type::Identifier, intern(member.lexeme)), member);
        auto tag = parse_rvalue();
        match(Token_Type::COMMA); // optional
        scratch_.push_back(add_spanning(binary_node(Type::Assignment_Expression,
                                            Operator::Assign,
                                            member_node,
                                            tag),
            member));
    }
    auto span = commit(base);

    expect(Token_Type::RBRACE, "'}'");
    expect(Token_Type::SEMICOLON, "';'");

    return add_spanning(span_node(Type::Union_Definition, span), keyword);
}

/**
 * @brief Parse a parenthesized rvalue list, shared by calls and definitions
 */
Node_Index Parser::parse_call_arguments_or_parameters()
{
    auto const& first = current();
    auto base = scratch_.size();
    if (!check(Token_Type::RPAREN)) {
        scratch_.push_back(parse_rvalue());
        while (match(Token_Type::COMMA)) {
            scratch_.push_back(parse_rvalue());
        }
    }
    auto span = commit(base);
    return add_spanning(span_node(Type::Block_Statement, span), first);
}

/**
 * @brief Parse one vector initial value: either a constant or a bare name
 */
Node_Index Parser::parse_vector_symbol()
{
    if (check(Token_Type::IDENTIFIER)) {
        auto const& name = advance();
        return add(string_node(Type::Identifier, intern(name.lexeme)), name);
    }
    return parse_constant();
}

/**
 * @brief Parse a vector's optional "[" constant? "]" size
 */
Node_Index Parser::parse_vector_size()
{
    if (!check(Token_Type::LBRACKET))
        return null_node_index;
    advance(); // '['
    if (check(Token_Type::RBRACKET)) {
        advance();
        return null_node_index;
    }
    auto size = parse_constant();
    expect(Token_Type::RBRACKET, "']'");
    return size;
}

/**
 * @brief Parse a function body: statement* followed by an optional return
 */
Node_Index Parser::parse_function_body()
{
    auto const& brace = current();
    expect(Token_Type::LBRACE, "'{'");

    auto base = scratch_.size();
    while (!check(Token_Type::RBRACE) and !check(Token_Type::KEYWORD_RETURN)) {
        scratch_.push_back(parse_statement(false));
    }
    if (check(Token_Type::KEYWORD_RETURN)) {
        scratch_.push_back(parse_return_statement());
    }
    auto span = commit(base);

    expect(Token_Type::RBRACE, "'}'");
    return add_spanning(span_node(Type::Block_Statement, span), brace);
}

/**
 * @brief Whether the current token starts a keyword statement
 */
bool Parser::at_statement_keyword() const
{
    return m::match(current().type)(
        m::pattern | m::or_(Token_Type::KEYWORD_AUTO,
                         Token_Type::KEYWORD_EXTRN,
                         Token_Type::KEYWORD_IF,
                         Token_Type::KEYWORD_WHILE,
                         Token_Type::KEYWORD_SWITCH,
                         Token_Type::KEYWORD_UNION,
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
    return check(Token_Type::IDENTIFIER) and check_ahead(1, Token_Type::COLON);
}

/**
 * @brief Visit a statement by its leading token
 */
Node_Index Parser::parse_statement(bool allow_block_and_return)
{
    if (allow_block_and_return and check(Token_Type::LBRACE))
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
Node_Index Parser::parse_block_statement()
{
    auto const& brace = current();
    expect(Token_Type::LBRACE, "'{'");

    auto base = scratch_.size();
    while (!check(Token_Type::RBRACE)) {
        scratch_.push_back(parse_statement(true));
    }
    auto span = commit(base);

    expect(Token_Type::RBRACE, "'}'");
    return add_spanning(span_node(Type::Block_Statement, span), brace);
}

/**
 * @brief Parse a label: NAME ":"
 */
Node_Index Parser::parse_label_statement()
{
    auto const& name = current();
    advance(); // NAME
    expect(Token_Type::COLON, "':'");
    return add_spanning(
        string_node(Type::Label_Statement, intern(name.lexeme)), name);
}

/**
 * @brief Parse "auto" lvalue ("," lvalue)* ";"
 */
Node_Index Parser::parse_auto_statement()
{
    auto const& keyword = current();
    advance(); // 'auto'

    auto base = scratch_.size();
    scratch_.push_back(parse_lvalue());
    while (match(Token_Type::COMMA)) {
        scratch_.push_back(parse_lvalue());
    }
    auto span = commit(base);

    expect(Token_Type::SEMICOLON, "';'");
    return add_spanning(span_node(Type::Auto_Statement, span), keyword);
}

/**
 * @brief Parse "extrn" NAME ("," NAME)* ";"
 */
Node_Index Parser::parse_extrn_statement()
{
    auto const& keyword = current();
    advance(); // 'extrn'

    auto base = scratch_.size();
    do {
        auto const& name = expect(Token_Type::IDENTIFIER, "an identifier");
        scratch_.push_back(
            add(string_node(Type::Identifier, intern(name.lexeme)), name));
    } while (match(Token_Type::COMMA));
    auto span = commit(base);

    expect(Token_Type::SEMICOLON, "';'");
    return add_spanning(span_node(Type::Extrn_Statement, span), keyword);
}

/**
 * @brief Parse "case" constant ":" statement*
 */
Node_Index Parser::parse_case_statement()
{
    auto const& keyword = current();
    advance(); // 'case'
    auto value = parse_constant();
    expect(Token_Type::COLON, "':'");

    auto base = scratch_.size();
    while (!check(Token_Type::KEYWORD_CASE) and !check(Token_Type::RBRACE)) {
        scratch_.push_back(parse_statement(true));
    }
    auto span = commit(base);
    auto body = add_spanning(span_node(Type::Block_Statement, span), keyword);

    return add_spanning(
        binary_node(Type::Case_Statement, Operator::None, value, body),
        keyword);
}

/**
 * @brief Parse "goto" NAME ";"
 */
Node_Index Parser::parse_goto_statement()
{
    auto const& keyword = current();
    advance(); // 'goto'
    auto const& name = expect(Token_Type::IDENTIFIER, "an identifier");
    expect(Token_Type::SEMICOLON, "';'");
    return add_spanning(
        string_node(Type::Goto_Statement, intern(name.lexeme)), keyword);
}

/**
 * @brief Parse "if" "(" rvalue ")" statement ["else" statement]
 *
 * Laid out in extra as [condition, then, else or null].
 */
Node_Index Parser::parse_if_statement()
{
    auto const& keyword = current();
    advance(); // 'if'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");

    auto then_branch = parse_statement(true);
    auto else_branch = match(Token_Type::KEYWORD_ELSE) ? parse_statement(true)
                                                       : null_node_index;

    auto base = scratch_.size();
    scratch_.push_back(condition);
    scratch_.push_back(then_branch);
    scratch_.push_back(else_branch);
    auto span = commit(base);

    return add_spanning(span_node(Type::If_Statement, span), keyword);
}

/**
 * @brief Parse "return" ["(" rvalue ")"] ";"
 */
Node_Index Parser::parse_return_statement()
{
    auto const& keyword = current();
    advance(); // 'return'
    auto value = null_node_index;
    if (match(Token_Type::LPAREN)) {
        value = parse_rvalue();
        expect(Token_Type::RPAREN, "')'");
    }
    expect(Token_Type::SEMICOLON, "';'");
    return add_spanning(
        unary_node(Type::Return_Statement, Operator::None, value), keyword);
}

/**
 * @brief Whether another expression can start here within a rvalue_statement
 */
bool Parser::at_expression_start() const
{
    return !at_statement_keyword() and !at_label() and
           !check(Token_Type::LBRACE) and !check(Token_Type::RBRACE) and
           !check(Token_Type::END_OF_FILE);
}

/**
 * @brief Parse one ";" or "rvalue ;", the unit rvalue_statement repeats
 *
 * A bare ";" yields an empty span and not a null child, so consumers
 * can treat every element of a statement list the same way.
 */
Node_Index Parser::parse_expression()
{
    auto const& first = current();
    auto base = scratch_.size();
    if (!match(Token_Type::SEMICOLON)) {
        scratch_.push_back(parse_rvalue());
        expect(Token_Type::SEMICOLON, "';'");
    }
    auto span = commit(base);
    return add_spanning(span_node(Type::Expression_Statement, span), first);
}

/**
 * @brief Parse expression+, greedily absorbing consecutive ";" statements
 */
Node_Index Parser::parse_rvalue_statement()
{
    auto const& first = current();
    auto base = scratch_.size();
    scratch_.push_back(parse_expression());
    while (at_expression_start()) {
        scratch_.push_back(parse_expression());
    }
    auto span = commit(base);
    return add_spanning(span_node(Type::Block_Statement, span), first);
}

/**
 * @brief Parse "while" "(" rvalue ")" statement
 */
Node_Index Parser::parse_while_statement()
{
    auto const& keyword = current();
    advance(); // 'while'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");
    auto body = parse_statement(true);
    return add_spanning(
        binary_node(Type::While_Statement, Operator::None, condition, body),
        keyword);
}

/**
 * @brief Parse "switch" "(" rvalue ")" "{" case_statement* "}"
 */
Node_Index Parser::parse_switch_statement()
{
    auto const& keyword = current();
    advance(); // 'switch'
    expect(Token_Type::LPAREN, "'('");
    auto condition = parse_rvalue();
    expect(Token_Type::RPAREN, "')'");
    expect(Token_Type::LBRACE, "'{'");

    auto base = scratch_.size();
    while (check(Token_Type::KEYWORD_CASE)) {
        scratch_.push_back(parse_case_statement());
    }
    auto span = commit(base);
    auto cases = add_spanning(span_node(Type::Block_Statement, span), keyword);

    expect(Token_Type::RBRACE, "'}'");
    return add_spanning(
        binary_node(Type::Switch_Statement, Operator::None, condition, cases),
        keyword);
}

/**
 * @brief Parse "break" ";"
 */
Node_Index Parser::parse_break_statement()
{
    auto const& keyword = current();
    advance(); // 'break'
    expect(Token_Type::SEMICOLON, "';'");
    return add_spanning(
        unary_node(Type::Break_Statement, Operator::None, null_node_index),
        keyword);
}

/**
 * @brief Whether the current token is one of the flat "binary" operators
 */
bool Parser::at_binary_operator() const
{
    return binary_operator_of(current().type) != Operator::None;
}

/**
 * @brief Whether a node can appear on the left of an assignment
 */
bool Parser::is_lvalue_shaped(Node_Index index) const
{
    if (index == null_node_index)
        return false;
    return m::match(ast_.nodes[index].type)(
        m::pattern | m::or_(Type::Identifier,
                         Type::Indirect_Identifier,
                         Type::Vector_Identifier) = [] { return true; },
        m::pattern | m::_ = [] { return false; });
}

/**
 * @brief Parse an rvalue
 *
 * Operators are emitted in source order without precedence climbing: a
 * chain becomes a right-leaning spine that the HIR's shunting-yard pass
 * reshapes. Nodes need not be right-associative once that
 * pass has run, so this stays a flat syntactic record of what was written.
 */
Node_Index Parser::parse_rvalue()
{
    auto const& first = current();
    auto left = parse_rvalue_primary();

    if (check(Token_Type::ASSIGN) and is_lvalue_shaped(left)) {
        advance();
        auto right = parse_rvalue();
        return add_spanning(
            binary_node(
                Type::Assignment_Expression, Operator::Assign, left, right),
            first);
    }

    if (check(Token_Type::QUESTION)) {
        advance();
        auto then_value = parse_rvalue();
        expect(Token_Type::COLON, "':'");
        auto else_value = parse_rvalue();

        auto base = scratch_.size();
        scratch_.push_back(left);
        scratch_.push_back(then_value);
        scratch_.push_back(else_value);
        auto span = commit(base);

        return add_spanning(span_node(Type::Ternary_Expression, span), first);
    }

    if (auto op = binary_operator_of(current().type); op != Operator::None) {
        advance();
        auto right = parse_rvalue();
        return add_spanning(
            binary_node(Type::Binary_Expression, op, left, right), first);
    }

    return left;
}

/**
 * @brief Parse the '&' / unary_modifier prefixed forms, else fall through
 */
Node_Index Parser::parse_rvalue_primary()
{
    auto const& first = current();

    if (check(Token_Type::AMP)) {
        advance();
        auto operand = parse_lvalue();
        return add_spanning(
            unary_node(
                Type::Address_Of_Expression, Operator::Address_Of, operand),
            first);
    }

    if (auto op = unary_operator_of(current().type); op != Operator::None) {
        advance();
        auto operand = parse_unary_operand();
        return add_spanning(
            unary_node(Type::Unary_Expression, op, operand), first);
    }

    return parse_unary_operand();
}

/**
 * @brief Parse the restricted unary_operand grammar
 */
Node_Index Parser::parse_unary_operand()
{
    auto const& first = current();

    return m::match(current().type)(
        m::pattern | m::or_(Token_Type::INC, Token_Type::DEC) =
            [&] {
                auto op =
                    check(Token_Type::INC) ? Operator::Inc : Operator::Dec;
                advance();
                auto operand = parse_lvalue();
                return add_spanning(
                    unary_node(Type::Pre_Inc_Dec_Expression, op, operand),
                    first);
            },
        m::pattern | Token_Type::LPAREN =
            [&] {
                advance();
                auto inner = parse_rvalue();
                expect(Token_Type::RPAREN, "')'");
                return add_spanning(
                    unary_node(
                        Type::Evaluated_Expression, Operator::None, inner),
                    first);
            },
        m::pattern |
            m::or_(Token_Type::INTEGER,
                Token_Type::FLOAT,
                Token_Type::CHAR_LITERAL,
                Token_Type::STRING_LITERAL,
                Token_Type::BOOL_LITERAL) = [&] { return parse_constant(); },
        m::pattern | m::or_(Token_Type::IDENTIFIER, Token_Type::STAR) =
            [&] {
                auto lvalue = parse_lvalue();
                if (match(Token_Type::LPAREN)) {
                    auto arguments = parse_call_arguments_or_parameters();
                    expect(Token_Type::RPAREN, "')'");
                    return add_spanning(binary_node(Type::Function_Expression,
                                            Operator::None,
                                            lvalue,
                                            arguments),
                        first);
                }
                if (check(Token_Type::INC) or check(Token_Type::DEC)) {
                    auto op =
                        check(Token_Type::INC) ? Operator::Inc : Operator::Dec;
                    advance();
                    return add_spanning(
                        unary_node(Type::Post_Inc_Dec_Expression, op, lvalue),
                        first);
                }
                return lvalue;
            },
        m::pattern | m::_ = [&]() -> Node_Index {
            error(fmt::format(
                "expected an expression, found '{}'", current().lexeme));
            return null_node_index;
        });
}

/**
 * @brief Parse "*" rvalue | NAME, with any number of trailing "[" rvalue "]"
 */
Node_Index Parser::parse_lvalue()
{
    auto const& first = current();
    Node_Index node;

    if (check(Token_Type::STAR)) {
        advance();
        auto operand = parse_rvalue();
        node = add_spanning(
            unary_node(
                Type::Indirect_Identifier, Operator::Indirection, operand),
            first);
    } else {
        auto const& name = expect(Token_Type::IDENTIFIER, "an lvalue");
        node = add(string_node(Type::Identifier, intern(name.lexeme)), name);
    }

    while (check(Token_Type::LBRACKET)) {
        advance();
        auto subscript = parse_rvalue();
        expect(Token_Type::RBRACKET, "']'");
        node = add_spanning(
            binary_node(
                Type::Vector_Identifier, Operator::None, node, subscript),
            first);
    }
    return node;
}

/**
 * @brief Parse a numeric, char, string, or boolean constant
 */
Node_Index Parser::parse_constant()
{
    auto const& first = current();

    return m::match(current().type)(
        m::pattern | Token_Type::FLOAT =
            [&] {
                // the trailing 'f' or 'F' suffix is not part of the value
                auto text =
                    current().lexeme.substr(0, current().lexeme.size() - 1);
                advance();
                Node node{};
                node.type = Type::Float_Literal;
                node.op = Operator::None;
                node.data.real = parse_real(text);
                return add(node, first);
            },
        m::pattern | Token_Type::INTEGER =
            [&] {
                auto text = current().lexeme;
                advance();
                if (check(Token_Type::DOT) and
                    check_ahead(1, Token_Type::INTEGER)) {
                    advance(); // '.'
                    auto fraction = current().lexeme;
                    advance();
                    // the two halves are adjacent in the source buffer, so
                    // the whole literal is one view over them
                    auto whole = std::string_view(text.data(),
                        static_cast<std::size_t>(
                            fraction.data() + fraction.size() - text.data()));
                    Node node{};
                    node.type = Type::Double_Literal;
                    node.op = Operator::None;
                    node.data.real = parse_real(whole);
                    return add_spanning(node, first);
                }
                Node node{};
                node.type = Type::Integer_Literal;
                node.op = Operator::None;
                node.data.integer = parse_integer(text);
                return add(node, first);
            },
        m::pattern | Token_Type::CHAR_LITERAL =
            [&] {
                auto text = current().lexeme;
                advance();
                auto inner = text.substr(1, text.size() - 2);
                // a lone whitespace character keeps its quotes, matching
                // the escaped whitespace constant in the grammar
                if (inner.size() == 1 and
                    std::isspace(static_cast<unsigned char>(inner[0])))
                    inner = text;
                return add(
                    string_node(Type::Char_Literal, intern(inner)), first);
            },
        m::pattern | Token_Type::STRING_LITERAL =
            [&] {
                auto text = current().lexeme;
                advance();
                return add(
                    string_node(Type::String_Literal, intern(text)), first);
            },
        m::pattern | Token_Type::BOOL_LITERAL =
            [&] {
                auto text = current().lexeme;
                advance();
                return add(
                    string_node(Type::Bool_Literal, intern(text)), first);
            },
        m::pattern | m::_ = [&]() -> Node_Index {
            error(fmt::format(
                "expected a constant, found '{}'", current().lexeme));
            return null_node_index;
        });
}

} // namespace credence::frontend
