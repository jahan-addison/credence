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

#include <credence/ir/operand.h>   // for Datatype
#include <credence/ir/operators.h> // for Operator
#include <deque>                   // for deque
#include <variant>                 // for variant

/****************************************************************************
 *
 * Expression queue
 *
 * The linear form of an expression that the temporary constructor reads.
 * Each item is either an operator or an operand, in the order an operand
 * stack consumes them:
 *
 *   x = a * b + c   becomes   x a b * c + =
 *
 * The queue is built from the HIR in hir_queue.h.
 *
 *****************************************************************************/

namespace credence::ir {

using Queue_Operand = operand::Operand::Type_Pointer;
using Queue_Item = std::variant<operators::Operator, Queue_Operand>;
using Queue = std::deque<Queue_Item>;

} // namespace credence::ir
