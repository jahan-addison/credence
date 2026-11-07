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

#include "table.h"

#include "binding.h"                        // for Object_Table, Vector
#include "checker.h"                        // for Type_Checker, Value
#include "hir.h"                            // for data_kind_to_string
#include <credence/error.h>                 // for throw_compiletime_error
#include <credence/language/ast_lowering.h> // for AST_Lowering
#include <credence/language/literal.h>      // for Literal, NULL_LITERAL
#include <credence/symbol.h>                // for Symbol_Table
#include <credence/types.h>                 // for get_rvalue_datatype_fr...
#include <credence/util.h>                  // for AST_Node
#include <cstddef>                          // for size_t
#include <easyjson.h>                       // for JSON
#include <fmt/format.h>                     // for format
#include <matchit.h>                        // for pattern, match, Pattern...
#include <string>                           // for basic_string, to_string
#include <utility>                          // for cmp_not_equal
#include <variant>                          // for get

/****************************************************************************
 * Table
 *
 * Semantic analysis on the HIR. A pass over each definition and statement
 * of a program that lowers its expressions into the HIR and type checks
 * them in their stack frame. The result is the definitions of the object
 * table - vectors, functions, and parameters - that the IR binds onto.
 *
 *  Example:
 *
 *  main() {
 *    extrn unit;
 *    auto x, arr[3];
 *    x = unit;
 *  }
 *
 *  unit 10;
 *
 *  Becomes:
 *
 *    functions["main"] -> { parameters: {} }
 *    vectors["arr"]    -> { size: 3, data: {} }
 *    vectors["unit"]   -> { size: 1, data: {"0": (10:int:4)} }
 *
 ****************************************************************************/

namespace credence::language::hir {

namespace m = matchit;

using checking::Storage;
using checking::Value;

/**
 * @brief Construct the object table from a set of definitions
 *
 *  A set of definitions constitute a B program.
 *
 *   Definition grammar:
 *
 *     definition : function_definition
 *         | vector_definition
 *
 *  Note that vector definitions are scanned first
 */
void Table::build_from_definitions(Node const& node)
{
    credence_assert_equal(node["root"].to_string(), "definitions");
    auto definitions = node["left"].array_range();
    for (auto& definition : definitions)
        if (definition["node"].to_string() == "vector_definition")
            build_from_vector_definition(definition);
    objects_->hoisted_symbols = hoisted_symbols_;
    objects_->globals = globals_;
    build_vector_definitions_from_symbols();
    build_vector_definitions_from_globals();
    for (auto& definition : definitions)
        if (definition["node"].to_string() == "function_definition")
            build_from_function_definition(definition);
    set_pointer_parameters_from_call_arguments();
}

/**
 * @brief Set each scalar parameter that is passed a pointer as a pointer
 *
 * A parameter passed to another function carries its pointer to the
 * callee, so the calls are resolved until no parameter changes. A pointer
 * is a doubleword, so the backends pass it in a doubleword register.
 *
 *  Example:
 *
 *    show(s) { printf("%s", s); }
 *    main(argc, argv) { show(argv[1]); }
 *
 *  Becomes:
 *
 *    functions["show"] -> { parameters: { "*s" } }
 */
void Table::set_pointer_parameters_from_call_arguments()
{
    bool changed{ true };
    while (changed) {
        changed = false;
        for (auto const& call : call_arguments_) {
            if (!objects_->functions.contains(call.callee) or
                not frames_.contains(call.callee))
                continue;
            auto& function = objects_->functions.at(call.callee).get();
            if (call.index >= function.parameters.size())
                continue;
            auto pointer = call.pointer;
            if (!call.parameter.empty() and
                objects_->functions.contains(call.caller))
                pointer = pointer or objects_->functions.at(call.caller)
                                         .get()
                                         .is_pointer_parameter(call.parameter);
            auto& parameter = function.parameters[call.index];
            auto& frame = frames_.at(call.callee);
            if (!pointer or parameter.starts_with("*") or
                frame.locals[parameter].storage != Storage::SCALAR)
                continue;
            frame.locals[parameter] = Value{ Storage::POINTER, "word" };
            parameter = fmt::format("*{}", parameter);
            changed = true;
        }
    }
}

/**
 * @brief Global vector definition and the types of its values
 */
void Table::build_from_vector_definition(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "vector_definition");
    credence_assert(node.has_key("right"));
    auto name = node["root"].to_string();
    auto size = node.has_key("left") ? node["left"]["root"].to_int() : 1;
    auto right_child_node = node["right"];
    std::vector<literal::Literal> values_at{};

