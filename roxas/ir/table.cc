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

#include <algorithm> // for __any_of_fn, any_of
#include <cassert>   // for assert
#include <deque>     // for deque
#include <format>    // for format, format_string
#include <map>       // for allocator, map
#include <matchit.h> // for pattern, PatternHelper, PatternPipable
#include <memory>    // for shared_ptr, make_shared
#include <roxas/ir/table.h>
#include <roxas/json.h>      // for JSON
#include <roxas/operators.h> // for Operator
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for RValue, Type_, Byte
#include <stdexcept>         // for runtime_error
#include <string>            // for basic_string, operator==, operator<=>
#include <utility>           // for pair, make_pair, move
#include <variant>           // for monostate, variant
#include <vector>            // for vector

namespace roxas {

namespace ir {
using namespace roxas::type;
using namespace matchit;

/**
 * @brief Throw program flow error
 *
 * @param message
 * @param object
 */
void Table::error(std::string_view message, std::string_view symbol_name)
{
    auto symbol = symbol_name.data();
    if (internal_symbols_.hasKey(symbol)) {
        throw std::runtime_error(
            std::format("Parsing error :: \"{}\" {}\n\t"
                        "on line {} in column {} :: {}",
                        symbol,
                        message,
                        internal_symbols_[symbol]["line"].ToInt(),
                        internal_symbols_[symbol]["column"].ToInt(),
                        internal_symbols_[symbol]["end_column"].ToInt()));
    } else {
        throw std::runtime_error(
            std::format("Parsing error :: \"{}\" {}", symbol, message));
    }
}

/**
 * @brief Parse rvalues and temporaries into an algebraic type
 *
 * @param node
 * @return RValue
 */
RValue Table::from_rvalue(Node& node)
{
    RValue rvalue = RValue{};
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
                rvalue.value =
                    std::make_shared<RValue>(from_function_expression(node));
            },

        pattern | "relation_expression" =
            [&] {
                rvalue.value =
                    std::make_shared<RValue>(from_relation_expression(node));
            },
        pattern | "indirect_lvalue" =
            [&] { rvalue.value = from_lvalue_expression(node); },
        pattern | "assignment_expression" =
            [&] {
                rvalue.value =
                    std::make_shared<RValue>(from_assignment_expression(node));
            },
        pattern | _ =
            [&] {
                if (is_unary) {
                    rvalue.value =
                        std::make_shared<RValue>(from_unary_expression(node));
                } else {
                    rvalue.value = std::monostate();
                }
            });
    return rvalue;
}

RValue Table::from_function_expression(Node& node)
{
    RValue rvalue{};

    assert(node["node"].ToString().compare("function_expression") == 0);
    std::vector<RValue::RValue_Pointer> parameters{};

    for (auto& param : node["right"].ArrayRange()) {
        parameters.push_back(std::make_shared<RValue>(from_rvalue(param)));
    }
    auto lhs = from_lvalue_expression(node["left"]);
    rvalue.value = std::make_pair(lhs, std::move(parameters));
    return rvalue;
}

/**
 * @brief An rvalue or temporary wrapped in parenthesis, pre-evaluated
 *
 * @param node
 * @return RValue
 */
RValue Table::from_evaluated_expression(Node& node)
{
    RValue rvalue{};
    assert(node["node"].ToString().compare("evaluated_expression") == 0);
    rvalue.value = std::make_shared<RValue>(from_rvalue(node["root"]));
    return rvalue;
}

/**
 * @brief Relation to sum type of operator and chain of rvalues
 *
 * @param node
 * @return RValue
 */
RValue Table::from_relation_expression(Node& node)
{
    RValue rvalue{};
    assert(node["node"].ToString().compare("relation_expression") == 0);
    std::vector<RValue::RValue_Pointer> blocks{};
    if (node.hasKey("right") and
        node["right"]["node"].ToString() == "ternary_expression") {
        auto ternary = node["right"];
        auto op = node["root"].ArrayRange().get()->at(0).ToString();
        blocks.push_back(std::make_shared<RValue>(from_rvalue(node["left"])));
        blocks.push_back(
            std::make_shared<RValue>(from_rvalue(ternary["root"])));
        blocks.push_back(
            std::make_shared<RValue>(from_rvalue(ternary["left"])));
        blocks.push_back(
            std::make_shared<RValue>(from_rvalue(ternary["right"])));
        rvalue.value =
            std::make_pair(BINARY_OPERATORS.at(op), std::move(blocks));
    } else {
        auto op = node["root"].ArrayRange().get()->at(0).ToString();
        blocks.push_back(std::make_shared<RValue>(from_rvalue(node["left"])));
        blocks.push_back(std::make_shared<RValue>(from_rvalue(node["right"])));
        rvalue.value =
            std::make_pair(BINARY_OPERATORS.at(op), std::move(blocks));
    }
    return rvalue;
}

