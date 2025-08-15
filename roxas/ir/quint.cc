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
#include <assert.h>  // for assert
#include <matchit.h> // for pattern, PatternHelper, PatternPipable
#include <roxas/ir/quint.h>
#include <roxas/ir/types.h> // for Byte, Type_
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table
#include <utility>          // for pair
#include <variant>          // for variant

namespace roxas {

namespace ir {

namespace quint {

using namespace matchit;

/**
 * @brief auto statement symbol construction
 *
 * @param node
 */
void build_from_auto_statement(Symbol_Table<>& symbols, Node& node)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("auto") == 0);
    assert(node.hasKey("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.ArrayRange()) {
        match(ident["node"].ToString())(
            pattern | "lvalue" =
                [&] {
                    symbols.set_symbol_by_name(ident["root"].ToString(),
                                               NULL_DATA_TYPE);
                },
            pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].ToInt();
                    symbols.set_symbol_by_name(
                        ident["root"].ToString(),
                        { static_cast<type::Byte>('0'), { "byte", size } });
                },
            pattern | "indirect_lvalue" =
                [&] {
                    symbols.set_symbol_by_name(
                        ident["left"]["root"].ToString(),
                        { "__WORD__", type::Type_["word"] });
                });
    }
}

/**
 * @brief Parse an expression node in recursive descent
 *
 * @param node
 */

Instructions build_from_rvalue_statement([[maybe_unused]] Node& node)
{
    Instructions instructions{};
    //     if (node.JSONType() == json::JSON::Class::Array) {
    //         for (auto& child_node : node.ArrayRange()) {
    //             from_expression(child_node);
    //         }
    //     }
    //     /* clang-format off */
    //     match(node["node"].ToString()) (
    //         /**************/
    //         /* Statements */
    //         /**************/
    //         pattern | "statement" = [&] {
    //             auto statement_type = node["root"].ToString();
    //             match (statement_type) (
    //                 pattern | "auto" = [&] {
    //                     util::log(util::Logging::INFO, "parsing auto
    //                     statement"); from_auto_statement(node);
    //                 }
    //             );
    //         },
    //         /**************/
    //         /* Expressions */
    //         /**************/
    //         pattern | "assignment_expression" = [&] {
    //             util::log(util::Logging::INFO, "parsing assignment
    //             expression"); from_assignment_expression(node);
    //         }
    //     );
    //     // /* clang-format on */
    //     if (node.hasKey("left")) {
    //         from_expression(node["left"]);
    //     }
    //     if (node.hasKey("right")) {
    //         from_expression(node["right"]);
    //     }
    return instructions;
}

} // namespace quint

} // namespace ir

} // namespace roxas