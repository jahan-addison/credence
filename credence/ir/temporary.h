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

#include <algorithm>            // for __find, find
#include <credence/ir/ita.h>    // for Instructions
#include <credence/operators.h> // for Operator
#include <credence/queue.h>     // for Queue
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/util.h>      // for AST_Node
#include <credence/values.h>    // for Expression, Size
#include <initializer_list>     // for initializer_list
#include <stack>                // for stack
#include <string>               // for basic_string, string
#include <utility>              // for pair
#include <vector>               // for vector

/****************************************************************************
 *  A set of algorithms that construct temporary lvalues "_tX" that aid in
 *  breaking expressions into 3- or 4- tuples for linear instructions. The
 *  rvalue stack should be ordered by operator precedence.
 *
 *  Example:
 *
 *  main() {
 *    auto x;
 *    x = (5 + 5) * (6 + 6);
 *  }
 *
 *  Becomes:
 *
 *  __main:
 *   BeginFunc ;
 *.   _t1 = (5:int:4) + (5:int:4);
 *.   _t2 = (6:int:4) + (6:int:4);
 *.   _t3 = _t1 * _t2;
 *.   x = _t3;
 *   EndFunc ;
 *
 ****************************************************************************/

namespace credence {

namespace ir {

using Expression_Instructions =
    std::pair<ir::Instructions, queue::detail::Queue::Container>;
using Temporary_Instructions = std::pair<std::string, ir::Instructions>;

namespace detail {

/**
 * @brief
 * Binary and Unary operators and temporary stack to instructions
 *
 * Consider the expression `(x > 1 || x < 1)`
 *
 * In order to express this in a set of 3 or 4 expressions, we create the
 * following temporaries:
 *
 * clang-format off
 *
 *  _t1 = x > 1
 *  _t2 = x < 1
 *  _t3 = _t1 || _t2
 *
 * clang-format on
 *
 * The binary operation temporary is "_t3", which we return.
 *
 * We must also keep note of evaluated expressions, i.e wrapped in
 * parenthesis:
 *
 * `(5 + 5) * (6 * 6)`
 *
 * Becomes:
 *
 * clang-format off
 *
 * _t1 = (5:int:4) + (5:int:4);
 * _t2 = (6:int:4) + (6:int:4);
 * _t3 = _t1 * _t2;
 *
 * clang-format on
 *
 * If there is a stack of temporaries from a sub-expression in an operand,
 * we pop them and use the last temporary's idenfitier for the instruction
 * name of the top of the temporary stack.
 *
 * I.e., the sub-expressions `x > 1` and `x < 1` were popped off the
 * temporary stack, which were assigned _t1 and _t2, and used in _t3
 * for the final binary expression.
 */
class Temporary
{
  public:
    Temporary() = delete;
    explicit Temporary(int* index)
        : temporary_index(index)
    {
    }

  public:
    using Operand = value::Expression::Type_Pointer;
    using Operands = std::vector<Operand>;
    using Operator = type::Operator;
    using Instructions = ir::Instructions;
    using Operand_Stack = std::stack<Operand>;
    using Temporary_Stack = std::stack<std::string>;

    value::Size insert_and_create_temporary_from_operand(Operand& operand);

  public:
    void unary_operand_to_temporary_stack(type::Operator op);
    void assignment_operands_to_temporary_stack();
    void binary_operands_to_temporary_stack(type::Operator op);
    void binary_operands_balanced_temporary_stack(type::Operator op);
    void binary_operands_unbalanced_temporary_stack(type::Operator op);

  public:
    void from_call_operands_to_temporary_instructions();
    void from_push_operands_to_temporary_instructions();

    Temporary_Instructions instruction_temporary_from_expression_operand(
        Operand& operand);

  public:
    Instructions instructions{};
    Operand_Stack operand_stack{};

  private:
    int* temporary_index;
    int parameters_size{ 0 };
    Temporary_Stack temporary_stack{};
};

constexpr bool is_in_place_unary_operator(type::Operator op)
{
    const auto unary_types = { type::Operator::PRE_DEC,
        type::Operator::POST_DEC,
        type::Operator::PRE_INC,
        type::Operator::POST_INC };
    return util::range_contains(op, unary_types);
}

} // namespace detail

Instructions expression_queue_to_temporary_instructions(
    queue::detail::Queue::Container const& queue,
    int* temporary_index);

Expression_Instructions expression_node_to_temporary_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node const& node,
    util::AST_Node const& details,
    int* temporary_index,
    int* identifier_index);

} // namespace ir

} // namespace credence