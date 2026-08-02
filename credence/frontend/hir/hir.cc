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

#include <credence/frontend/hir/hir.h>

#include <fmt/format.h> // for format
#include <matchit.h>    // for match, pattern, or_
#include <utility>      // for move

/****************************************************************************
 *
 * High level intermediate representation
 *
 * The AST with precedence resolved, names bound to symbols, and useless
 * syntax dropped. Keeps the shape of the AST, a flat array of trivially
 * copyable nodes with parallel arrays beside it, so a pass is a loop and
 * not a recursive walk.
 *
 *    nodes    the tree itself
 *    types    parallel to nodes, the type of nodes[i]
 *    metadata parallel to nodes, the source position of nodes[i]
 *    first    parallel to nodes, the first node of the subtree at i
 *    extra    flattened child lists that a Span indexes into
 *
 * Three things change on the way in from the AST:
 *
 *    - a chain such as "a * b + c" is reshaped to respect precedence,
 *      which the parser leaves alone
 *    - an identifier becomes a Symbol_Index, resolved once
 *    - parentheses and the blocks holding call arguments are dropped, as
 *      neither survives precedence resolution
 *
 * Nodes stay in post-order, children before parents, which gives the passes
 * two properties:
 *
 *    - a subtree is a contiguous range, first[i] through i, extracted in
 *      constant time
 *    - a bottom-up pass is a forward loop, as every operand of a node has
 *      already been visited when the node is reached
 *
 * The second lets the IR read an expression as a stack machine. Post-order
 * over an expression with correct precedence is the order an operand stack
 * consumes, so ir/hir_queue.cc walks first[i] through i and finds its
 * operands in the order it pops them.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

namespace m = matchit;

namespace {

using ast::Operator;

/**
 * @brief A node with two children
 */
Node binary_node(Type type, Operator op, Node_Index lhs, Node_Index rhs)
{
    Node node{};
    node.type = type;
    node.op = op;
    node.data.binary.lhs = lhs;
    node.data.binary.rhs = rhs;
    return node;
}

/**
 * @brief A node with one child
 */
Node unary_node(Type type, Operator op, Node_Index child)
{
    Node node{};
    node.type = type;
    node.op = op;
    node.data.unary = child;
    return node;
}

/**
 * @brief A node with a child list
 */
Node span_node(Type type, Span span)
{
    Node node{};
    node.type = type;
    node.op = Operator::None;
    node.data.span = span;
    return node;
}

/**
 * @brief A node with a resolved name
 */
Node symbol_node(Type type, Symbol_Index symbol)
{
    Node node{};
    node.type = type;
    node.op = Operator::None;
    node.data.symbol = symbol;
    return node;
}

} // namespace

namespace detail {

/**
 * @brief Append a node and keep the source position of the AST node
 *
 * The subtree start is computed here and not in a later pass, as
 * every child is already in place by the time its parent is appended.
 */
Node_Index Lowering::add(Node node, ast::Node_Index source)
{
    auto index = static_cast<Node_Index>(unit_.nodes.size());

    auto start = index;
    m::match(payload_of(node.type))(
        m::pattern | Payload::Binary =
            [&] {
                auto lhs = node.data.binary.lhs;
                auto rhs = node.data.binary.rhs;
                if (lhs != null_node_index)
                    start = std::min(start, unit_.first[lhs]);
                if (rhs != null_node_index)
                    start = std::min(start, unit_.first[rhs]);
            },
        m::pattern | Payload::Span =
            [&] {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i) {
                    auto child = unit_.extra[span.start + i];
                    if (child != null_node_index)
                        start = std::min(start, unit_.first[child]);
                }
            },
        m::pattern | Payload::Unary =
            [&] {
                if (node.data.unary != null_node_index)
                    start = std::min(start, unit_.first[node.data.unary]);
            },
        m::pattern | m::_ = [] {});

    unit_.nodes.push_back(node);
    unit_.types.push_back(null_type_index);
    unit_.offsets.push_back(unknown_offset);
    unit_.first.push_back(start);
    unit_.metadata.push_back(source < tree_.metadata.size()
                                 ? tree_.metadata[source]
                                 : ast::Meta{ 0, 0, 0, 0 });
    return index;
}

