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

#include <credence/ir/ita.h>

#include <cassert>            // for assert
#include <credence/ir/temp.h> // for rvalue_node_to_list_of_temp_instructions
#include <credence/queue.h>   // for rvalue_to_string, RValue_Queue
#include <credence/rvalue.h>  // for RValue_Parser
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for RValue, RValue_Type_Variant, WORD_LITERAL
#include <format>             // for format
#include <list>               // for list
#include <matchit.h>          // for pattern, PatternHelper, PatternPipable
#include <memory>             // for shared_ptr
#include <simplejson.h>       // for JSON, object
#include <stdexcept>          // for runtime_error
#include <utility>            // for pair, get, make_pair, cmp_not_equal
#include <variant>            // for get, monostate, variant

namespace credence {

namespace ir {

/**
 * @brief Construct a set of ita instructions from a set of definitions
 */
ITA::Instructions ITA::build_from_definitions(Node& node)
{

    assert(node["root"].to_string().compare("definitions") == 0);
    Instructions instructions{};
    auto definitions = node["left"];
    // vector definitions
    for (auto& definition : definitions.array_range())
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(definition);
    // function definitions
    for (auto& definition : definitions.array_range()) {
        m::match(definition["node"].to_string())(
            // cppcheck-suppress syntaxError
            m::pattern | "function_definition" = [&] {
                auto function_instructions =
                    build_from_function_definition(definition);
                insert_instructions(instructions, function_instructions);
            });
    }
    insert_instructions(instructions_, instructions);
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a function definition
 */
ITA::Instructions ITA::build_from_function_definition(Node& node)
{
    assert(node["node"].to_string().compare("function_definition") == 0);
    Instructions instructions{};
    Symbol_Table<> block_level{};
    auto name = node["root"].to_string();
    auto parameters = node["left"];
    auto block = node["right"];

    symbols_.set_symbol_by_name(name, type::WORD_LITERAL);

    if (parameters.JSON_type() == json::JSON::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            m::match(ident["node"].to_string())(
                m::pattern | "lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(
                            ident["root"].to_string(), type::NULL_LITERAL);
                    },
                m::pattern | "vector_lvalue" =
                    [&] {
                        auto size = ident["left"]["root"].to_int();
                        block_level.set_symbol_by_name(
                            ident["root"].to_string(),
                            { static_cast<type::Byte>('0'), { "byte", size } });
                    },
                m::pattern | "indirect_lvalue" =
                    [&] {
                        block_level.set_symbol_by_name(
                            ident["left"]["root"].to_string(),
                            type::WORD_LITERAL);
                    });
        }
    }
    instructions.emplace_back(make_quadruple(
        Instruction::LABEL, std::format("__{}", node["root"].to_string()), ""));

    instructions.emplace_back(make_quadruple(Instruction::FUNC_START, "", ""));

    tail_branch = make_temporary(&temporary);
    temporary = 0; // reset
    auto block_instructions = build_from_block_statement(block, true);
    insert_instructions(instructions, block_instructions);

