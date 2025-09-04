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

#include <algorithm>            // for max, __any_of_fn, any_of
#include <cassert>              // for assert
#include <credence/json.h>      // for JSON
#include <credence/operators.h> // for Operator, BINARY_OPERATORS
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for RValue, Type_
#include <credence/util.h>      // for unescape_string
#include <deque>                // for deque, operator==, _Deque_iterator
#include <format>               // for format, format_string
#include <functional>           // for identity
#include <map>                  // for map
#include <matchit.h>            // for pattern, PatternHelper, PatternPipable
#include <memory>               // for shared_ptr, make_shared
#include <stdexcept>            // for runtime_error
#include <string>               // for allocator, basic_string, operator==
#include <utility>              // for pair, make_pair, move
#include <variant>              // for monostate, variant
#include <vector>               // for vector

namespace credence {

/**
 * @brief Throw program flow error
 */
void RValue_Parser::error(std::string_view message,
                          std::string_view symbol_name)
{
    auto symbol = symbol_name.data();
    if (internal_symbols_.hasKey(symbol)) {
        throw std::runtime_error(
            std::format("Runtime error :: \"{}\" {}\n\t"
                        "on line {} in column {} :: {}",
                        symbol,
                        message,
                        internal_symbols_[symbol]["line"].ToInt(),
                        internal_symbols_[symbol]["column"].ToInt(),
                        internal_symbols_[symbol]["end_column"].ToInt()));
    } else {
        throw std::runtime_error(
            std::format("Runtime error :: \"{}\" {}", symbol, message));
    }
}

/**
 * @brief Parse rvalue ast node into type::RValue struct type pointer
 */
type::RValue RValue_Parser::from_rvalue(Node& node)
{

    using namespace matchit;
    auto rvalue = type::RValue{};
    bool is_unary =
        std::ranges::any_of(unary_types_, [&](std::string const& str) {
            return str == node["node"].ToString();
        });
    match(node["node"].ToString())(
        pattern | "constant_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        pattern | "number_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        pattern | "string_literal" =
            [&] { rvalue.value = from_constant_expression(node); },
        pattern |
            "lvalue" = [&] { rvalue.value = from_lvalue_expression(node); },
        pattern | "vector_lvalue" =
            [&] { rvalue.value = from_lvalue_expression(node); },
        pattern | "function_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_function_expression(node));
            },
        pattern | "evaluated_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_evaluated_expression(node));
            },

        pattern | "relation_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_relation_expression(node));
            },
        pattern | "indirect_lvalue" =
            [&] { rvalue.value = from_lvalue_expression(node); },
        pattern | "assignment_expression" =
            [&] {
                rvalue.value = std::make_shared<type::RValue>(
                    from_assignment_expression(node));
            },
        pattern | _ =
            [&] {
                if (is_unary) {
                    rvalue.value = std::make_shared<type::RValue>(
                        from_unary_expression(node));
                } else {
                    rvalue.value = std::monostate();
                }
            });
    return rvalue;
}

/**
 * @brief Build rvalue from function call expression
 */
type::RValue RValue_Parser::from_function_expression(Node& node)
{
    type::RValue rvalue{};

    assert(node["node"].ToString().compare("function_expression") == 0);
    std::vector<type::RValue::RValue_Pointer> parameters{};

    for (auto& param : node["right"].ArrayRange()) {
        parameters.push_back(
            std::make_shared<type::RValue>(from_rvalue(param)));
    }
    auto lhs = from_lvalue_expression(node["left"]);
    rvalue.value = std::make_pair(lhs, std::move(parameters));
    return rvalue;
}

/**
 * @brief An rvalue wrapped in parenthesis, pre-evaluated
 */
type::RValue RValue_Parser::from_evaluated_expression(Node& node)
{
    type::RValue rvalue{};
    assert(node["node"].ToString().compare("evaluated_expression") == 0);
    rvalue.value = std::make_shared<type::RValue>(from_rvalue(node["root"]));
    return rvalue;
}

