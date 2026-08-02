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

#include <credence/frontend/hir/serialize.h>

#include <fmt/format.h> // for format
#include <matchit.h>    // for match, pattern
#include <ostream>      // for ostream
#include <string>       // for string

/****************************************************************************
 *
 * HIR serializer
 *
 * Writes a lowered unit as an indented outline, the same shape the AST
 * serializer produces, with the resolved type of each node beside it. Meant
 * to be read by a person and diffed by a test.
 *
 * Reading a dump beside the AST dump of the same source is the quickest way
 * to see what lowering did - a chain grouped by source order in the AST is
 * grouped by precedence here.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

namespace m = matchit;

namespace {

constexpr int indent_width = 2;

/**
 * @brief Write the value or the name of a node, if it has one
 */
void write_payload(std::ostream& os, Unit const& unit, Node const& node)
{
    switch (payload_of(node.type)) {
        case Payload::Symbol: {
            auto name = unit.symbol_name(node.data.symbol);
            auto const& declared = unit.symbol_table.at(node.data.symbol);
            os << fmt::format(
                " \"{}\" [{}]", name, storage_to_string(declared.storage));
            break;
        }
        case Payload::String:
            os << fmt::format(" \"{}\"", unit.string(node.data.string));
            break;
        case Payload::Integer:
            os << ' ' << node.data.integer;
            break;
        case Payload::Double:
            os << ' ' << fmt::format("{}", node.data.real);
            break;
        default:
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

    auto const& node = unit.nodes[index];

    if (options.indices)
        os << fmt::format("[{}] ", index);

    os << pad << type_to_string(node.type);

    if (node.op != ast::Operator::None)
        os << " \"" << ast::operator_to_string(node.op) << '"';

    write_payload(os, unit, node);

    if (options.types) {
        os << fmt::format(
            " : {}", type_to_string(unit.type_table, unit.types[index]));
    }

    os << '\n';

    m::match(payload_of(node.type))(
        m::pattern | Payload::Span =
            [&] {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i)
                    visit(unit.extra[span.start + i], depth + 1);
            },
        m::pattern | Payload::Binary =
            [&] {
                visit(node.data.binary.lhs, depth + 1);
                visit(node.data.binary.rhs, depth + 1);
            },
        m::pattern | Payload::Unary =
            [&] {
                if (node.data.unary != null_node_index)
                    visit(node.data.unary, depth + 1);
            },
        m::pattern | m::_ = [] {});
}

void dump_node(std::ostream& os,
    Unit const& unit,
    Node_Index index,
    Dump_Options options)
{
    detail::Walk{ os, unit, options }.visit(index, 0);
}

void dump(std::ostream& os, Unit const& unit, Dump_Options options)
{
    if (unit.definitions.empty()) {
        os << "<empty>\n";
        return;
    }
    for (auto definition : unit.definitions)
        dump_node(os, unit, definition, options);
}

/**
 * @brief Write the linear form of an expression, as the IR consumes it
 */
void dump_linear(std::ostream& os, Unit const& unit, Node_Index index)
{
    auto range = unit.subtree(index);
    for (std::uint32_t i = 0; i < range.count; ++i) {
        auto const& node = unit.nodes[range.start + i];
        if (i > 0)
            os << ' ';
        os << type_to_string(node.type);
        if (node.op != ast::Operator::None)
            os << '(' << ast::operator_to_string(node.op) << ')';
        switch (payload_of(node.type)) {
            case Payload::Symbol:
                os << ':' << unit.symbol_name(node.data.symbol);
                break;
            case Payload::Integer:
                os << ':' << node.data.integer;
                break;
            default:
                break;
        }
    }
    os << '\n';
}

} // namespace credence::frontend::hir
