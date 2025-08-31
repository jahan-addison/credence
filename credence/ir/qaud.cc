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
#include <credence/ir/qaud.h>
#include <assert.h>          // for assert
#include <matchit.h>         // for pattern, match, PatternHelper, PatternPi...
#include <credence/ir/table.h>  // for Table
#include <credence/ir/temp.h>   // for rvalue_queue_to_linear_ir_instructions
#include <credence/json.h>      // for JSON
#include <credence/queue.h>     // for rvalues_to_queue, RValue_Queue
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for rvalue_type_pointer_from_rvalue, RValue
#include <format>            // for format
#include <utility>           // for pair
#include <variant>           // for variant
// clang-format on

namespace credence {

namespace ir {

using namespace type;

/**
 * @brief Construct a set of qaud instructions from a set of definitions
 */
Instructions build_from_definitions(Symbol_Table<>& symbols,
                                    Symbol_Table<>& globals,
                                    Node& node,
                                    Node& details)
{
    using namespace matchit;
    assert(node["root"].ToString().compare("definitions") == 0);
    Instructions instructions{};
    auto definitions = node["left"];
    // vector definitions first
    for (auto& definition : definitions.ArrayRange())
        if (definition["node"].ToString() == "vector_definition")
            build_from_vector_definition(globals, definition, details);
    for (auto& definition : definitions.ArrayRange()) {
        match(definition["node"].ToString())(
            // clang-format off
            pattern | "function_definition" = [&] {
                auto function_instructions =
                    build_from_function_definition(symbols, globals, definition, details);
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
                                            Symbol_Table<>& globals,
                                            Node& node,
                                            Node& details)
{
    using namespace matchit;
    Instructions instructions{};
    assert(node["node"].ToString().compare("function_definition") == 0);
    Symbol_Table<> block_level{};
    auto name = node["root"].ToString();
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
    instructions.push_back(make_quadruple(
        Instruction::LABEL, std::format("__{}", node["root"].ToString()), ""));
    instructions.push_back(make_quadruple(Instruction::FUNC_START, "", ""));
    auto block_instructions =
        build_from_block_statement(block_level, globals, block, details);
    instructions.insert(instructions.end(),
                        block_instructions.begin(),
                        block_instructions.end());
    instructions.push_back(make_quadruple(Instruction::FUNC_END, "", ""));
    return instructions;
}

void build_from_vector_definition(Symbol_Table<>& symbols,
                                  Node& node,
                                  Node& details)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("vector_definition") == 0);
    assert(node.hasKey("left"));
    auto name = node["root"].ToString();
    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    Table table{ details, symbols };
    if (right_child_node.ArrayRange().get()->empty()) {
        auto rvalue = table.from_rvalue(left_child_node);
        auto datatype = std::get<type::RValue::Value>(rvalue.value);
        symbols.set_symbol_by_name(name, datatype);
    } else {
        if (std::cmp_not_equal(left_child_node["root"].ToInt(),
                               right_child_node.ArrayRange().get()->size())) {
            throw std::runtime_error("Error: invalid vector definition, "
                                     "size of vector and rvalue "
                                     "entries do not match");
        }
        std::vector<RValue::Value> values_at{};
        for (auto& child_node : right_child_node.ArrayRange()) {
            auto rvalue = table.from_rvalue(child_node);
            auto datatype = std::get<type::RValue::Value>(rvalue.value);
            values_at.push_back(datatype);
        }
        symbols.set_symbol_by_name(name, values_at);
    }
}

/**
 * @brief Construct a set of qaud instructions from a block statement
 */
Instructions build_from_block_statement(Symbol_Table<>& symbols,
                                        Symbol_Table<>& globals,
                                        Node& node,
                                        Node& details)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("block") == 0);
    Instructions instructions{};
    Instructions branches{};
    int temporary{ 0 };
    assert(node.hasKey("left"));
    auto statements = node["left"];
    Table table{ details, symbols };
    for (auto& statement : statements.ArrayRange()) {
        auto statement_type = statement["root"].ToString();
        match(statement_type)(
            pattern |
                "auto" = [&] { build_from_auto_statement(symbols, statement); },
            pattern | "extrn" =
                [&] {
                    build_from_extrn_statement(symbols, globals, statement);
                },
            pattern | "if" =
                [&] {
                    auto [jump_instructions, if_instructions] =
                        build_from_if_statement(
                            symbols, globals, statement, details, &temporary);
                    instructions.insert(instructions.end(),
                                        jump_instructions.begin(),
                                        jump_instructions.end());
                    branches.insert(branches.end(),
                                    if_instructions.begin(),
                                    if_instructions.end());
                },
            pattern | "rvalue" =
                [&] {
                    auto rvalue_instructions = build_from_rvalue_statement(
                        symbols, statement, details, &temporary);
                    instructions.insert(instructions.end(),
                                        rvalue_instructions.begin(),
                                        rvalue_instructions.end());
                },
            pattern | "label" =
                [&] {
                    auto label_instructions =
                        build_from_label_statement(symbols, statement, details);
                    instructions.insert(instructions.end(),
                                        label_instructions.begin(),
                                        label_instructions.end());
                },
            pattern | "goto" =
                [&] {
                    auto goto_statement =
                        build_from_goto_statement(symbols, statement, details);
                    instructions.insert(instructions.end(),
                                        goto_statement.begin(),
                                        goto_statement.end());
                },
            pattern | "return" =
                [&] {
                    auto return_instructions = build_from_return_statement(
                        symbols, statement, details, &temporary);
                    instructions.insert(instructions.end(),
                                        return_instructions.begin(),
                                        return_instructions.end());
                }

        );
    }
    instructions.insert(instructions.end(), branches.begin(), branches.end());
    return instructions;
}

/**
 * @brief Construct an rvalue or block statement for a branch
 */
void insert_rvalue_or_block_branch_instructions(
    Symbol_Table<>& symbols,
    Symbol_Table<>& globals,
    Node& block,
    Node& details,
    int* temporary,
    Instructions& branch_instructions)
{
    if (block["root"].ToString() == "block") {
        auto block_instructions =
            build_from_block_statement(symbols, globals, block, details);

        branch_instructions.insert(branch_instructions.end(),
                                   block_instructions.begin(),
                                   block_instructions.end());
    } else {
        auto rvalue_instructions =
            build_from_rvalue_statement(symbols, block, details, temporary);
        branch_instructions.insert(branch_instructions.end(),
                                   rvalue_instructions.begin(),
                                   rvalue_instructions.end());
    }
}

/**
 * @brief Construct a set of qaud instructions from an if statement
 */
std::pair<Instructions, Instructions> build_from_if_statement(
    Symbol_Table<>& symbols,
    Symbol_Table<>& globals,
    Node& node,
    Node& details,
    int* temporary)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("if") == 0);
    assert(node.hasKey("left"));
    assert(node.hasKey("right"));
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    RValue_Queue list{};
    auto predicate = node["left"];
    auto* blocks = node["right"].ArrayRange().get();
    Table table{ details, symbols };

    auto predicate_rvalue = type::rvalue_type_pointer_from_rvalue(
        table.from_rvalue(predicate).value);
    rvalues_to_queue(predicate_rvalue, &list);
    auto if_instructions =
        rvalue_queue_to_linear_ir_instructions(&list, temporary);

    auto jump = detail::make_temporary(temporary);

    predicate_instructions.insert(predicate_instructions.end(),
                                  if_instructions.begin(),
                                  if_instructions.end());

    predicate_instructions.push_back(
        make_quadruple(Instruction::IF,
                       std::get<1>(if_instructions.back()),
                       instruction_to_string(Instruction::GOTO),
                       std::get<1>(jump)));

    branch_instructions.push_back(jump);

    if (blocks->at(0)["root"].ToString() == "block") {
        insert_rvalue_or_block_branch_instructions(symbols,
                                                   globals,
                                                   blocks->at(0),
                                                   details,
                                                   temporary,
                                                   branch_instructions);
    }

    // else statement
    if (!blocks->at(1).IsNull()) {
        auto else_label = detail::make_temporary(temporary);
        predicate_instructions.push_back(
            make_quadruple(Instruction::GOTO, std::get<1>(else_label), ""));
        branch_instructions.push_back(else_label);
        insert_rvalue_or_block_branch_instructions(symbols,
                                                   globals,
                                                   blocks->at(1),
                                                   details,
                                                   temporary,
                                                   branch_instructions);
    }

    return std::make_pair(predicate_instructions, branch_instructions);
}