/**
 * @brief Relation to sum type of operator and chain of rvalues
 */
type::RValue RValue_Parser::from_relation_expression(Node& node)
{
    type::RValue rvalue{};
    assert(node["node"].ToString().compare("relation_expression") == 0);
    std::vector<type::RValue::RValue_Pointer> blocks{};
    if (node.hasKey("right") and
        node["right"]["node"].ToString() == "ternary_expression") {
        auto ternary = node["right"];
        auto op = node["root"].ArrayRange().get()->at(0).ToString();
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(node["left"])));
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(ternary["root"])));
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(ternary["left"])));
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(ternary["right"])));
        rvalue.value =
            std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    } else {
        auto op = node["root"].ArrayRange().get()->at(0).ToString();
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(node["left"])));
        blocks.push_back(
            std::make_shared<type::RValue>(from_rvalue(node["right"])));
        rvalue.value =
            std::make_pair(type::BINARY_OPERATORS.at(op), std::move(blocks));
    }
    return rvalue;
}

/**
 * @brief Unary operator expression to algebraic pair
 */
type::RValue RValue_Parser::from_unary_expression(Node& node)
{
    using namespace matchit;
    using namespace type;
    std::map<std::string, Operator> const other_unary = {
        { "!", Operator::U_NOT },
        { "~", Operator::U_ONES_COMPLEMENT },
        { "*", Operator::U_INDIRECTION },
        { "-", Operator::U_MINUS },

    };
    RValue rvalue{};
    match(node["node"].ToString())(
        pattern | "pre_inc_dec_expression" =
            [&] {
                if (node["root"].ArrayRange().get()->at(0).ToString() == "++") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_INC, rhs);

                } else if (node["root"].ArrayRange().get()->at(0).ToString() ==
                           "--") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_DEC, rhs);
                }
            },
        pattern | "post_inc_dec_expression" =
            [&] {
                if (node["root"].ArrayRange().get()->at(0).ToString() == "++") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["right"]));
                    rvalue.value = std::make_pair(Operator::POST_INC, rhs);

                } else if (node["root"].ArrayRange().get()->at(0).ToString() ==
                           "--") {
                    auto rhs = std::make_shared<type::RValue>(
                        from_rvalue(node["right"]));
                    rvalue.value =
                        std::make_pair(Operator::POST_DEC, std::move(rhs));
                }
            },
        pattern | "address_of_expression" =
            [&] {
                auto op = node["root"].ArrayRange().get()->at(0).ToString();
                assert(op.compare("&") == 0);
                auto rhs =
                    std::make_shared<type::RValue>(from_rvalue(node["left"]));
                rvalue.value = std::make_pair(Operator::U_ADDR_OF, rhs);
            },
        // otherwise:
        pattern | _ =
            [&] {
                auto op = node["root"].ArrayRange().get()->at(0).ToString();
                auto rhs =
                    std::make_shared<type::RValue>(from_rvalue(node["left"]));
                rvalue.value = std::make_pair(other_unary.at(op), rhs);
            });
    return rvalue;
}

/**
 * @brief Parse assignment expression into pairs of LHS and RHS
 */
type::RValue RValue_Parser::from_assignment_expression(Node& node)
{
    assert(node["node"].ToString().compare("assignment_expression") == 0);
    assert(node.hasKey("left"));
    assert(node.hasKey("right"));

    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    if (!is_symbol(left_child_node)) {
        error("identifier of assignment not declared with 'auto' or 'extern'",
              left_child_node["root"].ToString());
    }

    auto lhs = from_lvalue_expression(left_child_node);
    auto rhs = from_rvalue(right_child_node);
    type::RValue rvalue = type::RValue{};
    rvalue.value = std::make_pair(lhs, std::make_shared<type::RValue>(rhs));

    return rvalue;
}

/**
 * @brief Parse lvalue expression data types
 */
