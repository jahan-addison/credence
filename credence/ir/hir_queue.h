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

#include <credence/frontend/ast.h>     // for String_Index
#include <credence/frontend/hir/hir.h> // for Node_Index
#include <credence/ir/queue.h>         // for Queue
#include <string>                      // for string, basic_string

/****************************************************************************
 *
 * HIR expression to instruction queue
 *
 * An HIR expression is already in the order an operand stack consumes, so
 * the queue the temporary constructor reads is a walk of the subtree range
 * and not a fresh ordering of it.
 *
 * The queue holds an operator or an operand per item, and the temporary
 * constructor in temporary.h turns that into the three and four tuple
 * instructions of the IR:
 *
 *   x = a * b + c
 *
 *   queue     x a b * c + =
 *
 *   becomes   _t1 = a * b;
 *             _t2 = _t1 + c;
 *             x = _t2;
 *
 *****************************************************************************/

namespace credence::ir {

namespace hir = credence::frontend::hir;

namespace detail {

/**
 * @brief The bytes of a string literal
 */
std::string string_value_of(frontend::ast::String_Index handle,
    hir::Unit const& unit);

/**
 * @brief The walk that turns a subtree range into a queue
 */
class Builder
{
  public:
    Builder(hir::Unit const& unit,
        Queue& queue,
        int* parameter,
        int* identifier)
        : unit_(unit)
        , queue_(queue)
        , parameter_(parameter)
        , identifier_(identifier)
    {
    }

    void visit(hir::Node_Index index);

  private:
    void visit_children(hir::Node_Index index);
    std::string subscript_name(hir::Node_Index index) const;
    std::string dereference_name(hir::Node_Index index) const;
    std::string string_value_of(frontend::ast::String_Index handle) const
    {
        return detail::string_value_of(handle, unit_);
    }

  private:
    hir::Unit const& unit_;
    Queue& queue_;
    int* parameter_;
    int* identifier_;
};

} // namespace detail

/**
 * @brief Build the operand and operator queue of an HIR expression
 */
Queue queue_from_hir(hir::Unit const& unit,
    hir::Node_Index index,
    int* parameter,
    int* identifier);

} // namespace credence::ir
