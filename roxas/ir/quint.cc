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
#include <iostream>  // for cout
#include <matchit.h> // for pattern, PatternHelper, PatternPipable
#include <memory>    // for unique_ptr
#include <roxas/ir/quint.h>
#include <roxas/ir/table.h>  // for Table
#include <roxas/json.h>      // for JSON
#include <roxas/operators.h> // for operator_to_string, Operato
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for RValue, Byte, Type_
#include <roxas/util.h>      // for overload
#include <stack>
#include <utility> // for pair, make_pair
#include <variant> // for get, monostate, visit, variant

namespace roxas {

namespace ir {

namespace quint {

using namespace type;
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

std::vector<std::string> build_from_rvalue(RValue::Type& rvalue)
{
    std::vector<std::string> items{};
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](RValue::_RValue&) {},
                   [&](RValue::Value&) {},
                   [&](RValue::LValue&) {},
                   [&](RValue::Unary&) {},
                   [&](RValue::Relation&) {},
                   [&](RValue::Function&) {},
                   [&](RValue::Symbol& s) {
                       items.push_back(s.first.first);
                       items.push_back(operator_to_string(Operator::R_EQUAL));
                       auto assignee = build_from_rvalue(s.second->value);
                       items.insert(
                           items.end(), assignee.begin(), assignee.end());
                   }

               },
               rvalue);
    return items;
}

Instructions build_from_rvalue_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("rvalue") == 0);
    assert(node.hasKey("left"));
    auto statement = node["left"];
    Instructions instructions{};
    Table table{ details, symbols };
    auto unraveled_statements = util::unravel_nested_node_array(&statement);
    for (auto& expressions : unraveled_statements->ArrayRange()) {
        match(expressions["node"].ToString())(
            pattern | "function_expression" =
                [&] {
                    auto rvalue = table.from_rvalue(expressions);
                    auto* function = &std::get<RValue::_RValue>(rvalue.value);
                    auto* expression =
                        &std::get<RValue::Function>(function->get()->value);
                    auto name = expression->first;
                    for (auto& parameters : expression->second) {
                        auto* param =
                            &std::get<RValue::LValue>(parameters->value);
                        instructions.push_back(std::make_tuple(
                            Instruction::PUSH, param->first, "", ""));
                    }
                    instructions.push_back(
                        std::make_tuple(Instruction::CALL, name, "", ""));
                },

            pattern | "relation_expression" =
                [&] {
                    // auto rvalue = table.from_rvalue(expressions);
                    // auto* relation =
                    // &std::get<RValue::_RValue>(rvalue.value); auto*
                    // expression =
                    //     &std::get<RValue::Relation>(relation->get()->value);
                    // if (expression->second.size() == 2) {
                    //     auto lhs = build_from_rvalue(expression->second);
                    //     auto rhs =
                    //     build_from_rvalue(expression->second.at(1));
                    // }
                },
            pattern |
                "indirect_lvalue" = [&] { std::cout << "test" << std::endl; },
            pattern | "assignment_expression" =
                [&] { std::cout << "test" << std::endl; },
            pattern | _ = [&] { std::cout << "test" << std::endl; });
    }
    return instructions;
}

} // namespace quint

} // namespace ir

} // namespace roxas