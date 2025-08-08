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
#include <matchit.h> // for pattern, match, PatternHelper, Patte...
#include <roxas/ir/ir.h>
#include <roxas/ir/operators.h> // for Operator
#include <roxas/util.h>         // for log, Logging, tuple_to_string
#include <stdexcept>            // for runtime_error
#include <tuple>                // for make_tuple, tuple

#include <roxas/json.h>   // for JSON
#include <roxas/symbol.h> // for Symbol_Table

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
        assert(ident["node"].ToString().compare("lvalue") == 0);
        symbols_.set_symbol_by_name(ident["root"].ToString(),
                                    { "", "null", 0 });
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

    on_identifier(left_child_node);

    auto rhs = match(right_child_node["node"].ToString())(
        pattern | "constant_literal" =
            [&] { return from_constant_literal(right_child_node); },
        pattern | "number_literal" =
            [&] { return from_number_literal(right_child_node); },
        pattern | "string_literal" =
            [&] { return from_string_literal(right_child_node); },
        pattern | _ =
            [&] {
                util::log(util::Logging::WARNING,
                          "lhs of assignment expression has unknown type");
                return std::make_tuple("", "null", 0);
            });

    quintuples_.emplace_back(std::make_tuple(Operator::EQUAL,
                                             left_child_node["root"].ToString(),
                                             util::tuple_to_string(rhs, ":"),
                                             "",
                                             ""));
}
/*
 * @brief
 * Parse identifier lvalue
 *
 *  Parse and verify identifer is declared with auto or extern
 *
 * @param node
 */
void Intermediate_Representation::on_identifier(Node& node)
{
    assert(node["node"].ToString().compare("lvalue") == 0);
    auto lvalue = node["root"].ToString();
    if (!symbols_.get_symbol_defined(lvalue))
        parsing_error("identifier not declared with 'auto' or 'extern'",
                      lvalue);
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
    return { std::to_string(node["root"].ToInt()), "int", sizeof(int) };
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
    return { value, "string", value.size() };
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
    return { node["root"].ToString(), "int", sizeof(int) };
}

} // namespace ir

} // namespace roxas