/**
 * @brief Move a scratch run of children into Unit::extra
 */
Span Lowering::commit(std::size_t scratch_base)
{
    auto count = static_cast<std::uint32_t>(scratch_.size() - scratch_base);
    auto start = static_cast<std::uint32_t>(unit_.extra.size());
    unit_.extra.insert(
        unit_.extra.end(), scratch_.begin() + scratch_base, scratch_.end());
    scratch_.resize(scratch_base);
    return Span{ start, count };
}

void Lowering::error(ast::Node_Index source, std::string message)
{
    auto const& meta = source < tree_.metadata.size() ? tree_.metadata[source]
                                                      : ast::Meta{ 0, 0, 0, 0 };
    diagnostics_.push_back(
        Diagnostic{ std::move(message), meta.line, meta.column });
}

Result Lowering::run()
{
    unit_.strings = tree_.strings;
    unit_.string_text = tree_.string_text;

    unit_.nodes.reserve(tree_.nodes.size());
    unit_.types.reserve(tree_.nodes.size());
    unit_.first.reserve(tree_.nodes.size());
    unit_.offsets.reserve(tree_.nodes.size());
    unit_.metadata.reserve(tree_.nodes.size());

    if (tree_.root != ast::null_node_index) {
        auto span = tree_.nodes[tree_.root].data.span;

        // every name defined at file scope is declared before any body is
        // lowered, so that a call may name a function defined further down
        for (std::uint32_t i = 0; i < span.count; ++i)
            hoist_definition(tree_.extra[span.start + i]);

        for (std::uint32_t i = 0; i < span.count; ++i) {
            auto definition = lower_definition(tree_.extra[span.start + i]);
            if (definition != null_node_index)
                unit_.definitions.push_back(definition);
        }
    }

    return Result{ std::move(unit_), std::move(diagnostics_) };
}

/**
 * @brief Declare the name a top level definition introduces
 *
 * Run over every definition before any body is lowered, since a call may
 * name a function that appears further down the file.
 */
void Lowering::hoist_definition(ast::Node_Index index)
{
    auto const& node = node_at(index);
    switch (node.type) {
        case ast::Type::Function_Definition: {
            auto name = name_of(tree_.extra[node.data.span.start]);
            unit_.symbol_table.declare(name,
                unit_.type_table.function_returning(type_word),
                Storage::Function);
            return;
        }
        case ast::Type::Vector_Definition: {
            auto span = node.data.span;
            auto name = name_of(tree_.extra[span.start]);
            auto size_node = tree_.extra[span.start + 1];
            auto values = tree_.extra[span.start + 2];
            auto count = std::uint32_t{ 0 };
            if (size_node != ast::null_node_index and
                node_at(size_node).type == ast::Type::Integer_Literal and
                node_at(size_node).data.integer > 0)
                count =
                    static_cast<std::uint32_t>(node_at(size_node).data.integer);
            if (count == 0)
                count = node_at(values).data.span.count;
            unit_.symbol_table.declare(name,
                unit_.type_table.vector_of(type_word, count),
                Storage::Vector,
                count);
            return;
        }
        case ast::Type::Union_Definition: {
            auto name = name_of(tree_.extra[node.data.span.start]);
            unit_.symbol_table.declare(name, type_word, Storage::Global);
            return;
        }
        default:
            return;
    }
}

Node_Index Lowering::lower_definition(ast::Node_Index index)
{
    return m::match(node_at(index).type)(
        m::pattern | ast::Type::Function_Definition =
            [&] { return lower_function(index); },
        m::pattern |
            ast::Type::Vector_Definition = [&] { return lower_vector(index); },
        m::pattern |
            ast::Type::Union_Definition = [&] { return lower_union(index); },
        m::pattern | m::_ =
            [&] {
                error(index, "expected a definition at file scope");
                return null_node_index;
            });
}

