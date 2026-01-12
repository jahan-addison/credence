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

#include <credence/expression.h>

#include <algorithm>            // for __find, find
#include <credence/error.h>     // for assert_equal_impl, credence_assert_equal
#include <credence/operators.h> // for Operator, BINARY_OPERATORS
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/util.h>      // for AST_Node, unescape_string
#include <credence/values.h>    // for make_lvalue, Expression, TYPE_LITERAL
#include <easyjson.h>           // for JSON, JSON_String
#include <fmt/format.h>         // for format
#include <map>                  // for map
#include <matchit.h>            // for pattern, PatternHelper, PatternPipable
#include <memory>               // for make_shared
#include <string>               // for basic_string, operator==, char_traits
#include <string_view>          // for basic_string_view
#include <utility>              // for make_pair, pair, move
#include <variant>              // for variant

/****************************************************************************
 *
 * Expressions - LL(1) parser
 *
 * Parses expression ast nodes and value category types from the frontend into
 * algebraic symbols for semantic analysis in the IR.
 *
 * Note that statements and non-expression nodes are omitted here.
 *
 *   B source:  x = 5 + 3 * 2
 *
 *   ast node:  {"node": "assignment",
 *               "left": {"name": "x"},
 *               "right": {"node": "binary_op", "op": "+", ...}}
 *
 *   Example:    Assignment(lvalue="x",
 *                        rvalue=BinaryOp(ADD,
 *                                       Literal(5),
 *                                       BinaryOp(MUL, ...)))
 *
 *****************************************************************************/

namespace credence {

namespace m = matchit;

/**
 * @brief Parse expression ast node into Expression struct type pointer
 */
Expression_Parser::Expression Expression_Parser::parse_from_node(
    Node const& node)
{

    auto expression = Expression{};
    auto node_type = node["node"].to_string();

    // pointer indirection assignment:
    // i.e. "*k = 10", "*e = *k++", etc.
    if (node.has_key("left") and
        node["left"]["node"].to_string() == "assignment_expression") {
        expression.value =
            std::make_shared<Expression>(from_assignment_expression_node(node));
        return expression;
    }

    m::match(node_type)(
        m::pattern | "constant_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "integer_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "float_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "bool_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "double_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "string_literal" =
            [&] { expression.value = from_constant_expression_node(node); },
        m::pattern | "lvalue" =
            [&] { expression.value = from_lvalue_expression_node(node); },
        m::pattern | "vector_lvalue" =
            [&] { expression.value = from_lvalue_expression_node(node); },
        m::pattern | "function_expression" =
            [&] {
                expression.value = std::make_shared<Expression>(
                    from_function_expression_node(node));
            },
        m::pattern | "evaluated_expression" =
            [&] {
                expression.value = std::make_shared<Expression>(
                    from_evaluated_expression_node(node));
            },
        m::pattern | "relation_expression" =
            [&] {
                expression.value = std::make_shared<Expression>(
                    from_relation_expression_node(node));
            },
        m::pattern | "ternary_expression" =
            [&] {
                expression.value = std::make_shared<Expression>(
                    from_ternary_expression_node(node));
            },
        m::pattern | "indirect_lvalue" =
            [&] { expression.value = from_lvalue_expression_node(node); },
        m::pattern | "assignment_expression" =
            [&] {
                expression.value = std::make_shared<Expression>(
                    from_assignment_expression_node(node));
            },
        m::pattern | m::_ =
            [&] {
                if (util::range_contains(node_type, unary_types)) {
                    expression.value = std::make_shared<Expression>(
                        from_unary_expression_node(node));
                } else {
                    credence_error(
                        fmt::format("Invalid AST node type `{}`", node_type));
                }
            });
    return expression;
}

/**
 * @brief Build expression from function call expression
 */
Expression_Parser::Expression Expression_Parser::from_function_expression_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "function_expression");
    credence_assert(node["right"].to_deque().size() >= 1);
    Expression expression{};
    Parameters parameters{};
    auto param_node = node["right"].to_deque();
    // if the size of parameters is 1 and its only
    // param is "null", (i.e. [null]) it's empty
    if (!param_node.empty() and not param_node.front().is_null())
        for (auto& param : param_node) {
            parameters.emplace_back(make_expression_pointer_from_ast(param));
        }
    auto lhs = from_lvalue_expression_node(node["left"]);
    expression.value = value::Expression::Function{ lhs, parameters };
    return expression;
}

