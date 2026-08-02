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

#include <credence/frontend/compile.h>

#include <credence/frontend/hir/address.h> // for resolve_addresses
#include <credence/frontend/hir/check.h>   // for check
#include <credence/frontend/parser.h>      // for Parser
#include <fmt/format.h>                    // for format
#include <ostream>                         // for ostream
#include <utility>                         // for move

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

Program compile(std::string source)
{
    auto tree = Parser::parse(std::move(source));

    auto lowered = hir::lower(tree);
    auto checked = hir::check(lowered.unit);
    auto addressed = hir::resolve_addresses(lowered.unit);

    // every pass runs before any of them is acted on, so one call reports
    // every error in a program and not only the first one found
    auto diagnostics = std::move(lowered.diagnostics);
    diagnostics.insert(diagnostics.end(), checked.begin(), checked.end());
    diagnostics.insert(diagnostics.end(), addressed.begin(), addressed.end());

    return Program{
        std::move(tree), std::move(lowered.unit), std::move(diagnostics)
    };
}

void report(std::ostream& os, Program const& program)
{
    for (auto const& diagnostic : program.diagnostics)
        os << fmt::format("{}:{}: {}",
                  diagnostic.line,
                  diagnostic.column,
                  diagnostic.message)
           << std::endl;
}

} // namespace credence::frontend