/**
 * @brief Lower a function, its parameters, and its body
 *
 * The parameters and the body share one scope, so a parameter is visible
 * to the whole body and a local may not shadow it.
 */
Node_Index Lowering::lower_function(ast::Node_Index index)
{
    auto span = node_at(index).data.span;
    auto name_node = tree_.extra[span.start];
    auto parameters = tree_.extra[span.start + 1];
    auto body = tree_.extra[span.start + 2];

    auto name = name_of(name_node);
    unit_.symbol_table.declare(name,
        unit_.type_table.function_returning(type_word),
        Storage::Function);

    unit_.symbol_table.push_scope();

    auto base = scratch_.size();

    // the name leads the span, so a definition holds what it is called
    scratch_.push_back(
        add(symbol_node(Type::Declaration, unit_.symbol_table.lookup(name)),
            name_node));

    auto parameter_span = node_at(parameters).data.span;
    for (std::uint32_t i = 0; i < parameter_span.count; ++i) {
        auto parameter = tree_.extra[parameter_span.start + i];

        // a parameter is a name, or "*p" when it takes a pointer
        auto parameter_name = parameter;
        Type_Index parameter_type = type_word;
        if (node_at(parameter).type == ast::Type::Indirect_Identifier) {
            parameter_name = node_at(parameter).data.unary;
            parameter_type = unit_.type_table.pointer_to(type_word);
        }

        if (parameter_name == ast::null_node_index or
            node_at(parameter_name).type != ast::Type::Identifier) {
            error(parameter, "a parameter must be a name");
            continue;
        }

        auto symbol = unit_.symbol_table.declare(name_of(parameter_name),
            parameter_type,
            Storage::Parameter,
            0,
            parameter_type != type_word);
        scratch_.push_back(
            add(symbol_node(Type::Declaration, symbol), parameter));
    }

    scratch_.push_back(lower_block(body));
    auto children = commit(base);

    unit_.symbol_table.pop_scope();

    return add(span_node(Type::Function, children), index);
}

/**
 * @brief Lower a vector definition and its initial values
 */
Node_Index Lowering::lower_vector(ast::Node_Index index)
{
    auto span = node_at(index).data.span;
    auto name_node = tree_.extra[span.start];
    auto size_node = tree_.extra[span.start + 1];
    auto values = tree_.extra[span.start + 2];

    auto count = std::uint32_t{ 0 };
    if (size_node != ast::null_node_index and
        node_at(size_node).type == ast::Type::Integer_Literal) {
        auto declared = node_at(size_node).data.integer;
        if (declared < 0)
            error(size_node, "a vector may not have a negative size");
        else
            count = static_cast<std::uint32_t>(declared);
    }

    auto base = scratch_.size();

    // the name leads the span, so a definition holds what it is called
    scratch_.push_back(add(symbol_node(Type::Declaration,
                               unit_.symbol_table.lookup(name_of(name_node))),
        name_node));

    auto value_span = node_at(values).data.span;
    for (std::uint32_t i = 0; i < value_span.count; ++i)
        scratch_.push_back(lower_expression(tree_.extra[value_span.start + i]));
    auto children = commit(base);

    // an unsized vector takes its length from the values it was given
    if (count == 0)
        count = children.count;

    unit_.symbol_table.declare(name_of(name_node),
        unit_.type_table.vector_of(type_word, count),
        Storage::Vector,
        count);

    return add(span_node(Type::Vector, children), index);
}

Node_Index Lowering::lower_union(ast::Node_Index index)
{
    auto span = node_at(index).data.span;
    auto name_node = tree_.extra[span.start];

    unit_.symbol_table.declare(name_of(name_node), type_word, Storage::Global);

    auto base = scratch_.size();
    for (std::uint32_t i = 1; i < span.count; ++i)
        scratch_.push_back(lower_expression(tree_.extra[span.start + i]));
    auto children = commit(base);

    return add(span_node(Type::Union, children), index);
}

/**
 * @brief Lower the statements of a block into one node
 */
