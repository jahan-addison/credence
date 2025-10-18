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
 *   A set of definitions constitute a B program
 *
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
    auto definitions = node["left"].array_range();
    for (auto& definition : definitions)
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(definition);
    for (auto& definition : definitions) {
        if (definition["node"].to_string() == "function_definition") {
            auto function_instructions =
                build_from_function_definition(definition);
            insert_instructions(instructions, function_instructions);
        }
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
    Parameters parameter_lvalues{};
    auto block = node["right"];

    symbols_.set_symbol_by_name(name, type::WORD_LITERAL);

    if (parameters.JSON_type() == util::AST_Node::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            m::match(ident["node"].to_string())(
                // cppcheck-suppress syntaxError
                m::pattern | "lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["root"].to_string());
                        symbols_.set_symbol_by_name(
                            ident["root"].to_string(), type::NULL_LITERAL);
                    },
                m::pattern | "vector_lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["root"].to_string());
                        auto size = ident["left"]["root"].to_int();
                        symbols_.set_symbol_by_name(
                            ident["root"].to_string(),
                            { static_cast<type::Byte>('0'), { "byte", size } });
                    },
                m::pattern | "indirect_lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["left"]["root"].to_string());
                        symbols_.set_symbol_by_name(
                            ident["left"]["root"].to_string(),
                            type::WORD_LITERAL);
                    });
        }
    }

    auto label = build_function_label_from_parameters(
        node["root"].to_string(), parameter_lvalues);

    instructions.emplace_back(make_quadruple(Instruction::LABEL, label));
    instructions.emplace_back(make_quadruple(Instruction::FUNC_START));

    make_root_branch();

    auto block_instructions = build_from_block_statement(block, true);

    insert_instructions(instructions, block_instructions);

    instructions.emplace_back(make_quadruple(Instruction::FUNC_END));

    // clear symbols from function scope
    symbols_.clear();

    return instructions;
}

std::string ITA::build_function_label_from_parameters(
    std::string_view name,
    Parameters const& parameters)
{
    auto label = std::format("__{}", name);
    label.append("(");
    if (!parameters.empty()) {
        for (auto it = parameters.begin(); it != parameters.end(); it++) {
            label.append(*it);
            if (it != parameters.end() - 1)
                label.append(",");
        }
    }
    label.append(")");
    return label;
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
            credence_runtime_error(
                "invalid vector definition, "
                "size of vector and assignment "
                "entries do not match",
                name,
                internal_symbols_);
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
        instructions.emplace_back(make_quadruple(Instruction::LEAVE));
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
            make_quadruple(Instruction::GOTO, std::get<1>(jump)));
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
            make_quadruple(Instruction::GOTO, std::get<1>(jump)));
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
            make_quadruple(Instruction::GOTO, std::get<1>(else_label)));
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
        make_quadruple(Instruction::LABEL, std::format("__L{}", label), ""));
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
    if (!parser.is_defined(label))
        credence_error(
            std::format("Error: label \"{}\" does not exist", label));

    instructions.emplace_back(
        make_quadruple(Instruction::GOTO, std::format("__L{}", label), ""));
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
            make_quadruple(Instruction::RETURN, std::get<1>(last)));
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
            credence_runtime_error(
                "symbol not defined in global scope", name, internal_symbols_);
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
                    auto name = ident["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        credence_runtime_error(
                            "identifier is already defined in auto declaration",
                            name,
                            internal_symbols_);

#endif
                    symbols_.set_symbol_by_name(
                        ident["root"].to_string(), type::NULL_LITERAL);
                },
            m::pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].to_int();
                    auto name = ident["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        credence_runtime_error(
                            "identifier is already defined in auto declaration",
                            name,
                            internal_symbols_);