/**
 * @brief Construct a set of qaud instructions from a label statement
 */
Instructions build_from_label_statement(Symbol_Table<>& symbols,
                                        Node& node,
                                        Node& details)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("label") == 0);
    assert(node.hasKey("left"));
    Instructions instructions{};
    auto statement = node["left"];
    Table table{ details, symbols };
    auto label = statement.ArrayRange().begin()->ToString();
    instructions.push_back(
        make_quadruple(Instruction::LABEL, std::format("_L_{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of qaud instructions from a goto statement
 */
Instructions build_from_goto_statement(Symbol_Table<>& symbols,
                                       Node& node,
                                       Node& details)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("goto") == 0);
    assert(node.hasKey("left"));
    Instructions instructions{};
    Table table{ details, symbols };
    auto statement = node["left"];
    auto label = statement.ArrayRange().begin()->ToString();
    if (!table.is_defined(label)) {
        throw std::runtime_error(
            std::format("Error: label \"{}\" does not exist", label));
    }

    instructions.push_back(make_quadruple(Instruction::GOTO, label, ""));
    return instructions;
}

/**
 * @brief Construct a set of qaud instructions from a block statement
 */
Instructions build_from_return_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details,
                                         int* temporary)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("return") == 0);
    Instructions instructions{};
    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    assert(node.hasKey("left"));
    auto return_statement = node["left"];
    Table table{ details, symbols };
    for (auto& expression : return_statement.ArrayRange()) {
        if (expression.JSONType() == json::JSON::Class::Array) {
            for (auto& rvalue : expression.ArrayRange()) {

                rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                    table.from_rvalue(rvalue).value));
            }
        } else {
            rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                table.from_rvalue(expression).value));
        }
    }
    rvalues_to_queue(rvalues, &list);
    auto return_instructions =
        rvalue_queue_to_linear_ir_instructions(&list, temporary);

    instructions.insert(instructions.end(),
                        return_instructions.begin(),
                        return_instructions.end());

    if (!list.empty() and instructions.empty()) {
        auto last_rvalue = std::get<type::RValue::Type_Pointer>(list.back());
        instructions.push_back(make_quadruple(
            Instruction::RETURN, util::rvalue_to_string(*last_rvalue), ""));
    } else {
        instructions.push_back(make_quadruple(Instruction::RETURN, "", ""));
    }

    return instructions;
}

