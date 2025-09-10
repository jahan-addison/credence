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
#include <assert.h>            // for assert
#include <credence/ir/temp.h>  // for make_temporary, rvalue_node_to_list_of...
#include <simplejson.h>     // for JSON
#include <credence/queue.h>    // for rvalue_to_string, RValue_Queue
#include <credence/rvalue.h>   // for RValue_Parser
#include <credence/symbol.h>   // for Symbol_Table
#include <credence/types.h>    // for RValue, RValue_Type_Variant, Type_, Byte
#include <matchit.h>           // for pattern, match, PatternHelper, Pattern...
#include <algorithm>           // for copy, max
#include <array>               // for array
#include <format>              // for format, format_string
#include <functional>          // for identity
#include <list>                // for list
#include <memory>              // for __shared_ptr_access, shared_ptr
#include <ostream>             // for basic_ostream, operator<<, endl
#include <ranges>              // for __find_fn, find
#include <stdexcept>           // for runtime_error
#include <utility>             // for pair, make_pair, cmp_not_equal
#include <variant>             // for get, variant
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
    assert(node["root"].to_string().compare("definitions") == 0);
    Instructions instructions{};
    auto definitions = node["left"];
    // vector definitions first
    for (auto& definition : definitions.array_range())
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(globals, definition, details);
    for (auto& definition : definitions.array_range()) {
        // cppcheck-suppress syntaxError
        match(definition["node"].to_string())(pattern |
                                                  "function_definition" = [&] {
            auto function_instructions = build_from_function_definition(
                symbols, globals, definition, details);
            instructions.insert(instructions.end(),
                                function_instructions.begin(),
                                function_instructions.end());
        });
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
    assert(node["node"].to_string().compare("function_definition") == 0);
    Symbol_Table<> block_level{};
    int temporary{ 0 };
    auto name = node["root"].to_string();
    auto parameters = node["left"];
    auto block = node["right"];

    symbols.set_symbol_by_name(name,
                               { "__WORD__", type::LITERAL_TYPE.at("word") });

    if (parameters.JSON_type() == json::JSON::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            match(ident["node"].to_string())(
                pattern | "lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(
                            ident["root"].to_string(), type::NULL_LITERAL);
                    },
                pattern | "vector_lvalue" =
                    [&] {
                        auto size = ident["left"]["root"].to_int();
                        block_level.set_symbol_by_name(
                            ident["root"].to_string(),
                            { static_cast<type::Byte>('0'), { "byte", size } });
                    },
                pattern | "indirect_lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(
                            ident["left"]["root"].to_string(),
                            { "__WORD__", type::LITERAL_TYPE.at("word") });
                    });
        }
    }
    instructions.emplace_back(make_quadruple(
        Instruction::LABEL, std::format("__{}", node["root"].to_string()), ""));
    instructions.emplace_back(make_quadruple(Instruction::FUNC_START, "", ""));
    auto tail_branch = detail::make_temporary(&temporary);
    auto block_instructions = build_from_block_statement(
        block_level, globals, block, details, true, tail_branch, &temporary);
    instructions.insert(instructions.end(),
                        block_instructions.begin(),
                        block_instructions.end());
    instructions.emplace_back(make_quadruple(Instruction::FUNC_END, "", ""));
    return instructions;
}

