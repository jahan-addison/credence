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
 * Type checker
 *
 * Assigns a type to every HIR node and reports the uses a declaration does
 * not allow. Nodes are in post-order, so the pass is a forward loop and not
 * a walk - every operand of a node has already been given a type when the
 * node itself is reached, and the result is written into the types array
 * beside it.
 *
 * What it rejects:
 *
 *    - arithmetic on a type that cannot take part in it
 *    - a bitwise or shift expression on a floating operand
 *    - subscripting something that is neither a vector nor a pointer
 *    - dereferencing something that does not address memory
 *    - taking the address of a value that has no storage
 *    - calling a name that is not a function
 *    - a subscript on a vector whose constant index is out of range
 *    - a goto naming a label that no statement defines
 *
 ****************************************************************************/

namespace credence::frontend::hir {

namespace detail {

/**
 * @brief The pass that types a unit and collects what it rejects
 */
class Checker
{
  public:
    explicit Checker(Unit& unit)
        : unit_(unit)
    {
    }

    std::vector<Diagnostic> run();

  private:
    void check_node(Node_Index index);
    void check_binary(Node_Index index);
    void check_assign(Node_Index index);
    void check_pointer_assign(Node_Index index,
        Node_Index target,
        Node_Index value);
    void check_subscript(Node_Index index);
    void check_address_of(Node_Index index);
    void check_dereference(Node_Index index);
    void check_unary(Node_Index index);
    void check_ternary(Node_Index index);
    void check_call(Node_Index index);
    void check_labels();

    void error(Node_Index index, std::string message);

    Type_Index type_of(Node_Index index) const
    {
        return index == null_node_index ? null_type_index : unit_.types[index];
    }

    /**
     * @brief Whether a node names storage that can be assigned or addressed
     */
    bool is_lvalue(Node_Index index) const
    {
        if (index == null_node_index)
            return false;
        switch (unit_.nodes[index].type) {
            case Type::Symbol_Ref:
            case Type::Subscript:
            case Type::Dereference:
                return true;
            default:
                return false;
        }
    }

  private:
    Unit& unit_;
    std::vector<Diagnostic> diagnostics_{};
};

} // namespace detail

/**
 * @brief Assign types across a unit and report what it rejects
 *
 * The unit is written in place, filling its types array. The returned
 * diagnostics are empty when the unit checks.
 */
std::vector<Diagnostic> check(Unit& unit);

} // namespace credence::frontend::hir