/**
 * @brief An expression wrapped in parenthesis, pre-evaluated
 */
Expression_Parser::Expression Expression_Parser::from_evaluated_expression_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "evaluated_expression");
    Expression expression{};
    expression.value = make_expression_pointer_from_ast(node["root"]);
    return expression;
}

/**
 * @brief Ternary relation expression
 */
Expression_Parser::Expression Expression_Parser::from_ternary_expression_node(
    Node const& node)
{
    Expression expression{};
    Parameters blocks{};
    auto conditional = node["left"];
    auto ternary = node["right"];
    if (node["root"].to_deque().size() == 0)
        return expression;
    auto op = node["root"].to_deque().front().to_string();
    blocks.emplace_back(make_expression_pointer_from_ast(node["left"]));
    blocks.emplace_back(make_expression_pointer_from_ast(ternary["root"]));
    blocks.emplace_back(make_expression_pointer_from_ast(ternary["left"]));
    blocks.emplace_back(make_expression_pointer_from_ast(ternary["right"]));
    expression.value =
        std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    return expression;
}

/**
 * @brief Relation to sum type of operator and chain of expressions
 */
Expression_Parser::Expression Expression_Parser::from_relation_expression_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "relation_expression");
    Expression expression{};
    Parameters blocks{};
    if (node.has_key("right") and
        node["right"]["node"].to_string() == "ternary_expression") {
        expression = from_ternary_expression_node(node);
    } else {
        auto op = node["root"].to_deque().front().to_string();
        blocks.emplace_back(make_expression_pointer_from_ast(node["left"]));
        blocks.emplace_back(make_expression_pointer_from_ast(node["right"]));
        expression.value =
            std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    }
    return expression;
}

/**
 * @brief Unary operator expression to algebraic pair
 */
Expression_Parser::Expression Expression_Parser::from_unary_expression_node(
    Node const& node)
{
    using namespace type;
    auto unary_type = node["node"].to_string();

    credence_assert_message(util::range_contains(unary_type, unary_types),
        fmt::format("Invalid unary expression type `{}`", unary_type));

    Expression expression{};
    std::map<std::string, Operator> const other_unary = {
        { "!", Operator::U_NOT             },
        { "~", Operator::U_ONES_COMPLEMENT },
        { "*", Operator::U_INDIRECTION     },
        { "-", Operator::U_MINUS           },
        { "+", Operator::U_PLUS            },
    };

    if (node["root"].JSON_type() != util::AST_Node::Class::Array)
        return expression;

    credence_assert(node["root"].to_deque().size() >= 1);

    auto op = node["root"].to_deque().front().to_string();

    m::match(unary_type)(
        m::pattern | "pre_inc_dec_expression" =
            [&] {
                if (op == "++") {
                    auto rhs = std::make_shared<Expression>(
                        parse_from_node(node["left"]));
                    expression.value = std::make_pair(Operator::PRE_INC, rhs);

                } else if (op == "--") {
                    auto rhs = std::make_shared<Expression>(
                        parse_from_node(node["left"]));
                    expression.value = std::make_pair(Operator::PRE_DEC, rhs);
                }
            },
        m::pattern | "post_inc_dec_expression" =
            [&] {
                if (op == "++") {
                    auto rhs = std::make_shared<Expression>(
                        parse_from_node(node["right"]));
                    expression.value = std::make_pair(Operator::POST_INC, rhs);

                } else if (op == "--") {
                    auto rhs = std::make_shared<Expression>(
                        parse_from_node(node["right"]));
                    expression.value =
                        std::make_pair(Operator::POST_DEC, std::move(rhs));
                }
            },
        m::pattern | "address_of_expression" =
            [&] {
                credence_assert_equal(op, "&");
                auto rhs = make_expression_pointer_from_ast(node["left"]);
                expression.value = std::make_pair(Operator::U_ADDR_OF, rhs);
            },
        m::pattern | m::_ =
            [&] {
                auto rhs = make_expression_pointer_from_ast(node["left"]);
                expression.value = std::make_pair(other_unary.at(op), rhs);
            });
    return expression;
}