Node_Index Lowering::lower_block(ast::Node_Index index)
{
    auto span = node_at(index).data.span;
    auto base = scratch_.size();
    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto child = lower_statement(tree_.extra[span.start + i]);
        if (child != null_node_index)
            scratch_.push_back(child);
    }
    return add(span_node(Type::Block, commit(base)), index);
}

/**
 * @brief Declare the names an auto or extrn statement introduces
 */
Node_Index Lowering::lower_declaration(ast::Node_Index index, Storage storage)
{
    auto span = node_at(index).data.span;
    auto base = scratch_.size();

    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto child = tree_.extra[span.start + i];

        // "auto *p" declares a pointer and not a scalar
        if (node_at(child).type == ast::Type::Indirect_Identifier) {
            auto target = node_at(child).data.unary;
            if (target == ast::null_node_index or
                node_at(target).type != ast::Type::Identifier) {
                error(child, "a pointer declaration must name an lvalue");
                continue;
            }
            auto symbol = unit_.symbol_table.declare(name_of(target),
                unit_.type_table.pointer_to(type_word),
                storage,
                0,
                true);
            scratch_.push_back(
                add(symbol_node(Type::Declaration, symbol), child));
            continue;
        }

        // "auto v[10]" declares a local vector and not a scalar
        if (node_at(child).type == ast::Type::Vector_Identifier) {
            auto base_node = node_at(child).data.binary.lhs;
            auto size_node = node_at(child).data.binary.rhs;
            auto count = std::uint32_t{ 0 };
            if (node_at(size_node).type == ast::Type::Integer_Literal)
                count =
                    static_cast<std::uint32_t>(node_at(size_node).data.integer);
            auto symbol = unit_.symbol_table.declare(name_of(base_node),
                unit_.type_table.vector_of(type_word, count),
                Storage::Vector,
                count);
            scratch_.push_back(
                add(symbol_node(Type::Declaration, symbol), child));
            continue;
        }

        if (node_at(child).type != ast::Type::Identifier) {
            error(child, "a declaration must name an lvalue");
            continue;
        }

        // "extrn v" names something defined elsewhere, which may be a
        // definition at file scope. Where it is, the declaration already
        // knows the shape of the name and is kept, not shadowed.
        if (storage == Storage::Extrn) {
            auto existing = unit_.symbol_table.lookup(name_of(child));
            if (existing != null_symbol_index) {
                scratch_.push_back(
                    add(symbol_node(Type::Declaration, existing), child));
                continue;
            }
        } else if (unit_.symbol_table.declared_in_current_scope(
                       name_of(child))) {
            error(child,
                fmt::format("'{}' is already declared in this scope",
                    unit_.string(name_of(child))));
        }

        auto symbol =
            unit_.symbol_table.declare(name_of(child), type_word, storage);
        scratch_.push_back(add(symbol_node(Type::Declaration, symbol), child));
    }

    // tagged so that the IR can tell a declaration list from a plain block,
    // since one reserves local storage and the other names a global
    auto kind = storage == Storage::Extrn ? Type::Extrn : Type::Auto;
    return add(span_node(kind, commit(base)), index);
}

