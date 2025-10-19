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

#include <credence/ir/ita.h> // for ITA
#include <credence/queue.h>  // for RValue_Queue
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue
#include <credence/util.h>   // for AST_Node
#include <cstddef>           // for size_t
#include <stack>             // for stack
#include <string>            // for string
#include <utility>           // for pair
#include <variant>           // for variant
namespace credence {
namespace type {
enum class Operator;
}
}

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

using RValue_Instructions = std::pair<ITA::Instructions, RValue_Queue>;

namespace detail {

using Operand_Stack = std::stack<type::RValue::Type_Pointer>;
using Temporary_Stack = std::stack<std::string>;

std::pair<std::string, std::size_t> insert_create_temp_from_operand(
    type::RValue::Type_Pointer operand,
    ITA::Instructions& instructions,
    int* temporary);
/**
 * @brief
 * Binary operators and temporary stack to instructions
 *
 * Consider the expression `(x > 1 || x < 1)`
 *
 * In order to express this in a set of 3 or 4 expressions, we create the
 * temporaries:
 *
 * clang-format off
 *
 *  _t1 = x > 1
 *  _t2 = x < 1
 *  _t3 = _t1 || _t2
 *
 * clang-format on
 *
 * The binary temporary is "_t3", which we return.
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

void binary_operands_balanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    ITA::Instructions& instructions,
    type::Operator op,
    int* temporary);

void binary_operands_unbalanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    ITA::Instructions& instructions,
    int* temporary);

std::pair<std::string, ITA::Instructions>
instruction_temporary_from_rvalue_operand(
    type::RValue::Type_Pointer& operand,
    int* temporary_size);

constexpr inline bool is_in_place_unary_operator(type::Operator op)
{
    const auto unary_types = { type::Operator::PRE_DEC,
                               type::Operator::POST_DEC,
                               type::Operator::PRE_INC,
                               type::Operator::POST_INC };
    return std::ranges::find(unary_types, op) != unary_types.end();
}

} // namespace detail

ITA::Instructions rvalue_queue_to_temp_instructions(
    RValue_Queue* queue,
    int* temporary);

RValue_Instructions rvalue_node_to_list_of_temp_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node const& node,
    util::AST_Node const& details,
    int* temporary);

} // namespace ir

} // namespace credence