/**
 * @brief Parse assignment expression into pairs of LHS and RHS
 */
Expression_Parser::Expression
Expression_Parser::from_assignment_expression_node(Node const& node)
{
    if (node["left"]["node"].to_string() == "assignment_expression") {
        // pointer indirection assignment:
        // i.e. "*k = 10", "*e = *k++", etc.
        auto indirect_node = util::AST::object();
        indirect_node["node"] = "indirect_lvalue";
        indirect_node["left"] = node["left"]["left"];
        indirect_node["root"][0] = "*";
        auto left_child_node = indirect_node;
        auto right_child_node = node["left"]["right"];
        if (!is_symbol(left_child_node["left"]))
            expression_parser_error("identifier of assignment not "
                                    "declared with 'auto' or 'extrn'",
                left_child_node["left"]["root"].to_string());
        auto lhs = from_lvalue_expression_node(left_child_node);
        auto rhs = make_expression_pointer_from_ast(right_child_node);
        Expression expression = Expression{};
        expression.value = make_pair(lhs, rhs);
        return expression;
    } else {
        credence_assert_equal(
            node["node"].to_string(), "assignment_expression");

        credence_assert(node.has_key("left"));
        credence_assert(node.has_key("right"));

        auto left_child_node = node["left"];
        auto right_child_node = node["right"];
        if (!is_symbol(left_child_node))
            expression_parser_error("identifier of assignment not "
                                    "declared with 'auto' or 'extrn'",
                left_child_node["root"].to_string());

        auto lhs = from_lvalue_expression_node(left_child_node);
        auto rhs = make_expression_pointer_from_ast(right_child_node);
        Expression expression = Expression{};
        expression.value = make_pair(lhs, rhs);

        return expression;
    }
}

/**
 * @brief Parse lvalue expression data types
 */
Expression_Parser::Expression::LValue
Expression_Parser::from_lvalue_expression_node(Node const& node)
{
    auto constant_type = node["node"].to_string();
    if (!symbols_.is_defined(node["root"].to_string()) and
        !symbols_.is_defined(node["left"]["root"].to_string())) {
        std::string name{};
        if (node.has_key("left"))
            name = node["left"]["root"].to_string();
        if (node.has_key("right"))
            name = node["right"]["root"].to_string();
        else
            name = node["root"].to_string();
        if (internal_symbols_.has_key(name)) {
            if (internal_symbols_.at(name)["type"].to_string() !=
                "function_definition")
                expression_parser_error("identifier does not exist in "
                                        "current scope, did you mean "
                                        "to use extrn?",
                    name);
            else
                symbols_.set_symbol_by_name(name, value::WORD_LITERAL);
        }
    }
    Expression::LValue lvalue{};
    m::match(node["node"].to_string())(
        m::pattern | "lvalue" =
            [&] {
                auto name = node["root"].to_string();
                if (symbols_.is_pointer(name))
                    lvalue = value::make_lvalue(name);
                else
                    lvalue = value::make_lvalue(
                        name, symbols_.get_symbol_by_name(name));
            },
        m::pattern | "vector_lvalue" =
            [&] {
                auto offset_value = node["left"]["root"];
                if (offset_value.JSON_type() ==
                    util::AST_Node::Class::Integral) {
                    lvalue = value::make_lvalue(fmt::format("{}[{}]",
                        node["root"].to_string(),
                        offset_value.to_int()));
                } else
                    lvalue = value::make_lvalue(fmt::format("{}[{}]",
                        node["root"].to_string(),
                        offset_value.to_string()));
            },
        m::pattern | "indirect_lvalue" =
            [&] {
                if (node["left"].has_key("left")) {
                    lvalue = value::make_lvalue(fmt::format(
                        "*{}", node["left"]["left"]["root"].to_string()));
                } else
                    lvalue = value::make_lvalue(
                        fmt::format("*{}", node["left"]["root"].to_string()));
            });
    return lvalue;
}

