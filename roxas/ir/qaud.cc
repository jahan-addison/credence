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

// clang-format off
#include <roxas/ir/qaud.h>
#include <assert.h>          // for assert
#include <matchit.h>         // for pattern, PatternHelper, PatternPipable
#include <roxas/ir/table.h>  // for Table
#include <roxas/ir/util.h>   // for rvalue_queue_to_instructions
#include <roxas/json.h>      // for JSON
#include <roxas/queue.h>     // for rvalues_to_queue, RValue_Queue
#include <roxas/symbol.h>    // for Symbol_Table
#include <roxas/types.h>     // for rvalue_type_pointer_from_rvalue, RValue
#include <utility>           // for pair
#include <variant>           // for variant
// clang-format on

namespace roxas {

namespace ir {

using namespace type;

/**
 * @brief Construct a set of qaud instructions from a set of definitions
 */
Instructions build_from_definitions(Symbol_Table<>& symbols,
                                    Node& node,
                                    Node& details)
{
    using namespace matchit;
    assert(node["root"].ToString().compare("definitions") == 0);
    Instructions instructions{};
    auto definitions = node["left"];
    for (auto& definition : definitions.ArrayRange()) {
        match(definition["node"].ToString())(
            // clang-format off
            pattern | "function_definition" = [&] {
                auto function_instructions =
                    build_from_function_definition(symbols, definition, details);
                instructions.insert(instructions.end(),
                                    function_instructions.begin(),
                                    function_instructions.end());
            }
            // clang-format on
        );
    }
    return instructions;
}

/**
 * @brief Construct a set of qaud instructions from a function definition
 */
Instructions build_from_function_definition(Symbol_Table<>& symbols,
                                            Node& node,
                                            Node& details)
{
    using namespace matchit;
    Instructions instructions{};
    assert(node["node"].ToString().compare("function_definition") == 0);
    Symbol_Table<> block_level{};
    auto name =
        node["root"].ToString() == "main" ? "__main" : node["root"].ToString();
    auto parameters = node["left"];
    auto block = node["right"];

    symbols.set_symbol_by_name(name, { "__WORD__", type::Type_["word"] });

    if (parameters.JSONType() == json::JSON::Class::Array and
        !parameters.ArrayRange().get()->at(0).IsNull()) {
        for (auto& ident : parameters.ArrayRange()) {
            match(ident["node"].ToString())(
                pattern | "lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(ident["root"].ToString(),
                                                       NULL_DATA_TYPE);
                    },
                pattern | "vector_lvalue" =
                    [&] {
                        auto size = ident["left"]["root"].ToInt();
                        block_level.set_symbol_by_name(
                            ident["root"].ToString(),
                            { static_cast<type::Byte>('0'), { "byte", size } });
                    },
                pattern | "indirect_lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(
                            ident["left"]["root"].ToString(),
                            { "__WORD__", type::Type_["word"] });
                    });
        }
    }
    instructions.push_back(make_quadruple(Instruction::LABEL, name, ""));
    instructions.push_back(make_quadruple(Instruction::FUNC_START, "", ""));
    auto block_instructions =
        build_from_block_statement(block_level, block, details);
    instructions.insert(instructions.end(),
                        block_instructions.begin(),
                        block_instructions.end());
    instructions.push_back(make_quadruple(Instruction::FUNC_END, "", ""));
    return instructions;
}

/**
 * @brief Construct a set of qaud instructions from a block statement
 */
Instructions build_from_block_statement(Symbol_Table<>& symbols,
                                        Node& node,
                                        Node& details)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("block") == 0);
    Instructions instructions{};
    assert(node.hasKey("left"));
    auto statements = node["left"];
    Table table{ details, symbols };
    for (auto& statement : statements.ArrayRange()) {
        auto statement_type = statement["root"].ToString();
        match(statement_type)(
            pattern |
                "auto" = [&] { build_from_auto_statement(symbols, statement); },
            pattern | "rvalue" =
                [&] {
                    auto rvalue_instructions = build_from_rvalue_statement(
                        symbols, statement, details);
                    instructions.insert(instructions.end(),
                                        rvalue_instructions.begin(),
                                        rvalue_instructions.end());
                }

        );
    }
    return instructions;
}

/**
 * @brief Symbol construction from auto declaration statements
 */
void build_from_auto_statement(Symbol_Table<>& symbols, Node& node)
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
 * @brief Construct a set of qaud instructions from an rvalue statement
 */
Instructions build_from_rvalue_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("rvalue") == 0);
    int temporary{ 0 };
    Instructions instructions{};
    assert(node.hasKey("left"));
    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    auto statement = node["left"];
    Table table{ details, symbols };
    // for each line:
    for (auto& expression : statement.ArrayRange()) {
        if (expression.JSONType() == json::JSON::Class::Array) {
            for (auto& rvalue : expression.ArrayRange()) {
                rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                    table.from_rvalue(rvalue).value));
            }
        } else {
            rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                table.from_rvalue(expression).value));
        }
        rvalues_to_queue(rvalues, &list);
        auto line = rvalue_queue_to_instructions(&list, &temporary);

        instructions.insert(instructions.end(), line.begin(), line.end());
        rvalues.clear();
        list.clear();
    }

    return instructions;
}

} // namespace ir

} // namespace roxas