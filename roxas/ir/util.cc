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

// clang-format off
#include <vector>             // for vector
#include <roxas/ir/util.h>
#include <roxas/ir/qaud.h>    // for Instruction, Instructions, instruction_...
#include <roxas/operators.h>  // for Operator, operator_to_string
#include <roxas/queue.h>      // for RValue_Queue
#include <roxas/types.h>      // for RValue, rvalue_type_pointer_from_rvalue
#include <roxas/util.h>       // for rvalue_to_string, overload
#include <deque>              // for deque
#include <format>             // for format
#include <map>                // for map
#include <memory>             // for shared_ptr
#include <stack>              // for stack
#include <stdexcept>          // for runtime_error
#include <string>             // for basic_string, string, to_string
#include <tuple>              // for tuple, get, make_tuple
#include <utility>            // for pair, make_pair
#include <variant>            // for visit, monostate
// clang-format on

namespace roxas {

namespace ir {

namespace detail {

using namespace type;

/**
 * @brief Runtime stack size is incorrect for an operator
 */
inline void instruction_error(type::Operator op)
{
    throw std::runtime_error(
        std::format("runtime error: invalid stack size for operator \"{}\"",
                    type::operator_to_string(op)));
}

/**
 * @brief Construct a quadruple that holds a scope temporary lvalue
 */
inline Quadruple make_temporary(int* temporary_size, std::string const& temp)
{
    auto lhs = std::format("_t{}", ++(*temporary_size), temp);
    return make_quadruple(Instruction::VARIABLE, lhs, temp);
}

/**
 * @brief Construct a temporary lvalue from a mutual recursive rvalue
 */
std::pair<std::string, Instructions> instruction_temporary_from_rvalue_operand(
    RValue::Type_Pointer& operand,
    int* temporary_size)
{
    Instructions instructions{};
    std::string temp_name{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](RValue::RValue_Pointer& s) {
                auto unwrap_type =
                    type::rvalue_type_pointer_from_rvalue(s->value);
                auto rvalue_pointer = instruction_temporary_from_rvalue_operand(
                    unwrap_type, temporary_size);

                instructions.insert(instructions.end(),
                                    rvalue_pointer.second.begin(),
                                    rvalue_pointer.second.end());
                temp_name = rvalue_pointer.first;
            },
            [&](RValue::Value&) {
                temp_name = util::rvalue_to_string(*operand, false);
            },
            [&](RValue::LValue&) {
                // auto lvalue = make_temporary(
                //     temporary_size, util::rvalue_to_string(*operand, false));
                // instructions.push_back(lvalue);
                // temp_name = std::get<1>(lvalue);
                temp_name = util::rvalue_to_string(*operand, false);
            },
            [&](RValue::Unary& s) {
                auto op = s.first;
                auto unwrap_rhs_type =
                    type::rvalue_type_pointer_from_rvalue(s.second->value);
                auto rhs = instruction_temporary_from_rvalue_operand(
                    unwrap_rhs_type, temporary_size);
                instructions.insert(
                    instructions.end(), rhs.second.begin(), rhs.second.end());
                auto temp = make_temporary(temporary_size, rhs.first);

                auto unary = make_temporary(temporary_size,
                                            std::format("{} {}",
                                                        operator_to_string(op),
                                                        std::get<1>(temp)));
                instructions.push_back(unary);
                temp_name = std::get<1>(unary);
            },
            [&](RValue::Relation& s) {
                auto op = s.first;
                if (s.second.size() == 2) {
                    auto unwrap_lhs_type =
                        type::rvalue_type_pointer_from_rvalue(
                            s.second.at(0)->value);
                    auto unwrap_rhs_type =
                        type::rvalue_type_pointer_from_rvalue(
                            s.second.at(1)->value);
                    auto lhs = instruction_temporary_from_rvalue_operand(
                        unwrap_lhs_type, temporary_size);
                    auto rhs = instruction_temporary_from_rvalue_operand(
                        unwrap_rhs_type, temporary_size);
                    auto relation =
                        make_temporary(temporary_size,
                                       std::format("{} {} {}",
                                                   lhs.first,
                                                   operator_to_string(op),
                                                   rhs.first));

                    instructions.push_back(relation);
                    temp_name = std::get<1>(relation);

                } else if (s.second.size() == 4) {
                    // ternary
                }
            },
            [&](RValue::Function& s) {
                temp_name = util::rvalue_to_string(s.first, false);
            },
            [&](RValue::Symbol& s) {
                temp_name = util::rvalue_to_string(s.first, false);
            } },
        *operand);

