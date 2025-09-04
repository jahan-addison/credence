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

#include <credence/ir/qaud.h> // for Instructions, make_quadruple, Instruction
#include <credence/queue.h>   // for RValue_Queue
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/types.h>   // for RValue
#include <cstddef>            // for size_t
#include <stack>              // for stack
#include <string>             // for allocator, string, operator+, to_string
#include <utility>            // for pair
#include <variant>            // for variant
namespace credence {
namespace type {
enum class Operator;
}
} // lines 29-29

namespace credence {

namespace ir {

namespace detail {

constexpr Quadruple make_temporary(int* temporary_size, std::string const& temp)
{
    return make_quadruple(Instruction::VARIABLE,
                          std::string{ "_t" } +
                              std::to_string(++(*temporary_size)),
                          temp);
}

constexpr Quadruple make_temporary(int* temporary_size)
{
    return make_quadruple(Instruction::LABEL,
                          std::string{ "_L" } +
                              std::to_string(++(*temporary_size)),
                          "");
}

std::pair<std::string, std::size_t> insert_create_temp_from_operand(
    type::RValue::Type_Pointer operand,
    Instructions& instructions,
    int* temporary);

void binary_operands_balanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    type::Operator op,
    int* temporary);

void binary_operands_unbalanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    int* temporary);

std::pair<std::string, Instructions> instruction_temporary_from_rvalue_operand(
    type::RValue::Type_Pointer& operand,
    int* temporary_size);

} // namespace detail

using RValue_Instructions = std::pair<Instructions, RValue_Queue>;

Instructions rvalue_queue_to_temp_ir_instructions(RValue_Queue* queue,
                                                  int* temporary);

RValue_Instructions rvalue_node_to_list_of_ir_instructions(
    Symbol_Table<> const& symbols,
    Node& node,
    Node& details,
    int* temporary);

} // namespace ir

} // namespace credence