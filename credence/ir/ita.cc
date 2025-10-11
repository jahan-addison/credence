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

#include <credence/assert.h>  // for CREDENCE_ASSERT_NODE, CREDENCE_ASSERT
#include <credence/ir/temp.h> // for rvalue_node_to_list_of_temp_instructions
#include <credence/queue.h>   // for rvalue_to_string, RValue_Queue
#include <credence/rvalue.h>  // for RValue_Parser
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for RValue, RValue_Type_Variant, WORD_LITERAL
#include <credence/util.h>    // for AST_Node
#include <format>             // for format
#include <matchit.h>          // for pattern, PatternHelper, PatternPipable
#include <memory>             // for shared_ptr
#include <simplejson.h>       // for JSON, object
#include <utility>            // for pair, get, make_pair, cmp_not_equal
#include <variant>            // for get, monostate, variant

namespace credence {

namespace ir {

/**
 * @brief Construct a set of ita instructions from a set of definitions
 *   Definition grammar:
 *
 *     definition : function_definition
 *         | vector_definition

 *  Note that vector definitions are scanned first
 */
ITA::Instructions ITA::build_from_definitions(Node const& node)
{

    CREDENCE_ASSERT_NODE(node["root"].to_string(), "definitions");
    Instructions instructions{};
    auto definitions = node["left"];
    for (auto& definition : definitions.array_range())
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(definition);
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
ITA::Instructions ITA::build_from_function_definition(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "function_definition");
    Instructions instructions{};
    auto name = node["root"].to_string();
    auto parameters = node["left"];
    std::vector<std::string> p_lvalues{};
    auto block = node["right"];

    symbols_.set_symbol_by_name(name, type::WORD_LITERAL);

    if (parameters.JSON_type() == util::AST_Node::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            m::match(ident["node"].to_string())(
                m::pattern | "lvalue" =
                    [&] {
                        p_lvalues.emplace_back(ident["root"].to_string());
                        symbols_.set_symbol_by_name(
                            ident["root"].to_string(), type::NULL_LITERAL);
                    },
                m::pattern | "vector_lvalue" =
                    [&] {
                        p_lvalues.emplace_back(ident["root"].to_string());
                        auto size = ident["left"]["root"].to_int();
                        symbols_.set_symbol_by_name(
                            ident["root"].to_string(),
                            { static_cast<type::Byte>('0'), { "byte", size } });
                    },
                m::pattern | "indirect_lvalue" =
                    [&] {
                        p_lvalues.emplace_back(
                            ident["left"]["root"].to_string());
                        symbols_.set_symbol_by_name(
                            ident["left"]["root"].to_string(),
                            type::WORD_LITERAL);
                    });
        }
    }
    instructions.emplace_back(make_quadruple(
        Instruction::LABEL, std::format("__{}", node["root"].to_string()), ""));

    instructions.emplace_back(make_quadruple(Instruction::FUNC_START, "", ""));
    temporary = 0;
    branch.set_root_branch(*this);
    auto block_instructions = build_from_block_statement(block, true);
    insert_instructions(instructions, block_instructions);

    instructions.emplace_back(make_quadruple(Instruction::FUNC_END, "", ""));

    // clear parameter symbols from scope
    for (auto const& name : p_lvalues) {
        symbols_.remove_symbol_by_name(name);
    }

    return instructions;
}

/**
 * @brief Construct ita instructions from a vector definition
 */
void ITA::build_from_vector_definition(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "vector_definition");
    CREDENCE_ASSERT(node.has_key("left"));
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
            credence_error(
                "Error: invalid vector definition, "
                "size of vector and value "
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
 * @brief Setup branch state and label stack based on statement type
 */
void ITA::build_statement_setup_branches(
    std::string_view type,
    Instructions& instructions)
{
    if (branch.is_branching_statement(type)) {
        branch.increment_branch_level();
        instructions.emplace_back(branch.stack.top().value());
    }
}

/**
 * @brief Teardown branch state and jump to resume from label on stack
 */
void ITA::build_statement_teardown_branches(
    std::string_view type,
    Instructions& instructions)
{
    if (branch.is_branching_statement(type)) {
        bool lookbehind = type == "while";
        if (instructions.empty() or
            not branch.last_instruction_is_jump(instructions.back()))
            instructions.emplace_back(make_quadruple(
                Instruction::GOTO,
                std::get<1>(branch.get_parent_branch(lookbehind).value()),
                ""));
        // if (type == "if")
        branch.stack.pop();
        branch.decrement_branch_level(true);
    }
}

/**
 *
 * @brief Construct a set of ita instructions from a block statement
 */
ITA::Instructions ITA::build_from_block_statement(
    Node const& node,
    bool root_function_scope)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "block");