Node_Index Lowering::lower_statement(ast::Node_Index index)
{
    if (index == ast::null_node_index)
        return null_node_index;

    return m::match(node_at(index).type)(
        m::pattern |
            ast::Type::Block_Statement = [&] { return lower_block(index); },

        m::pattern | ast::Type::Auto_Statement =
            [&] { return lower_declaration(index, Storage::Auto); },

        m::pattern | ast::Type::Extrn_Statement =
            [&] { return lower_declaration(index, Storage::Extrn); },

        m::pattern | ast::Type::Expression_Statement =
            [&] {
                auto span = node_at(index).data.span;
                auto base = scratch_.size();
                for (std::uint32_t i = 0; i < span.count; ++i)
                    scratch_.push_back(
                        lower_expression(tree_.extra[span.start + i]));
                return add(span_node(Type::Expression, commit(base)), index);
            },

        m::pattern | ast::Type::If_Statement =
            [&] {
                auto span = node_at(index).data.span;
                auto base = scratch_.size();
                scratch_.push_back(lower_expression(tree_.extra[span.start]));
                scratch_.push_back(
                    lower_statement(tree_.extra[span.start + 1]));
                scratch_.push_back(
                    lower_statement(tree_.extra[span.start + 2]));
                return add(span_node(Type::If, commit(base)), index);
            },

        m::pattern | ast::Type::While_Statement =
            [&] {
                auto condition =
                    lower_expression(node_at(index).data.binary.lhs);
                auto body = lower_statement(node_at(index).data.binary.rhs);
                return add(
                    binary_node(Type::While, Operator::None, condition, body),
                    index);
            },

        m::pattern | ast::Type::Switch_Statement =
            [&] {
                auto condition =
                    lower_expression(node_at(index).data.binary.lhs);
                auto cases = lower_block(node_at(index).data.binary.rhs);
                return add(
                    binary_node(Type::Switch, Operator::None, condition, cases),
                    index);
            },

        m::pattern | ast::Type::Case_Statement =
            [&] {
                auto value = lower_expression(node_at(index).data.binary.lhs);
                auto body = lower_block(node_at(index).data.binary.rhs);
                return add(binary_node(Type::Case, Operator::None, value, body),
                    index);
            },

        m::pattern | ast::Type::Return_Statement =
            [&] {
                auto value = node_at(index).data.unary == ast::null_node_index
                                 ? null_node_index
                                 : lower_expression(node_at(index).data.unary);
                return add(
                    unary_node(Type::Return, Operator::None, value), index);
            },

        m::pattern | ast::Type::Break_Statement =
            [&] {
                return add(
                    unary_node(Type::Break, Operator::None, null_node_index),
                    index);
            },

        m::pattern | ast::Type::Label_Statement =
            [&] {
                auto symbol = unit_.symbol_table.declare(
                    node_at(index).data.string, type_null, Storage::Label);
                return add(symbol_node(Type::Label, symbol), index);
            },

        m::pattern | ast::Type::Goto_Statement =
            [&] {
                auto name = node_at(index).data.string;
                auto symbol = unit_.symbol_table.lookup(name);
                // a goto may name a label further down the body, so an
                // unknown target is declared here and confirmed by the
                // checker
                if (symbol == null_symbol_index)
                    symbol = unit_.symbol_table.declare(
                        name, type_null, Storage::Label);
                return add(symbol_node(Type::Goto, symbol), index);
            },

        // any remaining statement position holds an expression
        m::pattern | m::_ = [&] { return lower_expression(index); });
}

/**
 * @brief Lower a call, keeping the callee and the arguments together
 */
Node_Index Lowering::lower_call(ast::Node_Index index)
{
    auto callee_node = node_at(index).data.binary.lhs;

    // a call may name a function this unit never defines, such as one from
    // the standard library or another object, which the linker resolves
    if (node_at(callee_node).type == ast::Type::Identifier) {
        auto name = name_of(callee_node);
        if (unit_.symbol_table.lookup(name) == null_symbol_index)
            unit_.symbol_table.declare(name,
                unit_.type_table.function_returning(type_word),
                Storage::Extrn,
                0,
                false,
                true);
    }

    auto callee = lower_expression(callee_node);

    auto arguments = node_at(index).data.binary.rhs;
    auto span = node_at(arguments).data.span;

    auto base = scratch_.size();
    for (std::uint32_t i = 0; i < span.count; ++i)
        scratch_.push_back(lower_expression(tree_.extra[span.start + i]));
    auto lowered = commit(base);

    // the parser's argument block is dropped, and the arguments hang off
    // the call itself
    auto list = add(span_node(Type::Block, lowered), arguments);
    return add(binary_node(Type::Call, Operator::None, callee, list), index);
}

/**
 * @brief Reshape a binary chain so that precedence decides the grouping
 *
 * The chain is flattened down its right hand spine into the operands and
 * operators it holds, then rebuilt with shunting-yard. Everything the
 * spine reaches is lowered exactly once, in source order.
 */