/**
 * @brief Parse constant expression data types
 */
Expression_Parser::Literal Expression_Parser::from_constant_expression_node(
    Node const& node)
{
    return m::match(node["node"].to_string())(
        m::pattern | "constant_literal" =
            [&] { return from_constant_literal_node(node); },
        m::pattern |
            "integer_literal" = [&] { return from_integer_literal_node(node); },
        m::pattern |
            "float_literal" = [&] { return from_float_literal_node(node); },
        m::pattern |
            "double_literal" = [&] { return from_double_literal_node(node); },
        m::pattern |
            "bool_literal" = [&] { return from_bool_literal_node(node); },
        m::pattern |
            "string_literal" = [&] { return from_string_literal_node(node); }

    );
}

/**
 * @brief Parse lvalue to pointer data type
 */
Expression_Parser::Literal Expression_Parser::from_indirect_identifier_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "indirect_lvalue");
    credence_assert(node.has_key("left"));
    if (!is_symbol(node["left"]))
        expression_parser_error("indirect identifier not defined, did you "
                                "forget to declare with "
                                "auto or extrn?",
            node["root"].to_string());

    return symbols_.get_symbol_by_name(node["left"]["root"].to_string());
}

/**
 * @brief Parse fixed-size vector (array) lvalue
 */
Expression_Parser::Literal Expression_Parser::from_vector_idenfitier_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "vector_lvalue");

    if (!is_symbol(node))
        expression_parser_error(
            "vector not defined, did you forget to declare with "
            "auto or extrn? No symbol found",
            node["root"].to_string());

    return symbols_.get_symbol_by_name(node["root"].to_string());
}

/**
 * @brief Parse integer literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_integer_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "integer_literal");
    return { static_cast<int>(node["root"].to_int()),
        value::TYPE_LITERAL.at("int") };
}

/**
 * @brief Parse float literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_float_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "float_literal");
    return { static_cast<float>(node["root"].to_float()),
        value::TYPE_LITERAL.at("float") };
}

/**
 * @brief Parse double literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_double_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "double_literal");
    return { static_cast<double>(node["root"].to_float()),
        value::TYPE_LITERAL.at("double") };
}

/**
 * @brief Parse bool literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_bool_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "bool_literal");
    return { node["root"].to_string() == "true" ? 1 : 0,
        value::TYPE_LITERAL.at("bool") };
}

/**
 * @brief Parse string literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_string_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "string_literal");
    auto string_literal = util::unescape_string(node["root"].to_string());
    return {
        string_literal.substr(1, string_literal.size() - 2),
        { "string", string_literal.substr(1, string_literal.size() - 2).size() }
    };
}

/**
 * @brief Parse constant literal node into symbols
 */
Expression_Parser::Literal Expression_Parser::from_constant_literal_node(
    Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "constant_literal");
    return { static_cast<char>(node["root"].to_string()[0]),
        value::TYPE_LITERAL.at("char") };
}

/**
 * @brief Raise error expressing parsing error
 */
inline void Expression_Parser::expression_parser_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location)
{
    credence_compile_error(location, message, symbol, internal_symbols_);
}

} // namespace credence