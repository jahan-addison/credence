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

#include "hir.h"

#include "literal.h"       // for literal_to_string, Literal, Array
#include "operators.h"     // for Operator
#include <credence/util.h> // for overload
#include <sstream>         // for basic_ostringstream, ostringstream
#include <string>          // for basic_string, string
#include <string_view>     // for basic_string_view, string_view
#include <variant>         // for monostate, visit

/****************************************************************************
 *
 * High level intermediate representation
 *
 * The HIR is a tree of Data_Kind nodes. AST_Lowering (ast_lowering.h)
 * lowers the Parser's AST_Node expressions into it, and Shunting_Yard
 * (precedence.h) orders it into a queue for the IR.
 *
 * Data_Kind::Kind is the algebraic sum of every expression shape the rest
 * of the compiler cares about - Literal, Array, Symbol, Unary, Relation,
 * Function, and LValue. The leaves are the ( Value : Type : Size ) tuple
 * from literal.h.
 *
 * "Type" is kept for the middle of that tuple, the data type of a value.
 * "Kind" is which expression shape a node is.
 *
 *  Example:
 *
 *   main() {
 *     auto x;
 *     x = 5 + 3 * 2;
 *   }
 *
 *   HIR:  Symbol(LValue("x"),
 *                Relation(B_ADD,
 *                         Literal(5:int:4),
 *                         Relation(B_MUL, ...)))
 *
 *****************************************************************************/

namespace credence::language::hir {

/**
 * @brief Data_Kind types to string in reverse polish notation
 */
std::string data_kind_to_string(Data_Kind::Kind const& item,
    bool separate,
    std::string_view separator)
{
    auto oss = std::ostringstream();
    auto space = separate ? " " : "";
    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](Data_Kind::Pointer const&) {},
            [&](literal::Literal const& s) {
                oss << literal::literal_to_string(s, separator) << space;
            },
            [&](literal::Array const& s) {
                for (auto const& value : s) {
                    oss << literal::literal_to_string(value, separator)
                        << space;
                }
            },
            [&](Data_Kind::LValue const& s) { oss << s.first << space; },
            [&](Data_Kind::Unary const& s) {
                oss << s.first
                    << data_kind_to_string(s.second->value, true, separator)
                    << space;
            },
            [&](Data_Kind::Relation const& s) {
                for (auto const& relation : s.second) {
                    oss << data_kind_to_string(relation->value, true, separator)
                        << space;
                }
            },
            [&](Data_Kind::Function const& s) {
                oss << s.first.first << space;
            },
            [&](Data_Kind::Symbol const& s) {
                oss << s.first.first << space;
            } },
        item);
    return oss.str();
}

} // namespace hir
