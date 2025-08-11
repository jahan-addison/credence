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

#include <cassert>   // for assert
#include <format>    // for format
#include <map>       // for map
#include <matchit.h> // for pattern, PatternHelper, PatternPipable
#include <memory>    // for make_unique, unique_ptr
#include <roxas/ir/ir.h>
#include <roxas/ir/types.h> // for Type_, Byte
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table
#include <roxas/util.h>     // for log, Logging
#include <stdexcept>        // for runtime_error
#include <utility>          // for pair, make_pair
#include <variant>          // for monostate, variant

namespace roxas {

namespace ir {

using namespace matchit;

using DataType = Intermediate_Representation::DataType;

/**
 * @brief
 * Throw Parsing or program flow error
 *
 * Use source data provided by internal symbol table
 *
 * @param message
 * @param object
 */
void Intermediate_Representation::parsing_error(std::string_view message,
                                                std::string_view object)
{
    if (internal_symbols_.hasKey(object.data())) {
        throw std::runtime_error(std::format(
            "Parsing error :: \"{}\" {}\n\t"
            "on line {} in column {} :: {}",
            object,
            message,
            internal_symbols_[object.data()]["line"].ToInt(),
            internal_symbols_[object.data()]["column"].ToInt(),
            internal_symbols_[object.data()]["end_column"].ToInt()));
    } else {
        throw std::runtime_error(
            std::format("Parsing error :: \"{}\" {}", object.data(), message));
    }
}

/**
 * @brief
 *  Parse an ast node recursively
 *
 * @param node
 */
void Intermediate_Representation::parse_node(Node& node)
{
    if (node.JSONType() == json::JSON::Class::Array) {
        for (auto& child_node : node.ArrayRange()) {
            parse_node(child_node);
        }
    }
    /* clang-format off */
    match(node["node"].ToString()) (
        /**************/
        /* Statements */
        /**************/
        pattern | "statement" = [&] {
            auto statement_type = node["root"].ToString();
            match (statement_type) (
                pattern | "auto" = [&] {
                    util::log(util::Logging::INFO, "parsing auto statement");
                    from_auto_statement(node);
                }
            );
        },
        /**************/
        /* Expressions */
        /**************/
        pattern | "assignment_expression" = [&] {
            util::log(util::Logging::INFO, "parsing assignment expression");
            from_assignment_expression(node);
        }
    );
    // /* clang-format on */
    if (node.hasKey("left")) {
        parse_node(node["left"]);
    }
    if (node.hasKey("right")) {
        parse_node(node["right"]);
    }
}

/**
 * @brief
 * Parse auto statements and declare in symbol table
 *
 * @param node
 */
void Intermediate_Representation::from_auto_statement(Node& node)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("auto") == 0);
    assert(node.hasKey("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.ArrayRange()) {
        match(ident["node"].ToString())(
            pattern | "lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(
                        ident["root"].ToString(),
                        { std::monostate(), Type_["null"] });
                },
            pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].ToInt();
                    symbols_.set_symbol_by_name(
                        ident["root"].ToString(),
                        { static_cast<Byte>('0'), { "byte", size } });
                },
            pattern | "indirect_lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(
                        ident["left"]["root"].ToString(),
                        { "__WORD_", Type_["word"] });
                });
    }
}

/**
 * @brief
 * Parse rvalue expression data types
 *
 * @param node
 * @return DataType
 */
RValue Intermediate_Representation::from_rvalue_expression(Node& node)
{
    RValue rvalue = RValue{};
     match(node["node"].ToString())(
        pattern | "constant_literal" = [&] {
            rvalue.value = from_constant_expression(node); },
        pattern | "number_literal" = [&] {
            rvalue.value = from_constant_expression(node); },
        pattern | "string_literal" = [&] {
            rvalue.value = from_constant_expression(node); },
        pattern | "lvalue" = [&] {
            rvalue.value = from_lvalue_expression(node); },
        pattern | "vector_lvalue" = [&] {
            rvalue.value = from_lvalue_expression(node); },
        pattern | "indirect_lvalue" = [&] {
            rvalue.value = from_lvalue_expression(node); },
        pattern | "assignment_expression" = [&] {
            rvalue.value = std::make_unique<RValue>(from_assignment_expression(node)); },
        pattern | _ = [&] {
            rvalue.value = std::monostate();
        }

    );
    return rvalue;
}