    instructions.emplace_back(make_quadruple(Instruction::FUNC_END, "", ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a vector definition
 */
void ITA::build_from_vector_definition(Node& node)
{
    assert(node["node"].to_string().compare("vector_definition") == 0);
    assert(node.has_key("left"));
    auto name = node["root"].to_string();
    auto left_child_node = node["left"];
    auto right_child_node = node["right"];
    if (right_child_node.to_deque().empty()) {
        auto rvalue = RValue_Parser::make_rvalue(
            left_child_node, internal_symbols_, symbols_);
        auto datatype = std::get<type::RValue::Value>(rvalue.value);
        symbols_.set_symbol_by_name(name, datatype);
    } else {
        if (std::cmp_not_equal(
                left_child_node["root"].to_int(),
                right_child_node.to_deque().size())) {
            throw std::runtime_error(
                "Error: invalid vector definition, "
                "size of vector and rvalue "
                "entries do not match");
        }
        std::vector<type::RValue::Value> values_at{};
        for (auto& child_node : right_child_node.array_range()) {
            auto rvalue = RValue_Parser::make_rvalue(
                child_node, internal_symbols_, symbols_);
            auto datatype = std::get<type::RValue::Value>(rvalue.value);
            values_at.emplace_back(datatype);
        }
        symbols_.set_symbol_by_name(name, values_at);
    }
}

/**
 * @brief Jump to resume ITA instructions from a branching tree.
 */
void ITA::return_and_resume_instructions(
    Instructions& instructions,
    std::string_view from)
{
    if (is_branching) {
        auto branching = std::ranges::find(BRANCH_STATEMENTS, from);
        if (branching == BRANCH_STATEMENTS.end()) {
            instructions.emplace_back(tail_branch.value());
            tail_branch = make_temporary(&temporary);
        }
    }
}

/**
 * @brief Jump to LEAVE ITA instruction from end of a block statement
 */
void ITA::return_and_resume_instructions(Instructions& instructions)
{
    if (is_branching) {
        instructions.emplace_back(tail_branch.value());
        is_branching = false;
    }
}

/**
 * @brief Construct a set of ita instructions from a block statement
 */
ITA::Instructions ITA::build_from_block_statement(Node& node, bool ret)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("block") == 0);

    Instructions instructions{};
    Instructions branches{};

    auto block_tail_branch = tail_branch;
    auto statements = node["left"];
    RValue_Parser parser{ internal_symbols_, symbols_ };
    for (auto& statement : statements.array_range()) {
        auto statement_type = statement["root"].to_string();
        return_and_resume_instructions(instructions, statement_type);
        m::match(statement_type)(
            m::pattern | "auto" = [&] { build_from_auto_statement(statement); },
            m::pattern |
                "extrn" = [&] { build_from_extrn_statement(statement); },
            m::pattern | "if" =
                [&] {
                    auto [jump_instructions, if_instructions] =
                        build_from_if_statement(block_tail_branch, statement);
                    is_branching = true;
                    insert_instructions(instructions, jump_instructions);
                    insert_instructions(branches, if_instructions);
                },
            m::pattern | "switch" =
                [&] {
                    auto [jump_instructions, switch_statements] =
                        build_from_switch_statement(
                            block_tail_branch, statement);
                    is_branching = true;
                    insert_instructions(instructions, jump_instructions);
                    insert_instructions(branches, switch_statements);
                },
            m::pattern | "while" =
                [&] {
                    auto [jump_instructions, while_instructions] =
                        build_from_while_statement(
                            block_tail_branch, statement);
                    is_branching = true;
                    insert_instructions(instructions, jump_instructions);
                    insert_instructions(branches, while_instructions);
                },
            m::pattern | "rvalue" =
                [&] {
                    auto rvalue_instructions =
                        build_from_rvalue_statement(statement);
                    insert_instructions(instructions, rvalue_instructions);
                },
            m::pattern | "label" =
                [&] {
                    auto label_instructions =
                        build_from_label_statement(statement);
                    insert_instructions(instructions, label_instructions);
                },
            m::pattern | "goto" =
                [&] {
                    auto goto_instructions =
                        build_from_goto_statement(statement);
                    insert_instructions(instructions, goto_instructions);
                },
            m::pattern | "return" =
                [&] {
                    auto return_instructions =
                        build_from_return_statement(statement);
                    insert_instructions(instructions, return_instructions);
                }

        );
    }
    if (ret and tail_branch.has_value()) {
        return_and_resume_instructions(instructions);
        instructions.emplace_back(make_quadruple(Instruction::LEAVE, "", ""));
    }
    insert_instructions(instructions, branches);
    return instructions;
}

/**
 * @brief Turn an rvalue into a "truthy" comparator for statement predicates
 */
std::string ITA::build_from_branch_comparator_rvalue(
    Node& block,
    Instructions& instructions)
{
    std::string temp_lvalue{};
    auto rvalue =
        RValue_Parser::make_rvalue(block, internal_symbols_, symbols_);
    auto comparator_instructions =
        rvalue_node_to_list_of_temp_instructions(
            symbols_, block, internal_symbols_, &temporary)
            .first;

    m::match(type::get_rvalue_type_as_variant(rvalue))(
        m::pattern | m::or_(
                         type::RValue_Type_Variant::Relation,
                         type::RValue_Type_Variant::Unary,
                         type::RValue_Type_Variant::Symbol,
                         type::RValue_Type_Variant::Value_Pointer) =
            [&] {
                insert_instructions(instructions, comparator_instructions);
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            },
        m::pattern | m::or_(
                         type::RValue_Type_Variant::LValue,
                         type::RValue_Type_Variant::Value) =
            [&] {
                auto rhs = std::format(
                    "{} {}",
                    instruction_to_string(Instruction::CMP),
                    rvalue_to_string(rvalue.value, false));
                auto temp = make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },
        m::pattern | type::RValue_Type_Variant::Function =
            [&] {
                insert_instructions(instructions, comparator_instructions);
                // instructions.pop_back();
                auto rhs = std::format(
                    "{} RET", instruction_to_string(Instruction::CMP));
                auto temp = make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            });

    return temp_lvalue;
}

/**
 * @brief Construct an rvalue or block statement for a branch
 */
void ITA::insert_rvalue_or_block_branch_instructions(
    Node& block,
    Instructions& branch_instructions)
{
    if (block["root"].to_string() == "block") {
        auto block_instructions = build_from_block_statement(block, false);

        insert_instructions(branch_instructions, block_instructions);

    } else {
        auto block_statement = json::object();
        block_statement["node"] = json::JSON{ "statement" };
        block_statement["root"] = json::JSON{ "block" };
        block_statement["left"].append(block);
        auto block_instructions =
            build_from_block_statement(block_statement, false);
        insert_instructions(branch_instructions, block_instructions);
    }
}

/**
 * @brief Construct a set of ita instructions from a switch statement
 */
ITA::Branch_Instructions ITA::build_from_switch_statement(
    Tail_Branch& block_level,
    Node& node)
{
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    auto predicate = node["left"];
    auto blocks = node["right"];

    auto predicate_temp =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);