    return std::make_pair(temp_name, instructions);
}

/**
 * @brief Unary operators  and temporary stack to instructions
 */
void unary_operand_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    Operator op,
    int* temporary)
{
    if (operand_stack.size() < 1)
        instruction_error(op);
    if (temporary_stack.size() > 1) {
        auto operand1 = temporary_stack.top();
        temporary_stack.pop();
        auto unary = make_temporary(
            temporary, std::format("{} {}", operator_to_string(op), operand1));
        temporary_stack.push(std::format(
            "{} {}", operator_to_string(op), std::get<1>(unary), ""));

    } else {
        auto operand1 = operand_stack.top();
        operand_stack.pop();
        if (temporary_stack.size() == 1) {
            auto operand1 = temporary_stack.top();
            temporary_stack.pop();
            auto last_expression = make_temporary(temporary, operand1);
            instructions.push_back(last_expression);
            temporary_stack.push(std::get<1>(last_expression));
        }
        auto rhs =
            instruction_temporary_from_rvalue_operand(operand1, temporary);
        instructions.insert(
            instructions.end(), rhs.second.begin(), rhs.second.end());
        auto unary = make_temporary(
            temporary, std::format("{} {}", operator_to_string(op), rhs.first));
        instructions.push_back(unary);
        temporary_stack.push(std::get<1>(unary));
    }
}

/**
 * @brief binary operands and temporary stack to instructions
 */
void binary_operands_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    Operator op,
    int* temporary)
{

    if (temporary_stack.size() >= 2) {
        auto rhs = temporary_stack.top();
        temporary_stack.pop();
        auto lhs = temporary_stack.top();
        temporary_stack.pop();
        auto last_instruction = make_temporary(
            temporary,
            std::format("{} {} {}", lhs, operator_to_string(op), rhs));
        instructions.push_back(last_instruction);
    } else if (temporary_stack.size() == 1) {
        auto operand1 = operand_stack.top();
        operand_stack.pop();
        auto lhs = temporary_stack.top();
        temporary_stack.pop();
        auto rhs =
            instruction_temporary_from_rvalue_operand(operand1, temporary);
        instructions.insert(
            instructions.end(), rhs.second.begin(), rhs.second.end());
        auto temp_lhs = make_temporary(temporary, lhs);
        instructions.push_back(temp_lhs);
        temporary_stack.push(std::format("{} {} {}",
                                         rhs.first,
                                         operator_to_string(op),
                                         std::get<1>(temp_lhs)));

    } else {
        if (operand_stack.size() < 2)
            instruction_error(op);
        auto operand1 = operand_stack.top();
        operand_stack.pop();
        auto operand2 = operand_stack.top();
        operand_stack.pop();
        auto rhs =
            instruction_temporary_from_rvalue_operand(operand1, temporary);
        auto lhs =
            instruction_temporary_from_rvalue_operand(operand2, temporary);
        instructions.insert(
            instructions.end(), rhs.second.begin(), rhs.second.end());
        instructions.insert(
            instructions.end(), lhs.second.begin(), lhs.second.end());
        temporary_stack.push(std::format(
            "{} {} {}", lhs.first, operator_to_string(op), rhs.first));
    }
}

} // namespace detail

/**
 * @brief Construct a set of qaudruple instructions based on an rvalue queue
 *
 * A queue corresponds to a line in the source code
 */
