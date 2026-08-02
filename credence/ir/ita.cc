/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include <credence/ir/ita.h>

#include <credence/error.h>        // for assert_equal_impl, credence_...
#include <credence/ir/hir_queue.h> // for queue_from_hir
#include <credence/ir/operand.h>   // for operand_to_string, WORD_LIT...
#include <credence/ir/temporary.h> // for hir_to_ita_instructions
#include <credence/symbol.h>       // for Symbol_Table
#include <credence/types.h>        // for get_unary_operator, is_unary...
#include <credence/util.h>         // for range_contains, AST_Node
#include <easyjson.h>              // for JSON
#include <fmt/format.h>            // for format
#include <initializer_list>        // for initializer_list
#include <matchit.h>               // for pattern, PatternHelper, Patt...
#include <memory>                  // for shared_ptr
#include <utility>                 // for get, pair, cmp_not_equal
#include <variant>                 // for monostate, get, variant

/****************************************************************************
 * Instruction Tuple Abstraction
 *
 * The intermediate representation (IR) is formalized as a linear four-tuple,
 * named the Instruction Tuple Abstraction (ITA). The ITA comprises a collection
 * of platform-independent instructions that approximate the structure and
 * semantics of a target machine language.
 *
 * See ir/readme.md for details.
 *
 *  Example transformation:
 *
 *  main() {
 *    auto x;
 *    x = 5 + 10;
 *  }
 *
 *  Becomes:
 *
 *  __main():
 *   BeginFunc ;
 *    LOCL x;
 *    _t1 = (5:int:4) + (10:int:4);
 *    MOV x _t1;
 *   EndFunc ;
 *
 ****************************************************************************/