    for (auto const& block : blocks.array_range()) {
        assert(block["node"].to_string().compare("statement") == 0);
        assert(block["root"].to_string().compare("case") == 0);

        auto jump = make_temporary(&temporary);
        auto statements = block["right"].to_deque();
        // the case predicate is always a constant literal
        auto constant_literal = RValue_Parser::make_rvalue(
            block["left"], internal_symbols_, symbols_);
        predicate_instructions.emplace_back(make_quadruple(
            Instruction::IF_E,
            predicate_temp,
            rvalue_to_string(constant_literal.value, false),
            std::get<1>(jump)));
        branch_instructions.emplace_back(jump);
        insert_rvalue_or_block_branch_instructions(
            statements.front(), branch_instructions);
        if (statements.size() > 1) {
            // break statement
            branch_instructions.emplace_back(make_quadruple(
                Instruction::GOTO, std::get<1>(block_level.value()), ""));
        }
    }
    if (!is_branching)
        predicate_instructions.emplace_back(make_quadruple(
            Instruction::GOTO, std::get<1>(block_level.value()), ""));
    return std::make_pair(predicate_instructions, branch_instructions);
}

/**
 * @brief Construct branch instructions from a while statement
 */
ITA::Branch_Instructions ITA::build_from_while_statement(
    Tail_Branch& block_level,
    Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("while") == 0);
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();
    auto start = make_temporary(&temporary);
    auto jump = make_temporary(&temporary);
    auto predicate_temp =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);

    predicate_instructions.push_back(start);
    predicate_instructions.emplace_back(make_quadruple(
        Instruction::IF,
        predicate_temp,
        instruction_to_string(Instruction::GOTO),
        std::get<1>(jump)));
    branch_instructions.emplace_back(jump);
    predicate_instructions.emplace_back(make_quadruple(
        Instruction::GOTO, std::get<1>(block_level.value()), ""));

    insert_rvalue_or_block_branch_instructions(
        blocks.at(0), branch_instructions);

    if (!is_branching)
        branch_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(start), ""));
    return std::make_pair(predicate_instructions, branch_instructions);
}

/**
 * @brief Construct branch instructions from an if statement
 */
