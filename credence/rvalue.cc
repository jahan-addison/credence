/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <credence/rvalue.h>

#include <credence/assert.h>    // for assert_equal_impl, CREDENCE_ASSERT_NODE
#include <credence/operators.h> // for Operator, BINARY_OPERATORS
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for RValue, LITERAL_TYPE, WORD_LITERAL
#include <credence/util.h>      // for unescape_string, AST_Node
#include <format>               // for format, format_string
#include <map>                  // for map
#include <mapbox/eternal.hpp>   // for element, map
#include <matchit.h>            // for pattern, PatternHelper, PatternPipable
#include <memory>               // for shared_ptr, allocator, make_shared
#include <simplejson.h>         // for JSON, JSON_String
#include <string>               // for basic_string, operator==, char_traits
#include <utility>              // for pair, make_pair, move
#include <variant>              // for variant

namespace credence {

namespace m = matchit;

/**
 * @brief Parse rvalue ast node into type::RValue struct type pointer
 */
type::RValue RValue_Parser::from_rvalue(Node const& node)
{

    auto rvalue = type::RValue{};
    auto rvalue_type = node["node"].to_string();

    m::match(rvalue_type)(
        // cppcheck-suppress syntaxError
        m::pattern | "constant_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        m::pattern | "number_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        m::pattern | "string_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        m::pattern |
            "lvalue" = [&] { rvalue.value = from_lvalue_expression(node); },
        m::pattern | "vector_lvalue" =
            [&] { rvalue.value = from_lvalue_expression(node); },
        m::pattern | "function_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_function_expression(node));
            },
        m::pattern | "evaluated_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_evaluated_expression(node));
            },
        m::pattern | "relation_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_relation_expression(node));
            },
        m::pattern | "ternary_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_ternary_expression(node));
            },
        m::pattern | "indirect_lvalue" =
            [&] { rvalue.value = from_lvalue_expression(node); },
        m::pattern | "assignment_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_assignment_expression(node));
            },
        m::pattern | m::_ =
            [&] {
                if (std::ranges::find(unary_types_, rvalue_type) !=
                    unary_types_.end()) {
                    rvalue.value = std::make_shared<type::RValue>(
                        from_unary_expression(node));
                } else {
                    credence_error(
                        std::format("Invalid rvalue type `{}`", rvalue_type));
                }
            });
    return rvalue;
}

/**
 * @brief Build rvalue from function call expression
 */
type::RValue RValue_Parser::from_function_expression(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "function_expression");
    CREDENCE_ASSERT(node["right"].to_deque().size() >= 1);
    type::RValue rvalue{};
    auto param_node = node["right"].to_deque();
    Parameters parameters{};
    // if the size of parameters is 1 and its only
    // param is "null", (i.e. [null]) it's empty
    if (!param_node.empty() and not param_node.front().is_null())
        for (auto& param : param_node) {
            parameters.emplace_back(shared_ptr_from_rvalue(param));
        }
    auto lhs = from_lvalue_expression(node["left"]);
    rvalue.value = std::make_pair(lhs, parameters);
    return rvalue;
}

/**
 * @brief An rvalue wrapped in parenthesis, pre-evaluated
 */
type::RValue RValue_Parser::from_evaluated_expression(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "evaluated_expression");
    type::RValue rvalue{};
    rvalue.value = shared_ptr_from_rvalue(node["root"]);
    return rvalue;
}

/**
 * @brief Ternary relation rvalue
 */
type::RValue RValue_Parser::from_ternary_expression(Node const& node)
{
    type::RValue rvalue{};
    Parameters blocks{};
    auto conditional = node["left"];
    auto ternary = node["right"];
    if (node["root"].to_deque().size() == 0)
        return rvalue;
    auto op = node["root"].to_deque().front().to_string();
    blocks.emplace_back(shared_ptr_from_rvalue(node["left"]));
    blocks.emplace_back(shared_ptr_from_rvalue(ternary["root"]));
    blocks.emplace_back(shared_ptr_from_rvalue(ternary["left"]));
    blocks.emplace_back(shared_ptr_from_rvalue(ternary["right"]));
    rvalue.value =
        std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    return rvalue;
}

/**
 * @brief Relation to sum type of operator and chain of rvalues
 */
type::RValue RValue_Parser::from_relation_expression(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "relation_expression");
    type::RValue rvalue{};
    Parameters blocks{};
    if (node.has_key("right") and
        node["right"]["node"].to_string() == "ternary_expression") {
        rvalue = from_ternary_expression(node);
    } else {
        auto op = node["root"].to_deque().front().to_string();
        blocks.emplace_back(shared_ptr_from_rvalue(node["left"]));
        blocks.emplace_back(shared_ptr_from_rvalue(node["right"]));
        rvalue.value =
            std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    }
    return rvalue;
}

/**
 * @brief Unary operator expression to algebraic pair
 */