namespace credence {

namespace ir {

namespace hir = credence::frontend::hir;

namespace {

/**
 * @brief The statement name the branch machinery keys its stack on
 */
constexpr std::string_view branch_name_of(hir::Type type)
{
    switch (type) {
        case hir::Type::If:
            return "if";
        case hir::Type::While:
            return "while";
        case hir::Type::Case:
            return "case";
        case hir::Type::Switch:
            return "switch";
        default:
            return "";
    }
}

} // namespace

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
/**
 * @brief The name of a declaration node
 */
std::string_view ITA::symbol_name_of(Node node) const
{
    return unit_->symbol_name(unit_->nodes[node].data.symbol);
}

/**
 * @brief The literal value of a constant node
 */
operand::Literal ITA::literal_of(Node node) const
{
    auto const& value = unit_->nodes[node];
    switch (value.type) {
        case hir::Type::Integer:
            return { static_cast<int>(value.data.integer),
                operand::TYPE_LITERAL.at("int") };
        case hir::Type::Double:
            return { value.data.real, operand::TYPE_LITERAL.at("double") };
        case hir::Type::Float:
            return { static_cast<float>(value.data.real),
                operand::TYPE_LITERAL.at("float") };
        case hir::Type::Bool:
            return { unit_->string(value.data.string) == "true" ? 1 : 0,
                operand::TYPE_LITERAL.at("bool") };
        case hir::Type::Char: {
            auto text = std::string{ unit_->string(value.data.string) };
            if (text.size() >= 2 and text.front() == '"')
                text = text.substr(1, text.size() - 2);
            return { text.empty() ? char{} : text.front(),
                operand::TYPE_LITERAL.at("char") };
        }
        case hir::Type::String: {
            auto text = std::string{ unit_->string(value.data.string) };
            if (text.size() >= 2 and text.front() == '"')
                text = text.substr(1, text.size() - 2);
            auto size = text.size();
            return {
                text, operand::Size{ "string", size }
            };
        }
        case hir::Type::Symbol_Ref:
            return { std::string{ unit_->symbol_name(value.data.symbol) },
                operand::WORD_LITERAL.second };
        default:
            return operand::NULL_LITERAL;
    }
}

/**
 * @brief Reject a call to a name nothing in reach defines
 *
 * The frontend lets a call name anything, since the standard library adds
 * its own names only once the frontend has run. By the time the IR builds,
 * the table holds everything the program can reach, so a name still absent
 * here is one the linker could not resolve either.
 */
void ITA::check_call_is_resolvable(Node node)
{
    auto const& call = unit_->nodes[node];
    auto callee = call.data.binary.lhs;

    if (unit_->nodes[callee].type != hir::Type::Symbol_Ref)
        return;

    auto symbol = unit_->nodes[callee].data.symbol;
    if (!unit_->symbol_table.at(symbol).assumed)
        return;

    auto name = std::string{ unit_->symbol_name(symbol) };
    if (!details_.has_key(name))
        ita_error("identifier does not exist in current scope, did you mean "
                  "to use extrn?",
            name);
}

Instructions ITA::build_from_definitions()
{
    Instructions instructions{};

    // vectors are placed before any function body refers to them
    for (auto definition : unit_->definitions)
        if (unit_->nodes[definition].type == hir::Type::Vector)
            build_from_vector_definition(definition);

    for (auto definition : unit_->definitions) {
        if (unit_->nodes[definition].type != hir::Type::Function)
            continue;
        auto function_instructions = build_from_function_definition(definition);
        ir::insert(instructions, function_instructions);
    }

    ir::insert(instructions_, instructions);
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a function definition
 */
Instructions ITA::build_from_function_definition(Node node)
{
    Instructions instructions{};
    auto span = unit_->nodes[node].data.span;

    // [name, parameter..., body]
    auto name = std::string{ symbol_name_of(unit_->extra[span.start]) };
    auto body = unit_->extra[span.start + span.count - 1];

    Parameters parameter_lvalues{};
    symbols_.set_symbol_by_name(name, operand::WORD_LITERAL);

    for (std::uint32_t i = 1; i + 1 < span.count; ++i) {
        auto parameter = unit_->extra[span.start + i];
        auto symbol = unit_->nodes[parameter].data.symbol;
        auto const& declared = unit_->symbol_table.at(symbol);
        auto parameter_name = std::string{ unit_->symbol_name(symbol) };

        if (declared.indirect) {
            parameter_lvalues.emplace_back(fmt::format("*{}", parameter_name));
            symbols_.set_symbol_by_name(parameter_name, operand::WORD_LITERAL);
        } else if (declared.storage == hir::Storage::Vector) {
            parameter_lvalues.emplace_back(parameter_name);
            symbols_.set_symbol_by_name(parameter_name,
                {
                    static_cast<unsigned char>('0'),
                    { "byte", static_cast<std::size_t>(declared.count) }
            });
        } else {
            parameter_lvalues.emplace_back(parameter_name);
            symbols_.set_symbol_by_name(parameter_name, operand::NULL_LITERAL);
        }
    }

    auto label = build_function_label_from_parameters(name, parameter_lvalues);

    instructions.emplace_back(make_quadruple(Instruction::LABEL, label));
    instructions.emplace_back(make_quadruple(Instruction::FUNC_START));

    make_root_branch();

    auto block_instructions = build_from_block_statement(body, true);

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
void ITA::build_from_vector_definition(Node node)
{
    auto span = unit_->nodes[node].data.span;

    // [name, value...]
    auto symbol = unit_->nodes[unit_->extra[span.start]].data.symbol;
    auto name = std::string{ unit_->symbol_name(symbol) };
    auto declared_size = unit_->symbol_table.at(symbol).count;
    auto values = span.count - 1;

    std::vector<operand::Literal> values_at{};

    if (std::cmp_less(declared_size, values))
        ita_error(
            fmt::format(
                "invalid vector definition, right-hand-side allocation of "
                "\"{}\" items is out of range; expected no more than "
                "\"{}\" "
                "items ",
                values,
                declared_size),
            name);

    globals_.set_symbol_by_name(name, values_at);
    for (std::uint32_t i = 1; i < span.count; ++i)
        values_at.emplace_back(literal_of(unit_->extra[span.start + i]));

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
Instructions ITA::build_from_block_statement(Node node,
    bool root_function_scope)
{
    auto [instructions, branches] = make_statement_instructions();
    auto span = unit_->nodes[node].data.span;

    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto statement = unit_->extra[span.start + i];
        auto kind = unit_->nodes[statement].type;
        auto statement_type = branch_name_of(kind);

        build_statement_setup_branches(statement_type, instructions);

        m::match(kind)(
            m::pattern | hir::Type::Auto =
                [&] { build_from_auto_statement(statement, instructions); },
            m::pattern | hir::Type::Extrn =
                [&] { build_from_extrn_statement(statement, instructions); },
            m::pattern | hir::Type::If =
                [&] {
                    auto [jump_instructions, if_instructions] =
                        build_from_if_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, if_instructions);
                },
            m::pattern | hir::Type::Switch =
                [&] {
                    auto [jump_instructions, switch_statements] =
                        build_from_switch_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, switch_statements);
                },
            m::pattern | hir::Type::While =
                [&] {
                    auto [jump_instructions, while_instructions] =
                        build_from_while_statement(statement);
                    ir::insert(instructions, jump_instructions);
                    ir::insert(branches, while_instructions);
                },
            m::pattern | hir::Type::Label =
                [&] {
                    auto label_instructions =
                        build_from_label_statement(statement);
                    ir::insert(instructions, label_instructions);
                },
            m::pattern | hir::Type::Goto =
                [&] {
                    auto goto_instructions =
                        build_from_goto_statement(statement);
                    ir::insert(instructions, goto_instructions);
                },
            m::pattern | hir::Type::Return =
                [&] {
                    auto return_instructions =
                        build_from_return_statement(statement);
                    ir::insert(instructions, return_instructions);
                },
            // anything else in a statement position holds an expression
            m::pattern | m::_ =
                [&] {
                    auto rvalue_instructions =
                        build_from_rvalue_statement(statement);
                    ir::insert(instructions, rvalue_instructions);
                });

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
void ITA::insert_branch_jump_and_resume_instructions(Node block,
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
void ITA::insert_branch_block_instructions(Node block,
    Instructions& branch_instructions)
{
    if (block == hir::null_node_index)
        return;
    auto block_instructions = build_from_block_statement(block, false);
    ir::insert(branch_instructions, block_instructions);
}

/**
 * @brief Turn an rvalue into a "truthy" comparator for statement
 * predicates
 */
std::string ITA::build_from_branch_comparator_rvalue(Node block,
    Instructions& instructions)
{
    std::string temp_lvalue{};
    auto comparator_instructions = hir_to_ita_instructions(
        *unit_, block, details_, &temporary, &identifier)
                                       .first;

    auto kind = unit_->nodes[block].type;

    m::match(kind)(
        // a comparison or a computed value leaves its result in the last
        // instruction it emitted
        m::pattern | m::or_(hir::Type::Binary,
                         hir::Type::Unary,
                         hir::Type::Subscript,
                         hir::Type::Dereference,
                         hir::Type::Address_Of,
                         hir::Type::Pre_Inc_Dec,
                         hir::Type::Post_Inc_Dec,
                         hir::Type::Assign,
                         hir::Type::Ternary) =
            [&] {
                ir::insert(instructions, comparator_instructions);
                temp_lvalue =
                    std::get<1>(instructions[instructions.size() - 1]);
            },

        // a name is compared by what it holds, so the comparison names it
        // and does not stand it up as a value
        m::pattern | hir::Type::Symbol_Ref =
            [&] {
                auto rhs = fmt::format("{} {}",
                    detail::instruction_to_string(Instruction::CMP),
                    symbol_name_of(block));
                auto temp = ir::make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },

        // a constant is compared directly
        m::pattern | m::or_(hir::Type::Integer,
                         hir::Type::Double,
                         hir::Type::Bool,
                         hir::Type::Char,
                         hir::Type::String) =
            [&] {
                auto rhs = fmt::format("{} {}",
                    detail::instruction_to_string(Instruction::CMP),
                    operand::literal_to_string(literal_of(block)));
                auto temp = ir::make_temporary(&temporary, rhs);
                instructions.emplace_back(temp);
                temp_lvalue = std::get<1>(temp);
            },

        m::pattern | hir::Type::Call =
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
ITA::Branch_Instructions ITA::build_from_case_statement(Node node,
    std::string const& switch_label,
    detail::Branch::Last_Branch const& tail)
{
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    bool break_statement = false;
    auto jump = make_temporary();

    auto value = unit_->nodes[node].data.binary.lhs;
    auto body = unit_->nodes[node].data.binary.rhs;

    predicate_instructions.emplace_back(make_quadruple(Instruction::JMP_E,
        switch_label,
        operand::literal_to_string(literal_of(value)),
        std::get<1>(jump)));
    if (branch.stack.size() > 2) {
        auto parent = tail.value_or(branch.get_parent_branch(true).value());
        branch_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(parent)));
    }
    branch_instructions.emplace_back(jump);

    // a trailing break leaves the switch and does not fall through
    auto body_span = unit_->nodes[body].data.span;
    if (body_span.count > 0 and
        unit_->nodes[unit_->extra[body_span.start + body_span.count - 1]]
                .type == hir::Type::Break)
        break_statement = true;

    insert_branch_block_instructions(body, branch_instructions);

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
ITA::Branch_Instructions ITA::build_from_switch_statement(Node node)
{
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    auto predicate = unit_->nodes[node].data.binary.lhs;
    auto blocks = unit_->nodes[node].data.binary.rhs;

    // get the parent label of the switch statement
    auto tail = branch.get_parent_branch();
    auto cases = std::stack<detail::Branch::Last_Branch>{};
    auto switch_label =
        build_from_branch_comparator_rvalue(predicate, predicate_instructions);
    branch.stack.emplace(tail);

    auto case_span = unit_->nodes[blocks].data.span;
    for (std::uint32_t i = 0; i < case_span.count; ++i) {
        auto statement = unit_->extra[case_span.start + i];
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
ITA::Branch_Instructions ITA::build_from_while_statement(Node node)
{
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();
    auto predicate = unit_->nodes[node].data.binary.lhs;
    auto body = unit_->nodes[node].data.binary.rhs;

    auto tail = branch.get_parent_branch(true);
    auto jump = make_temporary();
    auto start = make_temporary();

    branch.stack.emplace(start);

    predicate_instructions.emplace_back(start);

    insert_branch_jump_and_resume_instructions(
        predicate, predicate_instructions, branch_instructions, jump, tail);

    branch_instructions.emplace_back(jump);

    insert_branch_block_instructions(body, branch_instructions);

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from an if statement
 */
ITA::Branch_Instructions ITA::build_from_if_statement(Node node)
{
    auto [predicate_instructions, branch_instructions] =
        make_statement_instructions();

    // [condition, then, else or null]
    auto span = unit_->nodes[node].data.span;
    auto predicate = unit_->extra[span.start];
    auto then_branch = unit_->extra[span.start + 1];
    auto else_branch = unit_->extra[span.start + 2];

    auto start = make_temporary();
    auto jump = make_temporary();

    insert_branch_jump_and_resume_instructions(
        predicate, predicate_instructions, branch_instructions, jump);

    branch_instructions.emplace_back(jump);
    branch.stack.emplace(start);

    insert_branch_block_instructions(then_branch, branch_instructions);

    // no else statement
    if (else_branch == hir::null_node_index)
        predicate_instructions.emplace_back(start);

    // else statement
    if (else_branch != hir::null_node_index) {
        auto else_label = make_temporary();
        if (!branch.last_instruction_is_jump(branch_instructions.back()))
            branch_instructions.emplace_back(make_quadruple(Instruction::GOTO,
                std::get<1>(branch.get_parent_branch().value()),
                ""));
        predicate_instructions.emplace_back(
            make_quadruple(Instruction::GOTO, std::get<1>(else_label)));
        branch_instructions.emplace_back(else_label);
        insert_branch_block_instructions(else_branch, branch_instructions);
        predicate_instructions.emplace_back(start);
    }

    return { predicate_instructions, branch_instructions };
}

/**
 * @brief Construct ita instructions from a label statement
 */
Instructions ITA::build_from_label_statement(Node node)
{
    Instructions instructions{};
    auto label = std::string{ symbol_name_of(node) };
    instructions.emplace_back(
        make_quadruple(Instruction::LABEL, fmt::format("__L{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a goto statement
 */
Instructions ITA::build_from_goto_statement(Node node)
{
    Instructions instructions{};
    auto label = std::string{ symbol_name_of(node) };
    instructions.emplace_back(
        make_quadruple(Instruction::GOTO, fmt::format("__L{}", label), ""));
    return instructions;
}

/**
 * @brief Construct a set of ita instructions from a block statement
 */
Instructions ITA::build_from_return_statement(Node node)
{
    Instructions instructions{};
    auto value = unit_->nodes[node].data.unary;

    if (value == hir::null_node_index) {
        instructions.emplace_back(make_quadruple(Instruction::RETURN, ""));
        return instructions;
    }

    auto return_instructions = hir_to_ita_instructions(
        *unit_, value, details_, &temporary, &identifier);
    ir::insert(instructions, return_instructions.first);

    if (!return_instructions.second.empty() and instructions.empty()) {
        auto last_rvalue = std::get<operand::Operand::Type_Pointer>(
            return_instructions.second.back());
        instructions.emplace_back(make_quadruple(
            Instruction::RETURN, operand::operand_to_string(*last_rvalue)));
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
void ITA::build_from_extrn_statement(Node node, Instructions& instructions)
{
    auto span = unit_->nodes[node].data.span;
    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto declaration = unit_->extra[span.start + i];
        auto name = std::string{ symbol_name_of(declaration) };
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
void ITA::build_from_auto_statement(Node node, Instructions& instructions)
{
    auto span = unit_->nodes[node].data.span;
    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto declaration = unit_->extra[span.start + i];
        auto symbol = unit_->nodes[declaration].data.symbol;
        auto const& declared = unit_->symbol_table.at(symbol);
        auto name = std::string{ unit_->symbol_name(symbol) };

        if (declared.storage == hir::Storage::Vector) {
            instructions.emplace_back(make_quadruple(Instruction::LOCL, name));
            symbols_.set_symbol_by_name(name,
                {
                    static_cast<unsigned char>('0'),
                    { "byte", static_cast<std::size_t>(declared.count) }
            });
        } else if (declared.indirect) {
            instructions.emplace_back(
                make_quadruple(Instruction::LOCL, fmt::format("*{}", name)));
            symbols_.set_symbol_by_name(name, operand::WORD_LITERAL);
        } else {
            instructions.emplace_back(make_quadruple(Instruction::LOCL, name));
            symbols_.set_symbol_by_name(name, operand::NULL_LITERAL);
        }
    }
}

/**
 * @brief Construct a set of ita instructions from an rvalue statement
 */
Instructions ITA::build_from_rvalue_statement(Node node)
{
    auto const& statement = unit_->nodes[node];

    for (auto index = unit_->first[node]; index <= node; ++index)
        if (unit_->nodes[index].type == hir::Type::Call)
            check_call_is_resolvable(index);

    // a block of expression statements, each of which may be empty
    if (statement.type == hir::Type::Block or
        statement.type == hir::Type::Expression) {
        Instructions instructions{};
        auto span = statement.data.span;
        for (std::uint32_t i = 0; i < span.count; ++i) {
            auto child = unit_->extra[span.start + i];
            auto child_instructions = build_from_rvalue_statement(child);
            ir::insert(instructions, child_instructions);
        }
        return instructions;
    }

    return hir_to_ita_instructions(
        *unit_, node, details_, &temporary, &identifier)
        .first;
}

/**
 * @brief Emit a single qaudrupl-tuple to a std::ostream
 *   If indent is true indent with a tab for formatting
 */
void detail::emit_to(std::ostream& os, Quadruple const& ita, bool indent)
{ // not constexpr until C++23
    Instruction op = std::get<Instruction>(ita);
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
    if (util::range_contains(op, lhs_instruction)) {
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
    return util::range_contains(s, BRANCH_STATEMENTS);
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
    credence_compile_error(location, message, symbol, util::AST_Node{});
}

} // namespace ir

} // namespace credence