type::RValue::LValue RValue_Parser::from_lvalue_expression(Node& node)
{
    using namespace matchit;
    auto constant_type = node["node"].ToString();

    if (!symbols_.is_defined(node["root"].ToString()) and
        !symbols_.is_defined(node["left"]["root"].ToString())) {
        std::string name{};
        if (node.hasKey("left"))
            name = node["left"]["root"].ToString();
        if (node.hasKey("right"))
            name = node["right"]["root"].ToString();
        else
            name = node["root"].ToString();
        // hoist function definitions from the first pass to the memory table
        if (internal_symbols_.hasKey(name)) {
            if (internal_symbols_.at(name)["type"].ToString() !=
                "function_definition")
                error("identifier not defined, did you forget to declare with "
                      "auto or extern, or meant to call a function?",
                      name);
            else
                symbols_.set_symbol_by_name(
                    name, { "__WORD__", type::Type_["word"] });
        }
    }
    type::RValue::LValue lvalue{};
    match(node["node"].ToString())(
        pattern | "lvalue" =
            [&] {
                lvalue = std::make_pair(
                    node["root"].ToString(),
                    symbols_.get_symbol_by_name(node["root"].ToString()));
            },
        pattern | "vector_lvalue" =
            [&] {
                lvalue = std::make_pair(
                    node["root"].ToString(),
                    symbols_.get_symbol_by_name(node["root"].ToString()));
            },
        pattern | "indirect_lvalue" =
            [&] {
                lvalue = std::make_pair(node["left"]["root"].ToString(),
                                        symbols_.get_symbol_by_name(
                                            node["left"]["root"].ToString()));
            });
    return lvalue;
}

/**
 * @brief Parse constant expression data types
 */
type::RValue::Value RValue_Parser::from_constant_expression(Node& node)
{
    using namespace matchit;
    auto constant_type = node["node"].ToString();
    return match(constant_type)(
        pattern |
            "constant_literal" = [&] { return from_constant_literal(node); },
        pattern | "number_literal" = [&] { return from_number_literal(node); },
        pattern | "string_literal" = [&] { return from_string_literal(node); });
}

/**
 * @brief Parse lvalue to pointer data type
 */
type::RValue::Value RValue_Parser::from_indirect_identifier(Node& node)
{
    assert(node["node"].ToString().compare("indirect_lvalue") == 0);
    assert(node.hasKey("left"));
    if (!is_symbol(node["left"])) {
        error("indirect identifier not declared with 'auto' or 'extern'",
              node["root"].ToString());
    }
    return symbols_.get_symbol_by_name(node["left"]["root"].ToString());
}

/**
 * @brief Parse fixed-size vector (array) lvalue
 */
type::RValue::Value RValue_Parser::from_vector_idenfitier(Node& node)
{
    assert(node["node"].ToString().compare("vector_lvalue") == 0);

    if (!is_symbol(node)) {
        error("vector not declared with 'auto' or 'extern'",
              node["root"].ToString());
    }
    return symbols_.get_symbol_by_name(node["root"].ToString());
}

/**
 * @brief Parse number literal node into symbols
 */
type::RValue::Value RValue_Parser::from_number_literal(Node& node)
{
    assert(node["node"].ToString().compare("number_literal") == 0);
    return { static_cast<int>(node["root"].ToInt()), type::Type_["int"] };
}

/**
 * @brief Parse string literal node into symbols
 */
type::RValue::Value RValue_Parser::from_string_literal(Node& node)
{
    assert(node["node"].ToString().compare("string_literal") == 0);
    auto string_literal = util::unescape_string(node["root"].ToString());
    return { string_literal.substr(1, string_literal.size() - 2),
             { "string",
               string_literal.substr(1, string_literal.size() - 2).size() } };
}

/**
 * @brief Parse constant literal node into symbols
 */
type::RValue::Value RValue_Parser::from_constant_literal(Node& node)
{
    assert(node["node"].ToString().compare("constant_literal") == 0);
    return { static_cast<char>(node["root"].ToString()[0]),
             type::Type_["char"] };
}

} // namespace credence