ITA::Branch_Instructions ITA::build_from_if_statement(
    Tail_Branch& block_level,
    Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("if") == 0);
    Instructions predicate_instructions{};
    Instructions branch_instructions{};
    RValue_Queue list{};
    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();

    auto predicate_temp =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);

    auto jump = make_temporary(&temporary);

    predicate_instructions.emplace_back(make_quadruple(
        Instruction::IF,
        predicate_temp,
        instruction_to_string(Instruction::GOTO),
        std::get<1>(jump)));

    branch_instructions.emplace_back(jump);

    insert_rvalue_or_block_branch_instructions(
        blocks.at(0), branch_instructions);
    if (!is_branching)
        branch_instructions.emplace_back(make_quadruple(
            Instruction::GOTO, std::get<1>(block_level.value()), ""));

    // else statement
    if (!blocks.at(1).is_null()) {
        auto else_label = make_temporary(&temporary);
        predicate_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(else_label), ""));
        branch_instructions.emplace_back(else_label);
        insert_rvalue_or_block_branch_instructions(
            blocks.at(1), branch_instructions);
        if (!is_branching)
            branch_instructions.emplace_back(make_quadruple(
                Instruction::GOTO, std::get<1>(block_level.value()), ""));
    }
    return std::make_pair(predicate_instructions, branch_instructions);
}

/**
 * @brief Construct a set of ita instructions from a label statement
 */
ITA::Instructions ITA::build_from_label_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("label") == 0);
    assert(node.has_key("left"));
    Instructions instructions{};
    auto statement = node["left"];
    RValue_Parser parser{ internal_symbols_, symbols_ };
    auto label = statement.to_deque().front().to_string();
    instructions.emplace_back(
        make_quadruple(Instruction::LABEL, std::format("_L_{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a goto statement
 */
ITA::Instructions ITA::build_from_goto_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("goto") == 0);
    assert(node.has_key("left"));
    Instructions instructions{};
    RValue_Parser parser{ internal_symbols_, symbols_ };
    auto statement = node["left"];
    auto label = statement.to_deque().front().to_string();
    if (!parser.is_defined(label)) {
        throw std::runtime_error(
            std::format("Error: label \"{}\" does not exist", label));
    }

    instructions.emplace_back(make_quadruple(Instruction::GOTO, label, ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a block statement
 */
ITA::Instructions ITA::build_from_return_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("return") == 0);
    Instructions instructions{};
    assert(node.has_key("left"));
    auto return_statement = node["left"];

    auto return_instructions = rvalue_node_to_list_of_temp_instructions(
        symbols_, return_statement, internal_symbols_, &temporary);

    instructions.insert(
        instructions.end(),
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

/**
 * @brief Symbol construction from extrn declaration statements
 */
void ITA::build_from_extrn_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("extrn") == 0);
    assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        auto name = ident["root"].to_string();
        if (globals_.is_defined(name)) {
            symbols_.set_symbol_by_name(name, globals_);
        } else {
            throw std::runtime_error(
                std::format(
                    "Error: global symbol \"{}\" not defined for "
                    "extrn statement",
                    name));
        }
    }
}

/**
 * @brief Symbol construction from auto declaration statements
 */
void ITA::build_from_auto_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("auto") == 0);
    assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        m::match(ident["node"].to_string())(
            m::pattern | "lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(
                        ident["root"].to_string(), type::NULL_LITERAL);
                },
            m::pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].to_int();
                    symbols_.set_symbol_by_name(
                        ident["root"].to_string(),
                        { static_cast<type::Byte>('0'), { "byte", size } });
                },
            m::pattern | "indirect_lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(
                        ident["left"]["root"].to_string(), type::WORD_LITERAL);
                });
    }
}

/**
 * @brief Construct a set of ita instructions from an rvalue statement
 */
ITA::Instructions ITA::build_from_rvalue_statement(Node& node)
{
    assert(node["node"].to_string().compare("statement") == 0);
    assert(node["root"].to_string().compare("rvalue") == 0);
    RValue_Queue list{};
    assert(node.has_key("left"));
    auto statement = node["left"];

    return rvalue_node_to_list_of_temp_instructions(
               symbols_, statement, internal_symbols_, &temporary)
        .first;
}

} // namespace ir

} // namespace credence