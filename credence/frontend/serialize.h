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

#include <credence/frontend/ast.h> // for AST, Node_Index
#include <iosfwd>                  // for ostream

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

/**
 * @brief What a dump holds beyond the shape and values of the tree
 */
struct Dump_Options
{
    // Prefix each line with the node's index into AST::nodes
    bool indices{ false };
    // Append the source line and column of each node
    bool positions{ false };
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
    AST const& tree;
    Dump_Options options;

    void visit(Node_Index index, int depth) const;
};

} // namespace detail

/**
 * @brief Write the whole tree to an output stream
 */
void dump(std::ostream& os, AST const& tree, Dump_Options options = {});

/**
 * @brief Write one subtree to an output stream
 */
void dump_node(std::ostream& os,
    AST const& tree,
    Node_Index index,
    Dump_Options options = {});

/**
 * @brief Write the whole tree with default options
 */
std::ostream& operator<<(std::ostream& os, AST const& tree);

} // namespace credence::frontend::ast
