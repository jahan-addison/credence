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

#include <credence/frontend/ast.h>     // for AST
#include <credence/frontend/hir/hir.h> // for Unit, Diagnostic
#include <iosfwd>                      // for ostream
#include <string>                      // for string
#include <vector>                      // for vector

/****************************************************************************
 *
 * Frontend
 *
 * Runs every frontend pass over a source file and returns the parsed tree,
 * the lowered unit, and the diagnostics of all four passes:
 *
 *    parse    source to the flat AST
 *    lower    precedence, symbols, and useless syntax
 *    check    a type for every node, and the uses a declaration forbids
 *    address  what each expression addresses, and the literals that need
 *             storage
 *
 * Every pass runs before any diagnostic is acted on, so one call reports
 * every error in a program and not only the first one found.
 *
 * The tree is kept beside the unit for callers that want the program as
 * written and not as lowered, which is what the ast target prints.
 *
 *****************************************************************************/

namespace credence::frontend {

/**
 * @brief A source file taken as far as the frontend takes it
 */
struct Program
{
    ast::AST tree;
    hir::Unit unit;
    std::vector<hir::Diagnostic> diagnostics;

    /**
     * @brief Whether any pass rejected the program
     */
    bool failed() const { return !diagnostics.empty(); }
};

/**
 * @brief Run every frontend pass over a source file
 */
Program compile(std::string source);

/**
 * @brief Write the diagnostics of a program, one per line
 */
void report(std::ostream& os, Program const& program);

} // namespace credence::frontend
