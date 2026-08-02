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

#include <credence/frontend/serialize.h>

#include <credence/frontend/ast.h> // for AST, Node, Payload, Type
#include <fmt/format.h>            // for format_to, format
#include <matchit.h>               // for match, pattern
#include <ostream>                 // for ostream
#include <string>                  // for string

/****************************************************************************
 *
 * AST serializer
 *
 * Writes the tree as an indented outline, one node per line, children
 * indented under their parent. The output is meant to be read by a person
 * and diffed by a test, so it holds the shape and the values of the tree
 * and nothing else.
 *
 * Example:
 *
 *    main() {
 *      auto x;
 *      x = 1 + 2;
 *    }
 *
 *  Dumps as:
 *
 *    program
 *      function_definition
 *        identifier "main"
 *        block_statement
 *        block_statement
 *          auto_statement
 *            identifier "x"
 *          block_statement
 *            expression_statement
 *              assignment_expression "="
 *                identifier "x"
 *                binary_expression "+"
 *                  integer_literal 1
 *                  integer_literal 2
 *
 * Node indices are left out by default. They renumber whenever the parser
 * changes the order it appends nodes in, which would churn every golden
 * file for a change that did not alter the tree. Turn them on, along with
 * line and column, when reading a dump to debug the parser itself.
 *
 *****************************************************************************/

namespace credence::frontend::ast {

namespace m = matchit;

namespace {

constexpr int indent_width = 2;

/**
 * @brief Write a string with the escapes a B source file would carry
 *
 * Interned text is stored raw, so a literal holding a newline would break
 * the one node per line layout of the dump.
 */
void write_escaped(std::ostream& os, std::string_view text)
{
    for (char c : text) {
        switch (c) {
            case '\n':
                os << "\\n";
                break;
            case '\t':
                os << "\\t";
                break;
            case '\r':
                os << "\\r";
                break;
            case '"':
                os << "\\\"";
                break;
            case '\\':
                os << "\\\\";
                break;
            default:
                os << c;
                break;
        }
    }
}

/**
 * @brief Write the value of a node, if it has one
 */
void write_payload(std::ostream& os, AST const& tree, Node const& node)
{
    switch (payload_of(node.type)) {
        case Payload::String:
            os << " \"";
            write_escaped(os, tree.string(node.data.string));
            os << '"';
            break;
        case Payload::Integer:
            os << ' ' << node.data.integer;
            break;
        case Payload::Real:
            os << ' ' << fmt::format("{}", node.data.real);
            break;
        case Payload::None:
        case Payload::Binary:
        case Payload::Span:
        case Payload::Unary:
            break;
    }
}

} // namespace

/**
 * @brief Print one node and then its children
 */
void detail::Walk::visit(Node_Index index, int depth) const
{
    std::string pad(static_cast<std::size_t>(depth * indent_width), ' ');

    if (index == null_node_index) {
        if (options.indices)
            os << "[-] ";
        os << pad << "<null>\n";
        return;
    }

    auto const& node = tree.nodes[index];

    if (options.indices)
        os << fmt::format("[{}] ", index);

    os << pad << type_to_string(node.type);

    if (node.op != Operator::None)
        os << " \"" << operator_to_string(node.op) << '"';

    write_payload(os, tree, node);

    if (options.positions and index < tree.metadata.size()) {
        auto const& meta = tree.metadata[index];
        os << fmt::format(" @{}:{}", meta.line, meta.column);
    }

    os << '\n';

    m::match(payload_of(node.type))(
        m::pattern | Payload::Span =
            [&] {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i)
                    visit(tree.extra[span.start + i], depth + 1);
            },
        m::pattern | Payload::Binary =
            [&] {
                visit(node.data.binary.lhs, depth + 1);
                visit(node.data.binary.rhs, depth + 1);
            },
        m::pattern | Payload::Unary =
            [&] {
                // an absent operand, such as a bare return, prints nothing
                // and not a null placeholder
                if (node.data.unary != null_node_index)
                    visit(node.data.unary, depth + 1);
            },
        m::pattern | m::_ = [] {});
}

/**
 * @brief Write one subtree to an output stream
 */
void dump_node(std::ostream& os,
    AST const& tree,
    Node_Index index,
    Dump_Options options)
{
    detail::Walk{ os, tree, options }.visit(index, 0);
}

/**
 * @brief Write the whole tree to an output stream
 */
void dump(std::ostream& os, AST const& tree, Dump_Options options)
{
    if (tree.root == null_node_index) {
        os << "<empty>\n";
        return;
    }
    dump_node(os, tree, tree.root, options);
}

/**
 * @brief Write the whole tree with default options
 */
std::ostream& operator<<(std::ostream& os, AST const& tree)
{
    dump(os, tree);
    return os;
}

} // namespace credence::frontend::ast
