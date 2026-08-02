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

#pragma once

#include <credence/frontend/hir/hir.h> // for Unit, Node_Index
#include <iosfwd>                      // for ostream

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

/**
 * @brief What a dump holds beyond the shape of the unit
 */
struct Dump_Options
{
    // Prefix each line with the node's index into Unit::nodes
    bool indices{ false };
    // Append the resolved type of each node
    bool types{ true };
};

namespace detail {

/**
 * @brief The walk that prints one node per line
 */
struct Walk
{
    // the walk is an aggregate, built at the one place it is used
    // cppcheck-suppress uninitMemberVarNoCtor
    std::ostream& os;
    Unit const& unit;
    Dump_Options options;

    void visit(Node_Index index, int depth) const;
};

} // namespace detail

/**
 * @brief Write every definition of a unit to an output stream
 */
void dump(std::ostream& os, Unit const& unit, Dump_Options options = {});

/**
 * @brief Write one subtree to an output stream
 */
void dump_node(std::ostream& os,
    Unit const& unit,
    Node_Index index,
    Dump_Options options = {});

/**
 * @brief Write the linear form of an expression, as the IR consumes it
 *
 * The nodes of a subtree are already in the order an operand stack wants,
 * so this is the subtree range printed left to right.
 */
void dump_linear(std::ostream& os, Unit const& unit, Node_Index index);

} // namespace credence::frontend::hir
