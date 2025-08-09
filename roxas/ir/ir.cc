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

#include <algorithm>       // for max
#include <cassert>         // for assert
#include <format>          // for format, format_string
#include <map>             // for map
#include <matchit.h>       // for pattern, match, PatternHelper, Patte...
#include <roxas/ir/emit.h> // for emit_value
#include <roxas/ir/ir.h>
#include <roxas/ir/operators.h> // for Operator
#include <roxas/ir/types.h>     // for Type_
#include <roxas/json.h>         // for JSON
#include <roxas/symbol.h>       // for Symbol_Table
#include <roxas/util.h>         // for log, Logging
#include <stdexcept>            // for runtime_error
#include <tuple>                // for make_tuple
#include <utility>              // for pair, make_pair
#include <variant>              // for monostate, variant

namespace roxas {

namespace ir {

using DataType = Intermediate_Representation::DataType;

/**
 * @brief
 *  Parse an ast node recursively
 *
 * @param node
 */
void Intermediate_Representation::parse_node(Node& node)
{
    using namespace matchit;
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
    /* clang-format on */
    if (node.hasKey("left")) {
        parse_node(node["left"]);
    }
    if (node.hasKey("right")) {
        parse_node(node["right"]);
    }
}

/**
 * @brief
 * Parse fixed-size vector (array) lvalue
 *
 * @param node
 */
void Intermediate_Representation::from_vector_idenfitier(Node& node)
{
    assert(node["node"].ToString().compare("vector_lvalue") == 0);

    check_identifier_symbol(node);
}

/**
 * @brief
 * Parse auto statements and declare in symbol table
 *
 * @param node
 */
void Intermediate_Representation::from_auto_statement(Node& node)
{
    using namespace matchit;
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
                    symbols_.set_symbol_by_name(ident["root"].ToString(),
                                                { "__WORD_", Type_["word"] });
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
 *  Parse assignment expression
 *
 * @param node
 */
void Intermediate_Representation::from_assignment_expression(Node& node)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("assignment_expression") == 0);
    assert(node.hasKey("left"));
    assert(node.hasKey("right"));

    auto left_child_node = node["left"];
    auto right_child_node = node["right"];

    check_identifier_symbol(left_child_node);

    DataType rhs = match(right_child_node["node"].ToString())(
        pattern | "constant_literal" =
            [&] { return from_constant_literal(right_child_node); },
        pattern | "number_literal" =
            [&] { return from_number_literal(right_child_node); },
        pattern | "string_literal" =
            [&] { return from_string_literal(right_child_node); },
        pattern | _ =
            [&] { return std::make_pair(std::monostate(), Type_["null"]); });

    quintuples_.emplace_back(std::make_tuple(Operator::EQUAL,
                                             left_child_node["root"].ToString(),
                                             ir::emit_value(rhs, ":"),
                                             "",
                                             ""));
}
/*
 * @brief
 * Parse identifier lvalue
 *
 *  Parse and verify scaler identifer is declared with auto or extern
 *
 * @param node
 */
void Intermediate_Representation::check_identifier_symbol(Node& node)
{
    auto lvalue = node["root"].ToString();
    if (!symbols_.get_symbol_defined(lvalue) &&
        (!globals_.get_symbol_defined(lvalue))) {
        parsing_error("identifier not declared with 'auto' or 'extern'",
                      lvalue);
    }
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