Node_Index Lowering::lower_binary_chain(ast::Node_Index index)
{
    std::vector<ast::Node_Index> operands;
    std::vector<Operator> operators;

    auto current = index;
    while (node_at(current).type == ast::Type::Binary_Expression) {
        operands.push_back(node_at(current).data.binary.lhs);
        operators.push_back(node_at(current).op);
        current = node_at(current).data.binary.rhs;
    }
    operands.push_back(current);

    std::vector<Node_Index> output;
    std::vector<Operator> stack;
    std::vector<ast::Node_Index> sources;

    auto combine = [&]() {
        auto op = stack.back();
        stack.pop_back();
        auto rhs = output.back();
        output.pop_back();
        auto lhs = output.back();
        output.pop_back();
        auto source = sources.back();
        sources.pop_back();
        output.push_back(add(binary_node(Type::Binary, op, lhs, rhs), source));
    };

    for (std::size_t i = 0; i < operands.size(); ++i) {
        output.push_back(lower_expression(operands[i]));

        if (i >= operators.size())
            continue;

        auto op = operators[i];
        while (!stack.empty()) {
            auto top = stack.back();
            // a lower number binds tighter, so an operator already on the
            // stack is reduced when it binds at least as tightly
            auto tighter = precedence_of(top) < precedence_of(op);
            auto equal = precedence_of(top) == precedence_of(op);
            if (tighter or (equal and is_left_associative(op)))
                combine();
            else
                break;
        }
        stack.push_back(op);
        sources.push_back(index);
    }

    while (!stack.empty())
        combine();

    return output.empty() ? null_node_index : output.front();
}

Node_Index Lowering::lower_expression(ast::Node_Index index)
{
    if (index == ast::null_node_index)
        return null_node_index;

    auto const& node = node_at(index);

    return m::match(node.type)(
        // parentheses have done their work in the parser and carry nothing
        m::pattern | ast::Type::Evaluated_Expression =
            [&] { return lower_expression(node.data.unary); },

        m::pattern | ast::Type::Binary_Expression =
            [&] { return lower_binary_chain(index); },

        m::pattern |
            ast::Type::Function_Expression = [&] { return lower_call(index); },

        m::pattern | ast::Type::Assignment_Expression =
            [&] {
                auto target = lower_expression(node.data.binary.lhs);
                auto value = lower_expression(node.data.binary.rhs);
                return add(
                    binary_node(Type::Assign, Operator::Assign, target, value),
                    index);
            },

        m::pattern | ast::Type::Ternary_Expression =
            [&] {
                auto span = node.data.span;
                auto base = scratch_.size();
                for (std::uint32_t i = 0; i < span.count; ++i)
                    scratch_.push_back(
                        lower_expression(tree_.extra[span.start + i]));
                return add(span_node(Type::Ternary, commit(base)), index);
            },

        m::pattern | ast::Type::Unary_Expression =
            [&] {
                return add(unary_node(Type::Unary,
                               node.op,
                               lower_expression(node.data.unary)),
                    index);
            },

        m::pattern | ast::Type::Address_Of_Expression =
            [&] {
                return add(unary_node(Type::Address_Of,
                               Operator::Address_Of,
                               lower_expression(node.data.unary)),
                    index);
            },

        m::pattern | ast::Type::Indirect_Identifier =
            [&] { return lower_dereference(index); },

        m::pattern | ast::Type::Pre_Inc_Dec_Expression =
            [&] {
                return add(unary_node(Type::Pre_Inc_Dec,
                               node.op,
                               lower_expression(node.data.unary)),
                    index);
            },

        m::pattern | ast::Type::Post_Inc_Dec_Expression =
            [&] {
                return add(unary_node(Type::Post_Inc_Dec,
                               node.op,
                               lower_expression(node.data.unary)),
                    index);
            },

        m::pattern | ast::Type::Vector_Identifier =
            [&] {
                auto base_node = lower_expression(node.data.binary.lhs);
                auto subscript = lower_expression(node.data.binary.rhs);
                return add(
                    binary_node(
                        Type::Subscript, Operator::None, base_node, subscript),
                    index);
            },

        m::pattern | ast::Type::Identifier =
            [&] {
                auto symbol = unit_.symbol_table.lookup(node.data.string);
                if (symbol == null_symbol_index) {
                    error(index,
                        fmt::format("'{}' was not declared",
                            unit_.string(node.data.string)));
                    // carry on with an unresolved reference so that
                    // lowering reports every undeclared name instead of
                    // only the first
                    symbol = unit_.symbol_table.declare(
                        node.data.string, null_type_index, Storage::Extrn);
                }
                return add(symbol_node(Type::Symbol_Ref, symbol), index);
            },

        m::pattern | ast::Type::Integer_Literal =
            [&] {
                Node literal{};
                literal.type = Type::Integer;
                literal.op = Operator::None;
                literal.data.integer = node.data.integer;
                return add(literal, index);
            },

        m::pattern | ast::Type::Double_Literal =
            [&] {
                Node literal{};
                literal.type = Type::Double;
                literal.op = Operator::None;
                literal.data.real = node.data.real;
                return add(literal, index);
            },

        m::pattern | ast::Type::Float_Literal =
            [&] {
                Node literal{};
                literal.type = Type::Float;
                literal.op = Operator::None;
                literal.data.real = node.data.real;
                return add(literal, index);
            },

        m::pattern | ast::Type::Bool_Literal =
            [&] {
                Node literal{};
                literal.type = Type::Bool;
                literal.op = Operator::None;
                literal.data.string = node.data.string;
                return add(literal, index);
            },

        m::pattern | ast::Type::String_Literal =
            [&] {
                Node literal{};
                literal.type = Type::String;
                literal.op = Operator::None;
                literal.data.string = node.data.string;
                return add(literal, index);
            },

        m::pattern | ast::Type::Char_Literal =
            [&] {
                Node literal{};
                literal.type = Type::Char;
                literal.op = Operator::None;
                literal.data.string = node.data.string;
                return add(literal, index);
            },

        m::pattern | m::_ =
            [&] {
                error(index, "expected an expression");
                return null_node_index;
            });
}

