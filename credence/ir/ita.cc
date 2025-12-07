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

#include <algorithm>               // for __find, find
#include <credence/error.h>        // for assert_equal_impl, credence_asser...
#include <credence/expression.h>   // for Expression_Parser
#include <credence/ir/temporary.h> // for expression_node_to_temporary_inst...
#include <credence/symbol.h>       // for Symbol_Table
#include <credence/types.h>        // for get_unary_operator, is_unary_expr...
#include <credence/util.h>         // for AST_Node
#include <credence/values.h>       // for Expression, expression_type_to_st...
#include <fmt/format.h>            // for format
#include <initializer_list>        // for initializer_list
#include <matchit.h>               // for pattern, PatternHelper, PatternPi...
#include <memory>                  // for shared_ptr
#include <simplejson.h>            // for JSON
#include <utility>                 // for get, pair, cmp_not_equal
#include <variant>                 // for get, monostate, variant

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Get the rvalue and unary operator from an ita MOV instruction
 */
std::pair<std::string, std::string> get_rvalue_from_mov_qaudruple(
    Quadruple const& instruction)
{
    std::string rvalue{};
    std::string unary{};

    auto r2 = std::get<2>(instruction);
    auto r3 = std::get<3>(instruction);

    if (type::is_unary_expression(r2))
        unary = type::get_unary_operator(r2);
    if (!r2.empty())
        rvalue += r2;

    if (type::is_unary_expression(r3))
        unary = type::get_unary_operator(r3);
    if (!r3.empty())
        rvalue += r3;

    return { rvalue, unary };
}

/**
 * @brief Construct a set of ita instructions from a set of definitions
 *
 *  A set of definitions constitute a B program.
 *
 *   Definition grammar:
 *
 *     definition : function_definition
 *         | vector_definition

 *  Note that vector definitions are scanned first
 */
Instructions ITA::build_from_definitions(Node const& node)
{
    credence_assert_equal(node["root"].to_string(), "definitions");
    Instructions instructions{};
    auto definitions = node["left"].array_range();
    for (auto& definition : definitions)
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(definition);
    for (auto& definition : definitions) {
        if (definition["node"].to_string() == "function_definition") {
            auto function_instructions =
                build_from_function_definition(definition);
            ir::insert(instructions, function_instructions);
        }
    }
    ir::insert(instructions_, instructions);
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a function definition
 */
Instructions ITA::build_from_function_definition(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "function_definition");
    Instructions instructions{};
    auto name = node["root"].to_string();
    auto parameters = node["left"];
    Parameters parameter_lvalues{};
    auto block = node["right"];

    symbols_.set_symbol_by_name(name, value::Expression::WORD_LITERAL);

    if (parameters.JSON_type() == util::AST_Node::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            m::match(ident["node"].to_string())(
                // cppcheck-suppress syntaxError
                m::pattern | "lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["root"].to_string());
                        symbols_.set_symbol_by_name(ident["root"].to_string(),
                            value::Expression::NULL_LITERAL);
                    },
                m::pattern | "vector_lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["root"].to_string());
                        auto size = ident["left"]["root"].to_int();
                        symbols_.set_symbol_by_name(ident["root"].to_string(),
                            {
                                static_cast<unsigned char>('0'),
                                { "byte", size }
                        });
                    },
                m::pattern | "indirect_lvalue" =
                    [&] {
                        parameter_lvalues.emplace_back(
                            ident["left"]["root"].to_string());
                        symbols_.set_symbol_by_name(
                            ident["left"]["root"].to_string(),
                            value::Expression::WORD_LITERAL);
                    });
        }
    }

    auto label = build_function_label_from_parameters(
        node["root"].to_string(), parameter_lvalues);

    instructions.emplace_back(make_quadruple(Instruction::LABEL, label));
    instructions.emplace_back(make_quadruple(Instruction::FUNC_START));

    make_root_branch();

    auto block_instructions = build_from_block_statement(block, true);

    ir::insert(instructions, block_instructions);

    instructions.emplace_back(make_quadruple(Instruction::FUNC_END));

    // clear symbols from function scope
    symbols_.clear();

    return instructions;
}