    if (std::cmp_not_equal(size, right_child_node.to_deque().size()))
        semantic_error(
            fmt::format(
                "invalid vector definition, right-hand-side allocation of "
                "\"{}\" items is out of range; expected no more than "
                "\"{}\" items",
                right_child_node.to_deque().size(),
                size),
            name);

    globals_.set_symbol_by_name(name, values_at);
    for (auto& child_node : right_child_node.array_range()) {
        auto rvalue = AST_Lowering::lower(
            child_node, hoisted_symbols_, symbols_, globals_);
        values_at.emplace_back(std::get<literal::Literal>(rvalue.value));
    }
    globals_.set_symbol_by_name(name, values_at);

    auto& vector = vectors_[name];
    vector.size = static_cast<std::size_t>(size);
    // a string element is a scalar, only a string literal is a pointer
    for (std::size_t index = 0; index < values_at.size(); index++) {
        auto value = checking::get_value_from_literal(values_at[index]);
        value.storage = Storage::SCALAR;
        vector.data[std::to_string(index)] = value;
    }
}

/**
 * @brief Function definition, its parameters, and its stack frame
 */
void Table::build_from_function_definition(Node const& node)
{
    credence_assert_equal(node["node"].to_string(), "function_definition");
    auto name = node["root"].to_string();
    auto parameters = node["left"];
    stack_frame_symbol_ = name;

    if (frames_.contains(name))
        semantic_error("function name already exists", name);

    auto& frame = frames_[name];
    auto& function = objects_->set_function_definition(name);
    frame.symbol = name;
    objects_->labels.emplace(name);
    symbols_.set_symbol_by_name(name, literal::WORD_LITERAL);

    if (parameters.JSON_type() == util::AST_Node::Class::Array and
        !parameters.to_deque().front().is_null()) {
        for (auto& ident : parameters.array_range()) {
            m::match(ident["node"].to_string())(
                m::pattern | "lvalue" =
                    [&] {
                        auto parameter = ident["root"].to_string();
                        symbols_.set_symbol_by_name(
                            parameter, literal::NULL_LITERAL);
                        frame.locals[parameter] =
                            Value{ Storage::SCALAR, "word" };
                        frame.parameters.emplace(parameter);
                        function.parameters.emplace_back(parameter);
                    },
                m::pattern | "vector_lvalue" =
                    [&] {
                        auto parameter = ident["root"].to_string();
                        auto size = ident["left"]["root"].to_int();
                        symbols_.set_symbol_by_name(parameter,
                            {
                                static_cast<unsigned char>('0'),
                                { "byte", size }
                        });
                        frame.locals[parameter] =
                            Value{ Storage::POINTER, "word" };
                        frame.parameters.emplace(parameter);
                        function.parameters.emplace_back(parameter);
                    },
                m::pattern | "indirect_lvalue" =
                    [&] {
                        auto parameter = ident["left"]["root"].to_string();
                        symbols_.set_symbol_by_name(
                            parameter, literal::WORD_LITERAL);
                        frame.locals[parameter] =
                            Value{ Storage::POINTER, "word" };
                        frame.parameters.emplace(parameter);
                        function.parameters.emplace_back(
                            fmt::format("*{}", parameter));
                    });
        }
    }

    build_from_statement(node["right"]);

    // clear symbols from function scope
    symbols_.clear();
}

/**
 * @brief Construct table of vector symbols and size allocation
 */
