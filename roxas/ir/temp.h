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

#include <format>          // for format
#include <roxas/ir/qaud.h> // for Instructions, make_quadruple, Instruction
#include <roxas/queue.h>   // for RValue_Queue
#include <roxas/types.h>   // for RValue
#include <string>          // for string
#include <utility>         // for pair

namespace roxas {

namespace ir {

namespace detail {

/**
 * @brief Construct a quadruple that holds a linear instruction IR
 */
inline Quadruple make_temporary(int* temporary_size, std::string const& temp)
{
    auto lhs = std::format("_t{}", ++(*temporary_size), temp);
    return make_quadruple(Instruction::VARIABLE, lhs, temp);
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
    type::Operator op,
    int* temporary);

std::pair<std::string, Instructions> instruction_temporary_from_rvalue_operand(
    type::RValue::Type_Pointer& operand,
    int* temporary_size);

} // namespace

Instructions rvalue_queue_to_linear_ir_instructions(RValue_Queue* queue,
                                                    int* temporary);

} // namespace ir

} // namespace roxas