/**
 * @brief Build the function label from a parameter pack
 *  Example:
 *     __main(argc,argv):
 */
constexpr std::string ITA::build_function_label_from_parameters(
    std::string_view name,
    Parameters const& parameters)
{

    auto label = std::string{ "__" };
    label.append(name);
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
    credence_assert_equal(node["node"].to_string(), "vector_definition");
    credence_assert(node.has_key("right"));
    auto name = node["root"].to_string();
    auto size = node.has_key("left") ? node["left"]["root"].to_int() : 1;
    auto right_child_node = node["right"];
    std::vector<value::Literal> values_at{};

    if (std::cmp_not_equal(size, right_child_node.to_deque().size()))
        ita_error(
            fmt::format(
                "invalid vector definition, right-hand-side allocation of "
                "\"{}\" items is out of range; expected no more than "
                "\"{}\" "
                "items ",
                right_child_node.to_deque().size(),
                size),
            name);

    globals_.set_symbol_by_name(name, values_at);
    for (auto& child_node : right_child_node.array_range()) {
        auto rvalue = Expression_Parser::parse(
            child_node, internal_symbols_, symbols_, globals_);
        auto datatype = std::get<value::Literal>(rvalue.value);
        values_at.emplace_back(datatype);
    }

    globals_.set_symbol_by_name(name, values_at);
}

/**
 * @brief Setup branch state and label stack based on statement type
 */
void ITA::build_statement_setup_branches(std::string_view type,
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
void ITA::build_statement_teardown_branches(std::string_view type,
    Instructions& instructions)
{
    if (branch.is_branching_statement(type)) {
        bool lookbehind = type == "while";
        if (instructions.empty() or
            not branch.last_instruction_is_jump(instructions.back()))
            instructions.emplace_back(make_quadruple(Instruction::GOTO,
                std::get<1>(branch.get_parent_branch(lookbehind).value()),
                ""));
        branch.stack.pop();
        branch.decrement_branch_level(true);
    }
}

/**
 *
 * @brief Construct a set of ita instructions from a block statement
 */
Instructions ITA::build_from_block_statement(Node const& node,
    bool root_function_scope)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "block");

    auto [instructions, branches] = make_statement_instructions();
    auto statements = node["left"];

    for (auto& statement : statements.array_range()) {
        auto statement_type = statement["root"].to_string();

        build_statement_setup_branches(statement_type, instructions);

        m::match(statement_type)(
            m::pattern | "auto" =
                [&] { build_from_auto_statement(statement, instructions); },
            m::pattern | "extrn" =
                [&] { build_from_extrn_statement(statement, instructions); },
            m::pattern | "if" =
                [&] {
                    auto [jump_instructions, if_instructions] =
                        build_from_if_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, if_instructions);
                },
            m::pattern | "switch" =
                [&] {
                    auto [jump_instructions, switch_statements] =
                        build_from_switch_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, switch_statements);
                },
            m::pattern | "while" =
                [&] {
                    auto [jump_instructions, while_instructions] =
                        build_from_while_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, while_instructions);
                },
            m::pattern | "rvalue" =
                [&] {
                    auto rvalue_instructions =
                        build_from_rvalue_statement(statement);
                    ir::insert(instructions, rvalue_instructions);
                },
            m::pattern | "label" =
                [&] {
                    auto label_instructions =
                        build_from_label_statement(statement);
                    ir::insert(instructions, label_instructions);
                },
            m::pattern | "goto" =
                [&] {
                    auto goto_instructions =
                        build_from_goto_statement(statement);
                    ir::insert(instructions, goto_instructions);
                },
            m::pattern | "return" =
                [&] {
                    auto return_instructions =
                        build_from_return_statement(statement);
                    ir::insert(instructions, return_instructions);
                }

        );

        build_statement_teardown_branches(statement_type, branches);
    }

    if (root_function_scope) {
        branch.teardown();
        instructions.emplace_back(branch.get_root_branch().value());
        instructions.emplace_back(make_quadruple(Instruction::LEAVE));
    }

    ir::insert(instructions, branches);
    return instructions;
}