void Table::build_vector_definitions_from_symbols()
{
    auto keys = hoisted_symbols_.dump_keys();
    for (LValue const& key : keys) {
        if (hoisted_symbols_[key]["type"].to_string() == "vector_lvalue") {
            auto size = static_cast<std::size_t>(
                hoisted_symbols_[key]["size"].to_int());
            if (!objects_->vectors.contains(key))
                objects_->set_vector_definition(key, size);
        }
    }
}

/**
 * @brief Construct table of global vectors and their values
 */
void Table::build_vector_definitions_from_globals()
{
    if (globals_.empty())
        return;
    for (auto i = globals_.begin_t(); i != globals_.end_t(); i++) {
        std::size_t index = 0;
        auto symbol = *i;
        auto& vector =
            objects_->set_vector_definition(symbol.first, symbol.second.size());
        for (auto const& item : symbol.second) {
            auto key = std::to_string(index++);
            auto value = credence::type::get_rvalue_datatype_from_string(
                data_kind_to_string(item, false));
            objects_->insert_address_storage_rvalue(value);
            vector.data[key] = value;
        }
    }
}

/**
 * @brief Semantic analysis of a statement in the current stack frame
 */
void Table::build_from_statement(Node const& node)
{
    m::match(node["root"].to_string())(
        m::pattern | "block" = [&] { build_from_block_statement(node); },
        m::pattern | "auto" = [&] { build_from_auto_statement(node); },
        m::pattern | "extrn" = [&] { build_from_extrn_statement(node); },
        m::pattern | "if" = [&] { build_from_if_statement(node); },
        m::pattern | "while" = [&] { build_from_while_statement(node); },
        m::pattern | "switch" = [&] { build_from_switch_statement(node); },
        m::pattern | "label" = [&] { build_from_label_statement(node); },
        m::pattern | "goto" = [&] { build_from_goto_statement(node); },
        m::pattern | "return" = [&] { build_from_return_statement(node); },
        m::pattern | "rvalue" = [&] { build_from_rvalue_statement(node); },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Semantic analysis of each statement in a block statement
 */
void Table::build_from_block_statement(Node const& node)
{
    credence_assert_equal(node["root"].to_string(), "block");
    for (auto& statement : node["left"].array_range())
        build_from_statement(statement);
}

/**
 * @brief Declare scalars, vectors, and pointers from an auto statement
 */
void Table::build_from_auto_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    auto& frame = get_stack_frame();
    for (auto& ident : node["left"].array_range()) {
        auto name = ident["node"].to_string() == "indirect_lvalue"
                        ? ident["left"]["root"].to_string()
                        : ident["root"].to_string();
        if (symbols_.is_defined(name))
            semantic_error(
                "identifier is already defined in auto declaration", name);
        m::match(ident["node"].to_string())(
            m::pattern | "lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(name, literal::NULL_LITERAL);
                    frame.locals[name] = Value{ Storage::SCALAR, "null" };
                },
            m::pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].to_int();
                    if (std::cmp_greater(
                            size, checking::Type_Checker::max_vector_size))
                        semantic_error(
                            fmt::format("invalid vector declaration, size "
                                        "'{}' is a stack overflow",
                                size),
                            name);
                    symbols_.set_symbol_by_name(name,
                        {
                            static_cast<unsigned char>('0'),
                            { "byte", size }
                    });
                    frame.vectors[name].size = static_cast<std::size_t>(size);
                },
            m::pattern | "indirect_lvalue" =
                [&] {
                    symbols_.set_symbol_by_name(name, literal::WORD_LITERAL);
                    frame.locals[name] = Value{ Storage::POINTER, "null" };
                });
    }
}

/**
 * @brief Declare global vectors in the stack frame from an extrn statement
 */
void Table::build_from_extrn_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    for (auto& ident : node["left"].array_range()) {
        auto name = ident["root"].to_string();
        if (!globals_.is_defined(name))
            semantic_error("symbol not defined in global scope", name);
        symbols_.set_symbol_by_name(name, globals_.get_pointer_by_name(name));
    }
}

/**
 * @brief Semantic analysis of an if statement and its else statement
 */