/**
 * @brief Unary operator expression to algebraic pair
 * @param node
 * @return RValue
 */
RValue Table::from_unary_expression(Node& node)
{
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
                    auto rhs =
                        std::make_shared<RValue>(from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_INC, rhs);

                } else if (node["root"].ArrayRange().get()->at(0).ToString() ==
                           "--") {
                    auto rhs =
                        std::make_shared<RValue>(from_rvalue(node["left"]));
                    rvalue.value = std::make_pair(Operator::PRE_DEC, rhs);
                }
            },
        pattern | "post_inc_dec_expression" =
            [&] {
                if (node["root"].ArrayRange().get()->at(0).ToString() == "++") {
                    auto rhs =
                        std::make_shared<RValue>(from_rvalue(node["right"]));
                    rvalue.value = std::make_pair(Operator::POST_INC, rhs);

                } else if (node["root"].ArrayRange().get()->at(0).ToString() ==
                           "--") {
                    auto rhs =
                        std::make_shared<RValue>(from_rvalue(node["right"]));
                    rvalue.value =
                        std::make_pair(Operator::POST_DEC, std::move(rhs));
                }
            },
        pattern | "address_of_expression" =
            [&] {
                auto op = node["root"].ArrayRange().get()->at(0).ToString();
                assert(op.compare("&") == 0);
                auto rhs = std::make_shared<RValue>(from_rvalue(node["left"]));
                rvalue.value = std::make_pair(Operator::U_ADDR_OF, rhs);
            },
        // otherwise:
        pattern | _ =
            [&] {
                auto op = node["root"].ArrayRange().get()->at(0).ToString();
                auto rhs = std::make_shared<RValue>(from_rvalue(node["left"]));
                rvalue.value = std::make_pair(other_unary.at(op), rhs);
            });
    return rvalue;
}

/**
 * @brief Parse assignment expression into pairs of left-hand-side and
 * right-hand-side
 *
 * @param node
 * @return std::pair<RValue, RValue>
 */
RValue Table::from_assignment_expression(Node& node)
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
    RValue rvalue = RValue{};
    rvalue.value = std::make_pair(lhs, std::make_shared<RValue>(rhs));

    return rvalue;
}

/**
 * @brief Parse lvalue expression data types
 *
 * @param node
 * @return RValue::LValue
 */
RValue::LValue Table::from_lvalue_expression(Node& node)
{
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
        error("undefined identifier, did you forget to declare with "
              "auto or extern?",
              name);
    }
    RValue::LValue lvalue{};
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
 *
 * @param node
 * @return RValue
 */
RValue::Value Table::from_constant_expression(Node& node)
{
    auto constant_type = node["node"].ToString();
    return match(constant_type)(
        pattern |
            "constant_literal" = [&] { return from_constant_literal(node); },
        pattern | "number_literal" = [&] { return from_number_literal(node); },
        pattern | "string_literal" = [&] { return from_string_literal(node); });
}

/**
 * @brief Parse lvalue to pointer data type
 * @param node
 * @return RValue
 */
RValue::Value Table::from_indirect_identifier(Node& node)
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
 *
 * @param node
 */
RValue::Value Table::from_vector_idenfitier(Node& node)
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
 *
 * @param node
 */
RValue::Value Table::from_number_literal(Node& node)
{
    // use stack
    assert(node["node"].ToString().compare("number_literal") == 0);
    return { static_cast<int>(node["root"].ToInt()), Type_["int"] };
}

/**
 * @brief Parse string literal node into symbols
 *
 * @param node
 */
RValue::Value Table::from_string_literal(Node& node)
{
    assert(node["node"].ToString().compare("string_literal") == 0);
    auto value = node["root"].ToString();
    return { node["root"].ToString(), { "string", value.size() } };
}

/**
 * @brief Parse constant literal node into symbols
 *
 * @param node
 */
RValue::Value Table::from_constant_literal(Node& node)
{
    assert(node["node"].ToString().compare("constant_literal") == 0);
    return { static_cast<char>(node["root"].ToString()[0]), Type_["char"] };
}

} // namespace ir

} // namespace roxas