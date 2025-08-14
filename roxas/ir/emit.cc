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

#include <roxas/ir/emit.h>

namespace roxas {

namespace ir {

/**
 * @brief Parse an expression node in recursive descent
 *
 * @param node
 */

// void Table::from_expression(Node& node)
// {
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
//                     util::log(util::Logging::INFO, "parsing auto statement");
//                     from_auto_statement(node);
//                 }
//             );
//         },
//         /**************/
//         /* Expressions */
//         /**************/
//         pattern | "assignment_expression" = [&] {
//             util::log(util::Logging::INFO, "parsing assignment expression");
//             from_assignment_expression(node);
//         }
//     );
//     // /* clang-format on */
//     if (node.hasKey("left")) {
//         from_expression(node["left"]);
//     }
//     if (node.hasKey("right")) {
//         from_expression(node["right"]);
//     }
// }

} // namespace ir

} // namespace roxas