/**
 * @brief Lower a dereference, which may stand for an assignment through it
 *
 * "*p = v" parses as an indirection over the whole assignment, since the
 * dereference takes an rvalue and the assignment is reduced first. What it
 * means is an assignment through the pointer, so the two are swapped back
 * here and not left for a later pass to work out.
 */
Node_Index Lowering::lower_dereference(ast::Node_Index index)
{
    auto operand = node_at(index).data.unary;

    if (operand != ast::null_node_index and
        node_at(operand).type == ast::Type::Assignment_Expression) {
        auto through =
            add(unary_node(Type::Dereference,
                    Operator::Indirection,
                    lower_expression(node_at(operand).data.binary.lhs)),
                index);
        auto value = lower_expression(node_at(operand).data.binary.rhs);
        return add(binary_node(Type::Assign, Operator::Assign, through, value),
            operand);
    }

    return add(unary_node(Type::Dereference,
                   Operator::Indirection,
                   lower_expression(operand)),
        index);
}

} // namespace detail

std::string_view Unit::string(ast::String_Index handle) const
{
    if (handle == ast::null_string_index or handle >= strings.size())
        return {};
    auto const& entry = strings[handle];
    return std::string_view(string_text.data() + entry.offset, entry.length);
}

std::string_view Unit::symbol_name(Symbol_Index index) const
{
    if (index == null_symbol_index or index >= symbol_table.symbols().size())
        return {};
    return string(symbol_table.at(index).name);
}

Span Unit::subtree(Node_Index index) const
{
    if (index == null_node_index or index >= nodes.size())
        return Span{ 0, 0 };
    auto start = first[index];
    return Span{ start, index - start + 1 };
}

/**
 * @brief The precedence of an operator, where a lower number binds tighter
 */