void build_from_extrn_statement(Symbol_Table<>& symbols,
                                Symbol_Table<>& globals,
                                Node& node)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("extrn") == 0);
    assert(node.hasKey("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.ArrayRange()) {
        auto name = ident["root"].ToString();
        if (globals.is_defined(name)) {
            symbols.set_symbol_by_name(name, globals);
        } else {
            throw std::runtime_error(std::format(
                "Error: global symbol \"{}\" not defined for extrn statement",
                name));
        }
    }
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
                                         Node& details,
                                         int* temporary)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("rvalue") == 0);
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
        auto line = rvalue_queue_to_linear_ir_instructions(&list, temporary);

        instructions.insert(instructions.end(), line.begin(), line.end());
        rvalues.clear();
        list.clear();
    }

    return instructions;
}

/**
 * @brief Emit a qaudruple tuple to an std::ostream
 */
void emit_quadruple(std::ostream& os, Quadruple qaud)
{
    Instruction op = std::get<Instruction>(qaud);
    using namespace matchit;
    std::array<Instruction, 5> lhs_instruction = { Instruction::GOTO,
                                                   Instruction::PUSH,
                                                   Instruction::LABEL,
                                                   Instruction::POP,
                                                   Instruction::CALL };
    if (std::ranges::find(lhs_instruction, op) != lhs_instruction.end()) {
        if (op == Instruction::LABEL)
            os << std::get<1>(qaud) << ":" << std::endl;
        else
            os << op << " " << std::get<1>(qaud) << ";" << std::endl;
    } else {
        match(op)(
            pattern | Instruction::RETURN =
                [&] {
                    os << op << " " << std::get<1>(qaud) << ";" << std::endl;
                },
            pattern | Instruction::IF =
                [&] {
                    os << op << " " << std::get<1>(qaud) << " "
                       << std::get<2>(qaud) << " " << std::get<3>(qaud) << ";"
                       << std::endl;
                },
            pattern | _ =
                [&] {
                    os << std::get<1>(qaud) << " " << op << " "
                       << std::get<2>(qaud) << std::get<3>(qaud) << ";"
                       << std::endl;
                }

        );
    }
}

} // namespace ir

} // namespace credence