type::RValue RValue_Parser::from_unary_expression(Node const& node)
{
    using namespace type;
    auto unary_type = node["node"].to_string();

    CREDENCE_ASSERT_MESSAGE(
        std::ranges::find(unary_types_, unary_type) != unary_types_.end(),
        std::format("Invalid unary expression type `{}`", unary_type));

    RValue rvalue{};
    std::map<std::string, Operator> const other_unary = {
        { "!", Operator::U_NOT },         { "~", Operator::U_ONES_COMPLEMENT },
        { "*", Operator::U_INDIRECTION }, { "-", Operator::U_MINUS },
        { "+", Operator::U_PLUS },
    };

    if (node["root"].JSON_type() != util::AST_Node::Class::Array)
        return rvalue;

    CREDENCE_ASSERT(node["root"].to_deque().size() >= 1);

    auto op = node["root"].to_deque().front().to_string();

    m::match(unary_type)(
        m::pattern | "pre_inc_dec_expression" =
            [&] {
                if (op == "++") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_INC, rhs);

                } else if (op == "--") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_DEC, rhs);
                }
            },
        m::pattern | "post_inc_dec_expression" =
            [&] {
                if (op == "++") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["right"]));
                    rvalue.value = std::make_pair(Operator::POST_INC, rhs);

                } else if (op == "--") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["right"]));
                    rvalue.value =
                        std::make_pair(Operator::POST_DEC, std::move(rhs));
                }
            },
        m::pattern | "address_of_expression" =
            [&] {
                CREDENCE_ASSERT_EQUAL(op, "&");
                auto rhs = shared_ptr_from_rvalue(node["left"]);
                rvalue.value = std::make_pair(Operator::U_ADDR_OF, rhs);
            },
        m::pattern | m::_ =
            [&] {
                auto rhs = shared_ptr_from_rvalue(node["left"]);
                rvalue.value = std::make_pair(other_unary.at(op), rhs);
            });
    return rvalue;
}

/**
 * @brief Parse assignment expression into pairs of LHS and RHS
 */
type::RValue RValue_Parser::from_assignment_expression(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "assignment_expression");
    CREDENCE_ASSERT(node.has_key("left"));
    CREDENCE_ASSERT(node.has_key("right"));

    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    if (!is_symbol(left_child_node))
        credence_runtime_error(
            "identifier of assignment not declared with 'auto' or 'extrn'",
            left_child_node["root"].to_string(),
            internal_symbols_);

    auto lhs = from_lvalue_expression(left_child_node);
    auto rhs = shared_ptr_from_rvalue(right_child_node);
    type::RValue rvalue = type::RValue{};
    rvalue.value = std::make_pair(lhs, rhs);

    return rvalue;
}

/**
 * @brief Parse lvalue expression data types
 */
type::RValue::LValue RValue_Parser::from_lvalue_expression(Node const& node)
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
        // hoist function definitions from the first pass to the memory table
        if (internal_symbols_.has_key(name)) {
            if (internal_symbols_.at(name)["type"].to_string() !=
                "function_definition")
                credence_runtime_error(
                    "identifier not defined, did you forget to declare with "
                    "auto or extrn? No symbol found",
                    name,
                    internal_symbols_);
            else
                symbols_.set_symbol_by_name(name, type::WORD_LITERAL);
        }
    }
    type::RValue::LValue lvalue{};
    m::match(node["node"].to_string())(
        m::pattern | "lvalue" =
            [&] {
                lvalue = type::RValue::make_lvalue(
                    node["root"].to_string(),
                    symbols_.get_symbol_by_name(node["root"].to_string()));
            },
        m::pattern | "vector_lvalue" =
            [&] {
                lvalue = type::RValue::make_lvalue(
                    node["root"].to_string(),
                    symbols_.get_symbol_by_name(node["root"].to_string()));
            },
        m::pattern | "indirect_lvalue" =
            [&] {
                auto indirect_lvalue = node["left"]["root"].to_string();
                lvalue = type::RValue::make_lvalue(
                    std::format("*{}", node["left"]["root"].to_string()));
            });
    return lvalue;
}

/**
 * @brief Parse constant expression data types
 */
type::RValue::Value RValue_Parser::from_constant_expression(Node const& node)
{
    return m::match(node["node"].to_string())(
        m::pattern |
            "constant_literal" = [&] { return from_constant_literal(node); },
        m::pattern |
            "number_literal" = [&] { return from_number_literal(node); },
        m::pattern |
            "string_literal" = [&] { return from_string_literal(node); });
}

/**
 * @brief Parse lvalue to pointer data type
 */
type::RValue::Value RValue_Parser::from_indirect_identifier(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "indirect_lvalue");
    CREDENCE_ASSERT(node.has_key("left"));
    if (!is_symbol(node["left"]))
        credence_runtime_error(
            "indirect identifier not defined, did you forget to declare with "
            "auto or extrn? No symbol found",
            node["root"].to_string(),
            internal_symbols_);

    return symbols_.get_symbol_by_name(node["left"]["root"].to_string());
}

/**
 * @brief Parse fixed-size vector (array) lvalue
 */
type::RValue::Value RValue_Parser::from_vector_idenfitier(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "vector_lvalue");

    if (!is_symbol(node))
        credence_runtime_error(
            "vector not defined, did you forget to declare with "
            "auto or extrn? No symbol found",
            node["root"].to_string(),
            internal_symbols_);

    return symbols_.get_symbol_by_name(node["root"].to_string());
}

/**
 * @brief Parse number literal node into symbols
 */
type::RValue::Value RValue_Parser::from_number_literal(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "number_literal");
    return { static_cast<int>(node["root"].to_int()),
             type::LITERAL_TYPE.at("int") };
}

/**
 * @brief Parse string literal node into symbols
 */
type::RValue::Value RValue_Parser::from_string_literal(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "string_literal");
    auto string_literal = util::unescape_string(node["root"].to_string());
    return { string_literal.substr(1, string_literal.size() - 2),
             { "string",
               string_literal.substr(1, string_literal.size() - 2).size() } };
}

/**
 * @brief Parse constant literal node into symbols
 */
type::RValue::Value RValue_Parser::from_constant_literal(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "constant_literal");
    return { static_cast<char>(node["root"].to_string()[0]),
             type::LITERAL_TYPE.at("char") };
}

} // namespace credence