#endif
                    symbols_.set_symbol_by_name(
                        ident["root"].to_string(),
                        { static_cast<type::Byte>('0'), { "byte", size } });
                },
            m::pattern | "indirect_lvalue" =
                [&] {
                    auto name = ident["left"]["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        credence_runtime_error(
                            "identifier is already defined in auto declaration",
                            name,
                            internal_symbols_);

#endif
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

/**
 * @brief Emit a single qaudrupl-tuple to a std::ostream
 *   If indent is true indent with a tab for formatting
 */
void ITA::emit_to(std::ostream& os, ITA::Quadruple const& ita, bool indent)
{ // not constexpr until C++23
    ITA::Instruction op = std::get<ITA::Instruction>(ita);
    const std::initializer_list<ITA::Instruction> lhs_instruction = {
        ITA::Instruction::GOTO,
        ITA::Instruction::PUSH,
        ITA::Instruction::LABEL,
        ITA::Instruction::POP,
        ITA::Instruction::CALL
    };
    if (std::ranges::find(lhs_instruction, op) != lhs_instruction.end()) {
        if (op == ITA::Instruction::LABEL) {
            os << std::get<1>(ita) << ":" << std::endl;
        } else {
            if (indent)
                os << "    ";
            os << op << " " << std::get<1>(ita) << ";" << std::endl;
        }
    } else {
        if (indent) {
            if (op != ITA::Instruction::FUNC_START and
                op != ITA::Instruction::FUNC_END)
                os << "    ";
        }
        m::match(op)(
            m::pattern | ITA::Instruction::RETURN =
                [&] {
                    os << op << " " << std::get<1>(ita) << ";" << std::endl;
                },
            m::pattern |
                ITA::Instruction::LEAVE = [&] { os << op << ";" << std::endl; },
            m::pattern | m::or_(ITA::Instruction::IF, ITA::Instruction::JMP_E) =
                [&] {
                    os << op << " " << std::get<1>(ita) << " "
                       << std::get<2>(ita) << " " << std::get<3>(ita) << ";"
                       << std::endl;
                },
            m::pattern | m::_ =
                [&] {
                    os << std::get<1>(ita) << " " << op << " "
                       << std::get<2>(ita) << std::get<3>(ita) << ";"
                       << std::endl;
                    if (indent and op == ITA::Instruction::FUNC_END)
                        os << std::endl << std::endl;
                }

        );
    }
}

/**
 * @brief Create a statement AST from an rvalue statement or others
 */
inline util::AST_Node ITA::make_block_statement(
    std::deque<util::AST_Node> const& blocks)
{
    auto block_statement = util::AST::object();
    block_statement["node"] = util::AST_Node{ "statement" };
    block_statement["root"] = util::AST_Node{ "block" };
    block_statement["left"] = util::AST_Node{ blocks };

    return block_statement;
}

/**
 * @brief Create a statement AST from an rvalue statement or others
 */
inline util::AST_Node ITA::make_block_statement(util::AST_Node block)
{
    auto block_statement = util::AST::object();
    block_statement["node"] = util::AST_Node{ "statement" };
    block_statement["root"] = util::AST_Node{ "block" };
    block_statement["left"].append(block);
    return block_statement;
}
/**
 * @brief Use operator<< to implement nstruction symbol to string
 */
inline std::string ITA::instruction_to_string(
    Instruction op) // not constexpr until C++23
{
    std::ostringstream os;
    os << op;
    return os.str();
}

/**
 * @brief Use std::ostringstream to implement qaudruple-tuple to string
 */
inline std::string ITA::quadruple_to_string(
    ITA::Quadruple const& ita) // not constexpr until C++23
{
    std::ostringstream os;
    os << std::setw(2) << std::get<1>(ita) << std::get<0>(ita)
       << std::get<2>(ita) << std::get<3>(ita);

    return os.str();
}

/**
 * @brief Check if AST node is a branch statement node
 */
constexpr inline bool ITA::Branch::is_branching_statement(std::string_view s)
{
    return std::ranges::find(BRANCH_STATEMENTS, s) != BRANCH_STATEMENTS.end();
}

/**
 * @brief Check if a Quadruple instruction is Instruction::GOTO
 */
constexpr inline bool ITA::Branch::last_instruction_is_jump(
    Quadruple const& inst)
{
    return std::get<0>(inst) == Instruction::GOTO;
}

/**
 * @brief Increment branch level, create return label and add to branch stack
 */
inline void ITA::Branch::increment_branch_level()
{
    is_branching = true;
    level++;
    block_level = ITA::make_temporary(temporary);
    stack.emplace(block_level);
}

/**
 * @brief Decrement branch level and pop branch label off the stack
 */
inline void ITA::Branch::decrement_branch_level(bool not_branching)
{
    CREDENCE_ASSERT(level > 1);
    CREDENCE_ASSERT(!stack.empty());
    level--;
    if (not_branching)
        is_branching = false;
    stack.pop();
}

/**
 * @brief Set branching to false, branching is complete
 */
inline void ITA::Branch::teardown()
{
    CREDENCE_ASSERT_EQUAL(level, 1);
    is_branching = false;
}

/**
 * @brief Get a parent branch or the root branch label from the stack
 */
inline ITA::Tail_Branch ITA::Branch::get_parent_branch(bool last)
{
    CREDENCE_ASSERT(root_branch.has_value());
    if (last and stack.size() > 1) {
        auto top = stack.top();
        stack.pop();
        auto previous = stack.top();
        stack.emplace(top);
        return previous;
    } else
        return stack.empty() ? root_branch : stack.top();
}

} // namespace ir

} // namespace credence