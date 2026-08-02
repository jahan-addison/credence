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

#include <credence/frontend/hir/hir.h> // for Unit, Diagnostic
#include <string>                      // for string
#include <vector>                      // for vector

/****************************************************************************
 *
 * Address resolution
 *
 * Resolves what an expression addresses:
 *
 *    - a vector used as a value decays to a pointer to its first element,
 *      recorded as the type of that use
 *    - a subscript with a constant index is folded to a byte offset, so a
 *      backend reads a number and not an index with an element width
 *    - every string literal is collected, as one needs storage before its
 *      address can be taken
 *
 * Decay is a property of a use and not of a declaration. The symbol keeps
 * its vector type and the use is typed as a pointer, so both facts remain
 * available. Nothing is inserted into the node array, so an index resolved
 * by an earlier pass stays valid.
 *
 * A vector is not decayed where it is the base of a subscript or the
 * operand of an address-of, as both of those want the vector itself.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

namespace detail {

/**
 * @brief The pass that resolves what each expression addresses
 */
class Addresses
{
  public:
    explicit Addresses(Unit& unit)
        : unit_(unit)
    {
    }

    std::vector<Diagnostic> run();

  private:
    void mark_wanted_whole();
    void resolve_subscript(Node_Index index);
    void decay_vector_use(Node_Index index);

    void error(Node_Index index, std::string message);

  private:
    Unit& unit_;
    std::vector<Diagnostic> diagnostics_{};

    // nodes whose vector operand is wanted as itself and not decayed
    std::vector<bool> whole_{};
};

} // namespace detail

/**
 * @brief Settle what each expression addresses, filling offsets and
 *        collecting the string literals of the unit
 *
 * The unit is written in place. The returned diagnostics are empty when
 * every address resolves.
 */
std::vector<Diagnostic> resolve_addresses(Unit& unit);

} // namespace credence::frontend::hir