    auto [instructions, branches] = make_statement_instructions();
    auto statements = node["left"];

    for (auto& statement : statements.array_range()) {
        auto statement_type = statement["root"].to_string();

        build_statement_setup_branches(statement_type, instructions);

        m::match(statement_type)(
            m::pattern | "auto" = [&] { build_from_auto_statement(statement); },
            m::pattern |
                "extrn" = [&] { build_from_extrn_statement(statement); },
            m::pattern | "if" =
                [&] {
                    auto [jump_instructions, if_instructions] =
                        build_from_if_statement(statement);
                    insert_instructions(instructions, jump_instructions);
                    insert_instructions(branches, if_instructions);
                },
            m::pattern | "switch" =
                [&] {
                    auto [jump_instructions, switch_statements] =
                        build_from_switch_statement(statement);
                    insert_instructions(instructions, jump_instructions);
                    insert_instructions(branches, switch_statements);
                },
            m::pattern | "while" =
                [&] {
                    auto [jump_instructions, while_instructions] =
                        build_from_while_statement(statement);
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

        build_statement_teardown_branches(statement_type, branches);
    }

    if (root_function_scope) {
        branch.teardown();
        instructions.emplace_back(branch.get_root_branch().value());
        instructions.emplace_back(make_quadruple(Instruction::LEAVE, "", ""));
    }

    insert_instructions(instructions, branches);
    return instructions;
}

/**
 * @brief Insert the jump statement at the top of the predicate instruction
 * set, and push the GOTO to resume at the end of the branch instructions
 *
 * Note that generally the build_from_block_statement add
 * the GOTO, we add it here during stacks of branches
 */
void ITA::insert_branch_jump_and_resume_instructions(
    Node const& block,
    Instructions& predicate_instructions,
    Instructions& branch_instructions,
    Quadruple const& label,
    Tail_Branch const& tail)
{
    predicate_instructions.emplace_back(make_quadruple(
        Instruction::IF,
        build_from_branch_comparator_rvalue(block, predicate_instructions),
        instruction_to_string(Instruction::GOTO),
        std::get<1>(label)));

    if (branch.stack.size() > 2) {
        auto jump = tail.value_or(branch.get_parent_branch(true).value());
        branch_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(jump), ""));
    }
}

/**
 * @brief Construct block statement ita instructions for a branch
 */
void ITA::insert_branch_block_instructions(
    Node const& block,
    Instructions& branch_instructions)
{
    if (block["root"].to_string() == "block") {
        auto block_instructions = build_from_block_statement(block, false);
        insert_instructions(branch_instructions, block_instructions);

    } else {
        auto block_statement = make_block_statement(block);
        insert_branch_block_instructions(block_statement, branch_instructions);
    }
}

/**
 * @brief Turn an rvalue into a "truthy" comparator for statement predicates
 */
std::string ITA::build_from_branch_comparator_rvalue(
    Node const& block,
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
                auto rhs = std::format(
                    "{} RET", instruction_to_string(Instruction::CMP));
                auto temp = make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },
        m::pattern | m::_ =
            [&] {
                insert_instructions(instructions, comparator_instructions);
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            });

    return temp_lvalue;
}

/**
 * @brief Construct ita instructions from a case statement in a switch
 */
ITA::Branch_Instructions ITA::build_from_case_statement(
    Node const& node,
    std::string const& switch_label,
    Tail_Branch const& tail)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "case");

    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    bool break_statement = false;
    auto jump = make_temporary();
    auto statements = node["right"].to_deque();
    auto case_statement = make_block_statement(statements);

    auto condition =
        RValue_Parser::make_rvalue(node["left"], internal_symbols_, symbols_);

    predicate_instructions.emplace_back(make_quadruple(
        Instruction::JMP_E,
        switch_label,
        rvalue_to_string(condition.value, false),
        std::get<1>(jump)));
    if (branch.stack.size() > 2) {
        auto jump = tail.value_or(branch.get_parent_branch(true).value());
        branch_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(jump), ""));
    }
    branch_instructions.emplace_back(jump);

    // resolve all blocks in the statement
    if (!statements.empty() and
        statements.back()["root"].to_string() == "break") {
        break_statement = true;
        statements.pop_back();
    }

    insert_branch_block_instructions(case_statement, branch_instructions);

    if (break_statement)
        if (branch_instructions.empty() or
            !branch.last_instruction_is_jump(branch_instructions.back()))
            branch_instructions.emplace_back(make_quadruple(
                Instruction::GOTO,
                std::get<1>(branch.get_parent_branch().value()),
                ""));

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from a switch statement
 */