unsigned int precedence_of(Operator op)
{
    switch (op) {
        case Operator::Inc:
        case Operator::Dec:
            return 1;

        case Operator::Minus:
        case Operator::Plus:
        case Operator::Not:
        case Operator::Ones_Complement:
        case Operator::Address_Of:
        case Operator::Indirection:
            return 2;

        case Operator::Mul:
        case Operator::Div:
        case Operator::Mod:
            return 3;

        case Operator::Add:
        case Operator::Sub:
            return 4;

        case Operator::Lshift:
        case Operator::Rshift:
            return 5;

        case Operator::Lt:
        case Operator::Lte:
        case Operator::Gt:
        case Operator::Gte:
            return 6;

        case Operator::Eq:
        case Operator::Neq:
            return 7;

        case Operator::Bit_And:
            return 8;

        case Operator::Xor:
            return 9;

        case Operator::Bit_Or:
            return 10;

        case Operator::And:
            return 11;

        case Operator::Or:
            return 12;

        case Operator::Assign:
            return 14;

        case Operator::None:
            return 15;
    }
    return 15;
}

bool is_left_associative(Operator op)
{
    switch (op) {
        case Operator::Inc:
        case Operator::Dec:
        case Operator::Minus:
        case Operator::Plus:
        case Operator::Not:
        case Operator::Ones_Complement:
        case Operator::Address_Of:
        case Operator::Indirection:
        case Operator::Assign:
            return false;
        default:
            return true;
    }
}

/**
 * @brief Which member of Node::Data a node type keeps live
 */
Payload payload_of(Type type)
{
    switch (type) {
        case Type::Function:
        case Type::Vector:
        case Type::Union:
        case Type::Block:
        case Type::Expression:
        case Type::Auto:
        case Type::Extrn:
        case Type::If:
        case Type::Ternary:
            return Payload::Span;

        case Type::While:
        case Type::Switch:
        case Type::Case:
        case Type::Binary:
        case Type::Assign:
        case Type::Call:
        case Type::Subscript:
            return Payload::Binary;

        case Type::Return:
        case Type::Break:
        case Type::Unary:
        case Type::Address_Of:
        case Type::Dereference:
        case Type::Pre_Inc_Dec:
        case Type::Post_Inc_Dec:
            return Payload::Unary;

        case Type::Symbol_Ref:
        case Type::Label:
        case Type::Goto:
        case Type::Declaration:
            return Payload::Symbol;

        case Type::String:
        case Type::Char:
        case Type::Bool:
            return Payload::String;

        case Type::Integer:
            return Payload::Integer;

        case Type::Double:
        case Type::Float:
            return Payload::Double;
    }
    return Payload::None;
}

std::string_view type_to_string(Type type)
{
    switch (type) {
        case Type::Function:
            return "function";
        case Type::Vector:
            return "vector";
        case Type::Union:
            return "union";
        case Type::Block:
            return "block";
        case Type::Expression:
            return "expression";
        case Type::If:
            return "if";
        case Type::While:
            return "while";
        case Type::Switch:
            return "switch";
        case Type::Case:
            return "case";
        case Type::Goto:
            return "goto";
        case Type::Label:
            return "label";
        case Type::Return:
            return "return";
        case Type::Break:
            return "break";
        case Type::Auto:
            return "auto";
        case Type::Extrn:
            return "extrn";
        case Type::Declaration:
            return "declaration";
        case Type::Binary:
            return "binary";
        case Type::Unary:
            return "unary";
        case Type::Assign:
            return "assign";
        case Type::Ternary:
            return "ternary";
        case Type::Call:
            return "call";
        case Type::Subscript:
            return "subscript";
        case Type::Address_Of:
            return "address_of";
        case Type::Dereference:
            return "dereference";
        case Type::Pre_Inc_Dec:
            return "pre_inc_dec";
        case Type::Post_Inc_Dec:
            return "post_inc_dec";
        case Type::Symbol_Ref:
            return "symbol_ref";
        case Type::Integer:
            return "integer";
        case Type::Double:
            return "double";
        case Type::Float:
            return "float";
        case Type::String:
            return "string";
        case Type::Char:
            return "char";
        case Type::Bool:
            return "bool";
    }
    return "unknown";
}

Result lower(ast::AST const& tree)
{
    detail::Lowering lowering{ tree };
    return lowering.run();
}

} // namespace credence::frontend::hir