void build_from_vector_definition(Symbol_Table<>& symbols,
                                  Node& node,
                                  Node& details)
{
    using namespace matchit;
    assert(node["node"].to_string().compare("vector_definition") == 0);
    assert(node.has_key("left"));
    auto name = node["root"].to_string();
    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    if (right_child_node.to_deque().empty()) {
        auto rvalue =
            RValue_Parser::make_rvalue(left_child_node, details, symbols);
        auto datatype = std::get<type::RValue::Value>(rvalue.value);
        symbols.set_symbol_by_name(name, datatype);
    } else {
        if (std::cmp_not_equal(left_child_node["root"].to_int(),
                               right_child_node.to_deque().size())) {
            throw std::runtime_error("Error: invalid vector definition, "
                                     "size of vector and rvalue "
                                     "entries do not match");
        }
        std::vector<type::RValue::Value> values_at{};
        for (auto& child_node : right_child_node.array_range()) {
            auto rvalue =
                RValue_Parser::make_rvalue(child_node, details, symbols);
            auto datatype = std::get<type::RValue::Value>(rvalue.value);
            values_at.emplace_back(datatype);
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
                                        Node& details,
                                        bool ret,
                                        std::optional<Quadruple> tail_branch,
                                        std::optional<int*> temporary)
{

    using namespace matchit;
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("block") == 0);
    Instructions instructions{};
    Instructions branches{};
    auto scope_tail_branch = tail_branch;
    int scope_temp{ 0 };
    int* _temporary = temporary.value_or(&scope_temp);

    auto statements = node["left"];
    RValue_Parser parser{ details, symbols };
    for (auto& statement : statements.array_range()) {
        auto statement_type = statement["root"].to_string();
        match(statement_type)(
            pattern |
                "auto" = [&] { build_from_auto_statement(symbols, statement); },
            pattern | "extrn" =
                [&] {
                    build_from_extrn_statement(symbols, globals, statement);
                },
            pattern | "if" =
                [&] {
                    if (tail_branch.has_value()) {
                        auto [jump_instructions, if_instructions] =
                            build_from_if_statement(symbols,
                                                    globals,
                                                    tail_branch.value(),
                                                    statement,
                                                    details,
                                                    _temporary);
                        instructions.insert(instructions.end(),
                                            jump_instructions.begin(),
                                            jump_instructions.end());
                        branches.insert(branches.end(),
                                        if_instructions.begin(),
                                        if_instructions.end());
                        scope_tail_branch = detail::make_temporary(_temporary);
                        instructions.emplace_back(scope_tail_branch.value());
                    }
                },
            pattern | "while" =
                [&] {
                    if (tail_branch.has_value()) {
                        scope_tail_branch = detail::make_temporary(_temporary);
                        auto [jump_instructions, while_instructions] =
                            build_from_while_statement(
                                symbols,
                                globals,
                                scope_tail_branch.value(),
                                statement,
                                details,
                                _temporary);
                        instructions.insert(instructions.end(),
                                            jump_instructions.begin(),
                                            jump_instructions.end());
                        branches.insert(branches.end(),
                                        while_instructions.begin(),
                                        while_instructions.end());
                    }
                },
            pattern | "rvalue" =
                [&] {
                    auto rvalue_instructions = build_from_rvalue_statement(
                        symbols, statement, details, _temporary);
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
                        symbols, statement, details, _temporary);
                    instructions.insert(instructions.end(),
                                        return_instructions.begin(),
                                        return_instructions.end());
                }

        );
    }
    if (ret and tail_branch.has_value()) {
        instructions.emplace_back(make_quadruple(Instruction::LEAVE, "", ""));
    }
    instructions.insert(instructions.end(), branches.begin(), branches.end());
    return instructions;
}

/**
 * @brief Turn an rvalue into a "truthy" comparator for statement predicates
 */
