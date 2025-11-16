/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <credence/ir/ita.h>    // for ITA
#include <credence/operators.h> // for Operator
#include <credence/queue.h>     // for Queue
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/util.h>      // for AST_Node
#include <credence/value.h>     // for Expression
#include <cstddef>              // for size_t
#include <functional>           // for identity
#include <initializer_list>     // for initializer_list
#include <ranges>               // for __find_fn, find
#include <stack>                // for stack
#include <string>               // for string
#include <utility>              // for pair, move
#include <variant>              // for variant

/****************************************************************************
 *  A set of functions that construct temporary lvalues "_tX" that aid in
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

using Expression_Instructions = std::pair<ITA::Instructions, queue::Queue>;
using Temporary_Instructions = std::pair<std::string, ITA::Instructions>;

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
 * We must also keep note of evaluated expressions, i.e wrapped in parenthesis:
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

struct Temporary
{
    Temporary() = delete;
    explicit Temporary(int* index)
        : temporary_index(index)
    {
    }
    using Operand = internal::value::Expression::Type_Pointer;
    using Operands = std::vector<Operand>;
    using Instructions = ITA::Instructions;
    using Operand_Stack = std::stack<Operand>;
    using Temporary_Stack = std::stack<std::string>;

    internal::value::Size insert_and_create_temporary_from_operand(
        Operand& operand);

    void unary_operand_to_temporary_stack(type::Operator op);

    void assignment_operands_to_temporary_stack();

    void binary_operands_to_temporary_stack(type::Operator op);

    void binary_operands_balanced_temporary_stack(type::Operator op);

    void binary_operands_unbalanced_temporary_stack(type::Operator op);

    void from_call_operands_to_temporary_instructions();
    void from_push_operands_to_temporary_instructions();

    Temporary_Instructions instruction_temporary_from_expression_operand(
        Operand& operand);

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
    return std::ranges::find(unary_types, op) != unary_types.end();
}

} // namespace detail

ITA::Instructions queue_to_temporary_instructions(
    queue::Queue& queue,
    int* index);

Expression_Instructions expression_node_to_temporary_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node const& node,
    util::AST_Node const& details,
    int* temporary_index);

} // namespace ir

} // namespace credence