/**
 * @brief Insert the jump statement at the top of the predicate instruction
 * set, and push the GOTO to resume at the end of the branch instructions
 *
 * Note that generally the build_from_block_statement add
 * the GOTO, we add it here during stacks of branches
 */
void ITA::insert_branch_jump_and_resume_instructions(Node const& block,
    Instructions& predicate_instructions,
    Instructions& branch_instructions,
    Quadruple const& label,
    detail::Branch::Last_Branch const& tail)
{
    predicate_instructions.emplace_back(make_quadruple(Instruction::IF,
        build_from_branch_comparator_rvalue(block, predicate_instructions),
        detail::instruction_to_string(Instruction::GOTO),
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
void ITA::insert_branch_block_instructions(Node const& block,
    Instructions& branch_instructions)
{
    if (block["root"].to_string() == "block") {
        auto block_instructions = build_from_block_statement(block, false);
        ir::insert(branch_instructions, block_instructions);

    } else {
        auto block_statement = detail::make_block_statement(block);
        insert_branch_block_instructions(block_statement, branch_instructions);
    }
}

/**
 * @brief Turn an rvalue into a "truthy" comparator for statement
 * predicates
 */
std::string ITA::build_from_branch_comparator_rvalue(Node const& block,
    Instructions& instructions)
{
    std::string temp_lvalue{};
    auto rvalue = Expression_Parser::parse(block, internal_symbols_, symbols_);
    auto comparator_instructions = expression_node_to_temporary_instructions(
        symbols_, block, internal_symbols_, &temporary)
                                       .first;

    m::match(value::get_expression_type(rvalue.value))(
        m::pattern | m::or_(std::string{ "relation" },
                         std::string{ "unary" },
                         std::string{ "symbol" },
                         std::string{ "array" }) =
            [&] {
                ir::insert(instructions, comparator_instructions);
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            },
        m::pattern | m::or_(std::string{ "lvalue" }, std::string{ "literal" }) =
            [&] {
                auto rhs = fmt::format("{} {}",
                    detail::instruction_to_string(Instruction::CMP),
                    value::expression_type_to_string(rvalue.value, false));
                auto temp = ir::make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },
        m::pattern | std::string{ "function" } =
            [&] {
                ir::insert(instructions, comparator_instructions);
                auto rhs = fmt::format(
                    "{} RET", detail::instruction_to_string(Instruction::CMP));
                auto temp = ir::make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },
        m::pattern | m::_ =
            [&] {
                ir::insert(instructions, comparator_instructions);
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            });

    return temp_lvalue;
}

/**
 * @brief Construct ita instructions from a case statement in a switch
 */
ITA::Branch_Instructions ITA::build_from_case_statement(Node const& node,
    std::string const& switch_label,
    detail::Branch::Last_Branch const& tail)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "case");

    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    bool break_statement = false;
    auto jump = make_temporary();
    auto statements = node["right"].to_deque();
    auto case_statement = detail::make_block_statement(statements);

    auto condition =
        Expression_Parser::parse(node["left"], internal_symbols_, symbols_);

    predicate_instructions.emplace_back(make_quadruple(Instruction::JMP_E,
        switch_label,
        value::expression_type_to_string(condition.value, false),
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
            branch_instructions.emplace_back(make_quadruple(Instruction::GOTO,
                std::get<1>(branch.get_parent_branch().value()),
                ""));

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from a switch statement
 */
ITA::Branch_Instructions ITA::build_from_switch_statement(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "switch");
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    auto predicate = node["left"];
    auto blocks = node["right"];
    // get the parent label of the switch statement
    auto tail = branch.get_parent_branch();
    auto cases = std::stack<detail::Branch::Last_Branch>{};
    auto switch_label =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);
    branch.stack.emplace(tail);
    for (auto& statement : blocks.array_range()) {
        auto start = make_temporary();
        branch.stack.emplace(start);
        auto [jump_instructions, case_statements] =
            build_from_case_statement(statement, switch_label, tail);
        cases.emplace(branch.stack.top());
        ir::insert(predicate_instructions, jump_instructions);
        ir::insert(branch_instructions, case_statements);
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
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "while");

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
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "if");

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
            branch_instructions.emplace_back(make_quadruple(Instruction::GOTO,
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
Instructions ITA::build_from_label_statement(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "label");
    credence_assert(node.has_key("left"));
    Instructions instructions{};
    auto statement = node["left"];
    auto label = statement.to_deque().front().to_string();
    instructions.emplace_back(
        make_quadruple(Instruction::LABEL, fmt::format("__L{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a goto statement
 */
Instructions ITA::build_from_goto_statement(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "goto");
    credence_assert(node.has_key("left"));
    Instructions instructions{};
    Expression_Parser parser{ internal_symbols_, symbols_ };
    auto statement = node["left"];
    auto label = statement.to_deque().front().to_string();
    if (!parser.is_defined(label))
        credence_error(
            fmt::format("Error: label \"{}\" does not exist", label));

    instructions.emplace_back(
        make_quadruple(Instruction::GOTO, fmt::format("__L{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a block statement
 */
Instructions ITA::build_from_return_statement(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "return");
    credence_assert(node.has_key("left"));
    Instructions instructions{};
    auto return_statement = node["left"];

    auto return_instructions = expression_node_to_temporary_instructions(
        symbols_, return_statement, internal_symbols_, &temporary);

    instructions.insert(instructions.end(),
        return_instructions.first.begin(),
        return_instructions.first.end());

    if (!return_instructions.second.empty() and instructions.empty()) {
        auto last_rvalue = std::get<value::Expression::Type_Pointer>(
            return_instructions.second.back());
        instructions.emplace_back(make_quadruple(Instruction::RETURN,
            value::expression_type_to_string(*last_rvalue),
            ""));
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
void ITA::build_from_extrn_statement(Node const& node,
    Instructions& instructions)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "extrn");
    credence_assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        auto name = ident["root"].to_string();
        if (globals_.is_defined(name)) {
            auto global_symbol = globals_.get_pointer_by_name(name);
            symbols_.set_symbol_by_name(name, global_symbol);
            instructions.emplace_back(make_quadruple(Instruction::GLOBL, name));

        } else {
            ita_error("symbol not defined in global scope", name);
        }
    }
}

/**
 * @brief Symbol construction from auto declaration statements
 */
void ITA::build_from_auto_statement(Node const& node,
    Instructions& instructions)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "auto");
    credence_assert(node.has_key("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.array_range()) {
        m::match(ident["node"].to_string())(
            m::pattern | "lvalue" =
                [&] {
                    auto name = ident["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        ita_error("identifier is already defined in auto "
                                  "declaration",
                            name);

#endif
                    instructions.emplace_back(
                        make_quadruple(Instruction::LOCL, name));
                    symbols_.set_symbol_by_name(
                        name, value::Expression::NULL_LITERAL);
                },
            m::pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].to_int();
                    auto name = ident["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        ita_error("identifier is already defined in auto "
                                  "declaration",
                            name);

#endif
                    instructions.emplace_back(
                        make_quadruple(Instruction::LOCL, name));
                    symbols_.set_symbol_by_name(name,
                        {
                            static_cast<unsigned char>('0'),
                            { "byte", size }
                    });
                },
            m::pattern | "indirect_lvalue" =
                [&] {
                    auto name = ident["left"]["root"].to_string();
#ifndef CREDENCE_TEST
                    if (symbols_.is_defined(name))
                        ita_error("identifier is already defined in auto "
                                  "declaration",
                            name);

#endif
                    instructions.emplace_back(make_quadruple(
                        Instruction::LOCL, fmt::format("*{}", name)));
                    symbols_.set_symbol_by_name(
                        name, value::Expression::WORD_LITERAL);
                });
    }
}

/**
 * @brief Construct a set of ita instructions from an rvalue statement
 */
Instructions ITA::build_from_rvalue_statement(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "statement");
    credence_assert_equal(node["root"].to_string(), "rvalue");
    credence_assert(node.has_key("left"));
    auto statement = node["left"];

    return expression_node_to_temporary_instructions(
        symbols_, statement, internal_symbols_, &temporary)
        .first;
}

/**
 * @brief Emit a single qaudrupl-tuple to a std::ostream
 *   If indent is true indent with a tab for formatting
 */
void detail::emit_to(std::ostream& os, Quadruple const& ita, bool indent)
{ // not constexpr until C++23
    Instruction op = std::get<Instruction>(ita);
    // clang-format off
    const std::initializer_list<Instruction> lhs_instruction = {
        Instruction::GOTO,
        Instruction::GLOBL,
        Instruction::LOCL,
        Instruction::PUSH,
        Instruction::LABEL,
        Instruction::POP,
        Instruction::CALL
    };
    // clang-format on
    if (std::ranges::find(lhs_instruction, op) != lhs_instruction.end()) {
        if (op == Instruction::LABEL) {
            os << std::get<1>(ita) << ":" << std::endl;
        } else {
            if (indent)
                os << "    ";
            os << op << " " << std::get<1>(ita) << ";" << std::endl;
        }
    } else {
        if (indent) {
            if (op != Instruction::FUNC_START and op != Instruction::FUNC_END)
                os << "    ";
        }
        m::match(op)(
            m::pattern | Instruction::RETURN =
                [&] {
                    os << op << " " << std::get<1>(ita) << ";" << std::endl;
                },
            m::pattern |
                Instruction::LEAVE = [&] { os << op << ";" << std::endl; },
            m::pattern | m::or_(Instruction::IF, Instruction::JMP_E) =
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
                    if (indent and op == Instruction::FUNC_END)
                        os << std::endl << std::endl;
                }

        );
    }
}

/**
 * @brief Check if AST node is a branch statement node
 */
constexpr inline bool detail::Branch::is_branching_statement(std::string_view s)
{
    return std::ranges::find(BRANCH_STATEMENTS, s) != BRANCH_STATEMENTS.end();
}

/**
 * @brief Check if a Quadruple instruction is Instruction::GOTO
 */
constexpr inline bool detail::Branch::last_instruction_is_jump(
    Quadruple const& inst)
{
    return std::get<0>(inst) == Instruction::GOTO;
}

/**
 * @brief Increment branch level, create return label and add to branch
 * stack
 */
inline void detail::Branch::increment_branch_level()
{
    is_branching = true;
    level++;
    block_level = ir::make_temporary(temporary);
    stack.emplace(block_level);
}

/**
 * @brief Decrement branch level and pop branch label off the stack
 */
inline void detail::Branch::decrement_branch_level(bool not_branching)
{
    credence_assert(level > 1);
    credence_assert(!stack.empty());
    level--;
    if (not_branching)
        is_branching = false;
    stack.pop();
}

/**
 * @brief Set branching to false, branching is complete
 */
constexpr inline void detail::Branch::teardown()
{
    credence_assert_equal(level, 1);
    is_branching = false;
}

/**
 * @brief Get a parent branch or the root branch label from the stack
 */
detail::Branch::Last_Branch detail::Branch::get_parent_branch(bool last)
{
    credence_assert(root_branch.has_value());
    if (last and stack.size() > 1) {
        auto top = stack.top();
        stack.pop();
        auto previous = stack.top();
        stack.emplace(top);
        return previous;
    } else
        return stack.empty() ? root_branch : stack.top();
}

/**
 * @brief Raise ITA construction error
 */
inline void ITA::ita_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location)
{
    credence_compile_error(location, message, symbol, internal_symbols_);
}

} // namespace ir

} // namespace credence