ITA::Branch_Instructions ITA::build_from_switch_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "switch");
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    auto predicate = node["left"];
    auto blocks = node["right"];
    // get the parent label of the switch statement
    auto tail = branch.get_parent_branch();
    auto cases = std::stack<Tail_Branch>{};
    auto switch_label =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);
    branch.stack.emplace(tail);
    for (auto& statement : blocks.array_range()) {
        auto start = make_temporary();
        branch.stack.emplace(start);
        auto [jump_instructions, case_statements] =
            build_from_case_statement(statement, switch_label, tail);
        cases.emplace(branch.stack.top());
        insert_instructions(predicate_instructions, jump_instructions);
        insert_instructions(branch_instructions, case_statements);
        branch.stack.pop();
    }
    while (!cases.empty()) {
        auto label = cases.top();
        predicate_instructions.emplace_back(label.value());
        cases.pop();
    }
    branch.stack.pop();
    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from a while statement
 */
ITA::Branch_Instructions ITA::build_from_while_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "while");

    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();

    auto tail = branch.get_parent_branch(true);
    auto jump = make_temporary();
    auto start = make_temporary();

    branch.stack.emplace(start);

    predicate_instructions.emplace_back(start);

    insert_branch_jump_and_resume_instructions(
        predicate, predicate_instructions, branch_instructions, jump, tail);

    branch_instructions.emplace_back(jump);

    insert_branch_block_instructions(blocks.at(0), branch_instructions);

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from an if statement
 */
ITA::Branch_Instructions ITA::build_from_if_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "if");

    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();

    auto predicate = node["left"];
    auto blocks = node["right"].to_deque();

    auto start = make_temporary();
    auto jump = make_temporary();

    insert_branch_jump_and_resume_instructions(
        predicate, predicate_instructions, branch_instructions, jump);

    branch_instructions.emplace_back(jump);
    branch.stack.emplace(start);

    insert_branch_block_instructions(blocks.at(0), branch_instructions);

    // no else statement
    if (blocks.at(1).is_null())
        predicate_instructions.emplace_back(start);

    // else statement
    if (!blocks.at(1).is_null()) {
        auto else_label = make_temporary();
        if (!branch.last_instruction_is_jump(branch_instructions.back()))
            branch_instructions.emplace_back(make_quadruple(
                Instruction::GOTO,
                std::get<1>(branch.get_parent_branch().value()),
                ""));
        predicate_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(else_label), ""));
        branch_instructions.emplace_back(else_label);
        insert_branch_block_instructions(blocks.at(1), branch_instructions);
        predicate_instructions.emplace_back(start);
    }

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from a label statement
 */
ITA::Instructions ITA::build_from_label_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "label");
    CREDENCE_ASSERT(node.has_key("left"));
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
ITA::Instructions ITA::build_from_goto_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "goto");
    CREDENCE_ASSERT(node.has_key("left"));
    Instructions instructions{};
    RValue_Parser parser{ internal_symbols_, symbols_ };
    auto statement = node["left"];
    auto label = statement.to_deque().front().to_string();
    if (!parser.is_defined(label)) {
        credence_error(
            std::format("Error: label \"{}\" does not exist", label));
    }

    instructions.emplace_back(make_quadruple(Instruction::GOTO, label, ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a block statement
 */
ITA::Instructions ITA::build_from_return_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "return");
    CREDENCE_ASSERT(node.has_key("left"));
    Instructions instructions{};
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
void ITA::build_from_extrn_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "extrn");
    CREDENCE_ASSERT(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        auto name = ident["root"].to_string();
        if (globals_.is_defined(name)) {
            symbols_.set_symbol_by_name(name, globals_);
        } else {
            credence_error(
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
void ITA::build_from_auto_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "auto");
    CREDENCE_ASSERT(node.has_key("left"));
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
ITA::Instructions ITA::build_from_rvalue_statement(Node const& node)
{
    CREDENCE_ASSERT_NODE(node["node"].to_string(), "statement");
    CREDENCE_ASSERT_NODE(node["root"].to_string(), "rvalue");
    CREDENCE_ASSERT(node.has_key("left"));
    RValue_Queue list{};
    auto statement = node["left"];

    return rvalue_node_to_list_of_temp_instructions(
               symbols_, statement, internal_symbols_, &temporary)
        .first;
}

} // namespace ir

} // namespace credence