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
#include <string>            // for basic_string, string
#include <utility>           // for pair
namespace credence {
namespace type {
enum class Operator;
}
} // lines 31-31

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

// clang-format off
std::pair<std::string, ITA::Instructions>
instruction_temporary_from_rvalue_operand(
    type::RValue::Type_Pointer& operand,
    int* temporary_size);

} // namespace detail

ITA::Instructions
rvalue_queue_to_temp_instructions(
    RValue_Queue* queue,
    int* temporary);
// clang-format on

RValue_Instructions rvalue_node_to_list_of_temp_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node& node,
    util::AST_Node& details,
    int* temporary);

} // namespace ir

} // namespace credence