std::string detail::build_from_branch_comparator_from_rvalue(
    Symbol_Table<>& symbols,

    Node& details,
    Node& block,
    Instructions& instructions,
    int* temporary)
{
    using namespace matchit;
    std::string temp_lvalue{};
    auto rvalue = RValue_Parser::make_rvalue(block, details, symbols);
    auto comparator_instructions = rvalue_node_to_list_of_ir_instructions(
                                       symbols, block, details, temporary)
                                       .first;

    match(type::get_rvalue_type_as_variant(rvalue))(
        pattern | or_(type::RValue_Type_Variant::Relation,
                      type::RValue_Type_Variant::Unary,
                      type::RValue_Type_Variant::Symbol,
                      type::RValue_Type_Variant::Value_Pointer) =
            [&] {
                instructions.insert(instructions.end(),
                                    comparator_instructions.begin(),
                                    comparator_instructions.end());
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            },
        pattern | or_(type::RValue_Type_Variant::LValue,
                      type::RValue_Type_Variant::Value) =
            [&] {
                auto rhs = std::format("{} {}",
                                       instruction_to_string(Instruction::CMP),
                                       rvalue_to_string(rvalue.value, false));
                auto temp = detail::make_temporary(temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },
        pattern | type::RValue_Type_Variant::Function =
            [&] {
                instructions.insert(instructions.end(),
                                    comparator_instructions.begin(),
                                    comparator_instructions.end());
                // instructions.pop_back();
                auto rhs = std::format("{} RET",
                                       instruction_to_string(Instruction::CMP));
                auto temp = detail::make_temporary(temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            });

    return temp_lvalue;
}

/**
 * @brief Construct an rvalue or block statement for a branch
 */

void detail::insert_rvalue_or_block_branch_instructions(
    Symbol_Table<>& symbols,
    Symbol_Table<>& globals,
    Node& block,
    Node& details,
    Quadruple const& tail_branch,
    int* temporary,
    Instructions& branch_instructions)
{
    if (block["root"].to_string() == "block") {
        auto block_instructions = build_from_block_statement(
            symbols, globals, block, details, false, tail_branch, temporary);

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
 * @brief Construct branch instructions from a while statement
 */
Branch_Instructions build_from_while_statement(Symbol_Table<>& symbols,
                                               Symbol_Table<>& globals,
                                               Quadruple const& tail_branch,
                                               Node& node,
                                               Node& details,
                                               int* temporary)
{
    using namespace matchit;
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("while") == 0);
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();

    auto start = detail::make_temporary(temporary);
    auto jump = detail::make_temporary(temporary);

    auto predicate_temp = detail::build_from_branch_comparator_from_rvalue(
        symbols, details, predicate, predicate_instructions, temporary);

    predicate_instructions.emplace_back(
        make_quadruple(Instruction::IF,
                       predicate_temp,
                       instruction_to_string(Instruction::GOTO),
                       std::get<1>(jump)));

    branch_instructions.emplace_back(jump);
    predicate_instructions.emplace_back(
        make_quadruple(Instruction::GOTO, std::get<1>(tail_branch), ""));

    detail::insert_rvalue_or_block_branch_instructions(symbols,
                                                       globals,
                                                       blocks.at(0),
                                                       details,
                                                       tail_branch,
                                                       temporary,
                                                       branch_instructions);

    branch_instructions.emplace_back(
        make_quadruple(Instruction::GOTO, std::get<1>(start), ""));
    predicate_instructions.emplace_back(tail_branch);

    return std::make_pair(predicate_instructions, branch_instructions);
}

/**
 * @brief Construct branch instructions from an if statement
 */
Branch_Instructions build_from_if_statement(Symbol_Table<>& symbols,
                                            Symbol_Table<>& globals,
                                            Quadruple const& tail_branch,
                                            Node& node,
                                            Node& details,
                                            int* temporary)
{
    using namespace matchit;
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("if") == 0);
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    RValue_Queue list{};
    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();

    auto predicate_temp = detail::build_from_branch_comparator_from_rvalue(
        symbols, details, predicate, predicate_instructions, temporary);

    auto jump = detail::make_temporary(temporary);

    predicate_instructions.emplace_back(
        make_quadruple(Instruction::IF,
                       predicate_temp,
                       instruction_to_string(Instruction::GOTO),
                       std::get<1>(jump)));

    branch_instructions.emplace_back(jump);

    detail::insert_rvalue_or_block_branch_instructions(symbols,
                                                       globals,
                                                       blocks.at(0),
                                                       details,
                                                       tail_branch,
                                                       temporary,
                                                       branch_instructions);

    branch_instructions.emplace_back(
        make_quadruple(Instruction::GOTO, std::get<1>(tail_branch), ""));

    // else statement
    if (!blocks.at(1).is_null()) {
        auto else_label = detail::make_temporary(temporary);
        predicate_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(else_label), ""));
        branch_instructions.emplace_back(else_label);
        detail::insert_rvalue_or_block_branch_instructions(symbols,
                                                           globals,
                                                           blocks.at(1),
                                                           details,
                                                           tail_branch,
                                                           temporary,
                                                           branch_instructions);
        branch_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(tail_branch), ""));
    }
    predicate_instructions.emplace_back(tail_branch);
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
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("label") == 0);
    assert(node.has_key("left"));
    Instructions instructions{};
    auto statement = node["left"];
    RValue_Parser parser{ details, symbols };
    auto label = statement.array_range().begin()->to_string();
    instructions.emplace_back(
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
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("goto") == 0);
    assert(node.has_key("left"));
    Instructions instructions{};
    RValue_Parser parser{ details, symbols };
    auto statement = node["left"];
    auto label = statement.array_range().begin()->to_string();
    if (!parser.is_defined(label)) {
        throw std::runtime_error(
            std::format("Error: label \"{}\" does not exist", label));
    }

    instructions.emplace_back(make_quadruple(Instruction::GOTO, label, ""));
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
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("return") == 0);
    Instructions instructions{};
    assert(node.has_key("left"));
    auto return_statement = node["left"];

    auto return_instructions = rvalue_node_to_list_of_ir_instructions(
        symbols, return_statement, details, temporary);

    instructions.insert(instructions.end(),
                        return_instructions.first.begin(),
                        return_instructions.first.end());

    if (!return_instructions.second.empty() and instructions.empty()) {
        auto last_rvalue = std::get<type::RValue::Type_Pointer>(
            return_instructions.second.back());
        instructions.emplace_back(make_quadruple(
            Instruction::RETURN, rvalue_to_string(*last_rvalue), ""));
    } else {
        auto last = instructions[instructions.size() - 1];
        instructions.emplace_back(
            make_quadruple(Instruction::RETURN, std::get<1>(last), ""));
    }

    return instructions;
}

void build_from_extrn_statement(Symbol_Table<>& symbols,
                                Symbol_Table<>& globals,
                                Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("extrn") == 0);
    assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        auto name = ident["root"].to_string();
        if (globals.is_defined(name)) {
            symbols.set_symbol_by_name(name, globals);
        } else {
            throw std::runtime_error(
                std::format("Error: global symbol \"{}\" not defined for "
                            "extrn statement",
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
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("auto") == 0);
    assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        match(ident["node"].to_string())(
            pattern | "lvalue" =
                [&] {
                    symbols.set_symbol_by_name(ident["root"].to_string(),
                                               type::NULL_LITERAL);
                },
            pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].to_int();
                    symbols.set_symbol_by_name(
                        ident["root"].to_string(),
                        { static_cast<type::Byte>('0'), { "byte", size } });
                },
            pattern | "indirect_lvalue" =
                [&] {
                    symbols.set_symbol_by_name(
                        ident["left"]["root"].to_string(),
                        { "__WORD__", type::LITERAL_TYPE.at("word") });
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
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("rvalue") == 0);
    RValue_Queue list{};
    assert(node.has_key("left"));
    auto statement = node["left"];

    return rvalue_node_to_list_of_ir_instructions(
               symbols, statement, details, temporary)
        .first;
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
            pattern |
                Instruction::LEAVE = [&] { os << op << ";" << std::endl; },
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