Instructions rvalue_queue_to_instructions(RValue_Queue* queue, int* temporary)
{
    using namespace roxas::ir::detail;
    Instructions instructions{};
    std::stack<std::string> temporary_stack{};
    std::stack<type::RValue::Type_Pointer> operand_stack{};
    int param_on_stack = 0;
    for (auto& item : *queue) {
        std::visit(
            util::overload{
                [&](type::Operator op) {
                    switch (op) {
                        // relational operators
                        case Operator::R_EQUAL:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);

                            break;
                        case Operator::R_NEQUAL:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::R_LT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::R_GT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::R_LE:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::R_GE:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::R_OR:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);

                            break;
                        case Operator::R_AND:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;

                        // math binary operators
                        case Operator::B_SUBTRACT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::B_ADD:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::B_MOD:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::B_MUL:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::B_DIV:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;

                        // unary increment/decrement
                        case Operator::PRE_INC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::POST_INC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        case Operator::PRE_DEC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::POST_DEC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // bit ops
                        case Operator::RSHIFT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::OR:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::AND:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::LSHIFT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::XOR:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;

                        case Operator::U_NOT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_ONES_COMPLEMENT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // pointer operators
                        case Operator::U_SUBSCRIPT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_INDIRECTION:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_ADDR_OF:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_MINUS:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // lvalue and address operators
                        case Operator::U_CALL: {
                            if (temporary_stack.size() >= 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                auto temp_rhs = make_temporary(temporary, rhs);
                                instructions.push_back(temp_rhs);
                                temporary_stack.push(
                                    std::format("{} {}",
                                                operator_to_string(op),
                                                std::get<1>(temp_rhs)));
                                instructions.push_back(
                                    make_quadruple(Instruction::CALL, rhs, ""));
                                temporary_stack.push(rhs);
                            } else {
                                auto operand = operand_stack.top();
                                operand_stack.pop();
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand, temporary);
                                instructions.insert(instructions.end(),
                                                    rhs.second.begin(),
                                                    rhs.second.end());

                                instructions.push_back(make_quadruple(
                                    Instruction::CALL, rhs.first, ""));

                                temporary_stack.push(
                                    instruction_to_string(Instruction::RETURN));
                            }
                            instructions.push_back(make_quadruple(
                                Instruction::POP,
                                std::to_string(param_on_stack *
                                               type::Type_["word"].second),
                                "",
                                ""));

                            param_on_stack = 0;
                        } break;
                        case Operator::U_PUSH: {
                            if (temporary_stack.size() >= 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                instructions.push_back(
                                    make_quadruple(Instruction::PUSH, rhs, ""));
                            } else {
                                auto operand = operand_stack.top();
                                operand_stack.pop();
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand, temporary);
                                instructions.insert(instructions.end(),
                                                    rhs.second.begin(),
                                                    rhs.second.end());
                                instructions.push_back(make_quadruple(
                                    Instruction::PUSH, rhs.first, ""));
                            }
                            param_on_stack++;
                        } break;
                        case Operator::B_ASSIGN: {
                            if (temporary_stack.size() >= 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                auto lvalue = operand_stack.top();
                                operand_stack.pop();
                                auto lhs =
                                    instruction_temporary_from_rvalue_operand(
                                        lvalue, temporary);
                                instructions.insert(instructions.end(),
                                                    lhs.second.begin(),
                                                    lhs.second.end());
                                instructions.push_back(make_quadruple(
                                    Instruction::VARIABLE, lhs.first, rhs));
                            } else {
                                auto operand1 = operand_stack.top();
                                operand_stack.pop();
                                auto operand2 = operand_stack.top();
                                operand_stack.pop();
                                auto lhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand2, temporary);
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand1, temporary);
                                instructions.insert(instructions.end(),
                                                    lhs.second.begin(),
                                                    lhs.second.end());
                                instructions.insert(instructions.end(),
                                                    rhs.second.begin(),
                                                    rhs.second.end());
                                instructions.push_back(
                                    make_quadruple(Instruction::VARIABLE,
                                                   lhs.first,
                                                   rhs.first));
                            }
                        } break;
                        case Operator::B_TERNARY:
                            break;
                    }
                },
                [&](type::RValue::Type_Pointer& s) { operand_stack.push(s); }

            },
            item);
    }
    return instructions;
}

} // namespace ir

} // namespace roxas