/**
 * @brief
 * Parse assignment expression into pairs of left-hand-side and right-hand-side
 *
 * @param node
 * @return std::pair<DataType, DataType>
 */
RValue Intermediate_Representation::from_assignment_expression(Node& node)
{
    assert(node["node"].ToString().compare("assignment_expression") == 0);
    assert(node.hasKey("left"));
    assert(node.hasKey("right"));

    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    if (!is_symbol(left_child_node)) {
        parsing_error(
            "lvalue of assignment not declared with 'auto' or 'extern'",
            left_child_node["root"].ToString());
    }

    auto lhs = from_lvalue_expression(left_child_node);
    auto rhs = from_rvalue_expression(right_child_node);
    RValue rvalue = RValue{};
    rvalue.value =
        std::make_pair(lhs, std::make_unique<RValue>(std::move(rhs)));

    return rvalue;
}

/**
 * @brief
 * Parse lvalue expression data types
 *
 * @param node
 * @return LValue
 */
LValue Intermediate_Representation::from_lvalue_expression(Node& node)
{
    auto constant_type = node["node"].ToString();
    if (!symbols_.get_symbol_defined(node["root"].ToString()) &&
        !symbols_.get_symbol_defined(node["left"]["root"].ToString())) {
        auto name = node.hasKey("left") ? node["left"]["root"].ToString()
                                        : node["root"].ToString();
        parsing_error("undefined lvalue, did you forget to declare with "
                      "auto or extern?",
                      name);
    }
    LValue lvalue{};
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
 * @brief
 * Parse constant expression data types
 *
 * @param node
 * @return DataType
 */
DataType Intermediate_Representation::from_constant_expression(Node& node)
{
    auto constant_type = node["node"].ToString();
    return match(constant_type)(
        pattern |
            "constant_literal" = [&] { return from_constant_literal(node); },
        pattern | "number_literal" = [&] { return from_number_literal(node); },
        pattern | "string_literal" = [&] { return from_string_literal(node); });
}

/**
 * @brief
 * Parse lvalue to pointer data type
 *
 * @param node
 * @return DataType
 */
DataType Intermediate_Representation::from_indirect_identifier(Node& node)
{
    assert(node["node"].ToString().compare("indirect_lvalue") == 0);
    assert(node.hasKey("left"));
    if (!is_symbol(node["left"])) {
        parsing_error("pointer not declared with 'auto' or 'extern'",
                      node["root"].ToString());
    }
    return symbols_.get_symbol_by_name(node["left"]["root"].ToString());
}

/**
 * @brief
 * Parse fixed-size vector (array) lvalue
 *
 * @param node
 */
DataType Intermediate_Representation::from_vector_idenfitier(Node& node)
{
    assert(node["node"].ToString().compare("vector_lvalue") == 0);

    if (!is_symbol(node)) {
        parsing_error("vector not declared with 'auto' or 'extern'",
                      node["root"].ToString());
    }
    return symbols_.get_symbol_by_name(node["root"].ToString());
}

/**
 * @brief
 * Parse number literal node into IR symbols
 *
 * @param node
 */
DataType Intermediate_Representation::from_number_literal(Node& node)
{
    // use stack
    assert(node["node"].ToString().compare("number_literal") == 0);
    return { static_cast<int>(node["root"].ToInt()), Type_["int"] };
}

/**
 * @brief
 * Parse string literal node into IR symbols
 *
 * @param node
 */
DataType Intermediate_Representation::from_string_literal(Node& node)
{
    // use stack
    assert(node["node"].ToString().compare("string_literal") == 0);
    auto value = node["root"].ToString();
    return { node["root"].ToString(), { "string", value.size() } };
}

/**
 * @brief
 * Parse constant literal node into IR symbols
 *
 * @param node
 */
DataType Intermediate_Representation::from_constant_literal(Node& node)
{
    // use stack
    assert(node["node"].ToString().compare("constant_literal") == 0);
    return { static_cast<char>(node["root"].ToString()[0]), Type_["char"] };
}

} // namespace ir

} // namespace roxas