void Table::build_from_if_statement(Node const& node)
{
    from_expression_node(node["left"]);
    auto blocks = node["right"].to_deque();
    build_from_statement(blocks.at(0));
    if (!blocks.at(1).is_null())
        build_from_statement(blocks.at(1));
}

/**
 * @brief Semantic analysis of a while statement
 */
void Table::build_from_while_statement(Node const& node)
{
    from_expression_node(node["left"]);
    build_from_statement(node["right"].to_deque().at(0));
}

/**
 * @brief Semantic analysis of a switch statement and each of its cases
 */
void Table::build_from_switch_statement(Node const& node)
{
    from_expression_node(node["left"]);
    for (auto& case_statement : node["right"].array_range()) {
        from_expression_node(case_statement["left"]);
        for (auto& statement : case_statement["right"].array_range())
            build_from_statement(statement);
    }
}

/**
 * @brief Declare a label in the current stack frame
 */
void Table::build_from_label_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    auto label = node["left"].to_deque().front().to_string();
    auto& frame = get_stack_frame();
    if (frame.labels.contains(label))
        semantic_error("label is already defined", label);
    frame.labels.emplace(label);
}

/**
 * @brief Check that the label of a goto statement exists
 */
void Table::build_from_goto_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    auto label = node["left"].to_deque().front().to_string();
    if (!hoisted_symbols_.has_key(label))
        semantic_error("label does not exist", label);
}

/**
 * @brief Type check the return value of a return statement
 *
 * A function may return from more than one statement, but each return
 * value must be the same type
 */
void Table::build_from_return_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    auto expressions = get_expression_nodes(node["left"]);
    if (expressions.empty())
        return;
    auto checker = checking::Type_Checker{
        get_stack_frame(), vectors_, hoisted_symbols_, &call_arguments_
    };
    for (auto it = expressions.begin(); it != expressions.end() - 1; it++)
        checker.from_expression(
            AST_Lowering::lower(*it, hoisted_symbols_, symbols_, globals_)
                .value);
    checker.from_return_expression(AST_Lowering::lower(
        expressions.back(), hoisted_symbols_, symbols_, globals_)
            .value);
}

/**
 * @brief Type check each expression of an rvalue statement
 */
void Table::build_from_rvalue_statement(Node const& node)
{
    credence_assert(node.has_key("left"));
    from_expression_node(node["left"]);
}

/**
 * @brief Lower and type check an expression node or array of nodes
 */
Value Table::from_expression_node(Node const& node)
{
    Value value{};
    auto checker = checking::Type_Checker{
        get_stack_frame(), vectors_, hoisted_symbols_, &call_arguments_
    };
    for (auto const& expression : get_expression_nodes(node))
        value = checker.from_expression(AST_Lowering::lower(
            expression, hoisted_symbols_, symbols_, globals_)
                .value);
    return value;
}

/**
 * @brief Flatten a node or nested arrays of nodes into expression nodes
 */
std::vector<Table::Node> Table::get_expression_nodes(Node const& node)
{
    std::vector<Node> expressions{};
    if (node.JSON_type() != util::AST_Node::Class::Array) {
        if (!node.is_null())
            expressions.emplace_back(node);
        return expressions;
    }
    for (auto& expression : node.array_range()) {
        auto nested = get_expression_nodes(expression);
        expressions.insert(expressions.end(), nested.begin(), nested.end());
    }
    return expressions;
}

/**
 * @brief Get the stack frame of the function under analysis
 */
checking::Frame& Table::get_stack_frame()
{
    credence_assert(frames_.contains(stack_frame_symbol_));
    return frames_.at(stack_frame_symbol_);
}

/**
 * @brief Raise a semantic error in the current stack frame
 */
void Table::semantic_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location)
{
    if (stack_frame_symbol_.empty())
        credence_compile_error(location, message, symbol, hoisted_symbols_);
    throw_compiletime_error(message,
        symbol,
        location,
        "symbol",
        stack_frame_symbol_,
        hoisted_symbols_);
}

} // namespace credence::language::hir
