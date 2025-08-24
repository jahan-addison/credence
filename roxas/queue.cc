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
#include <roxas/queue.h>
#include <roxas/operators.h>  // for Operator, get_precedence, is_left_assoc...
#include <roxas/types.h>      // for RValue
#include <roxas/util.h>       // for overload
#include <algorithm>          // for copy, max
#include <memory>             // for make_shared, shared_ptr, __shared_ptr_a...
#include <stack>              // for stack
#include <variant>            // for variant, visit, monostate
// clang-format on

/**************************************************************************
 *
 *                      (+++++++++++)
 *                 (++++)
 *              (+++)
 *            (+++)
 *           (++)
 *           [~]
 *           | | (~)  (~)  (~)    /~~~~~~~~~~~~
 *        /~~~~~~~~~~~~~~~~~~~~~~~  [~_~_] |    * * * /~~~~~~~~~~~|
 *      [|  %___________________           | |~~~~~~~~            |
 *        \[___] ___   ___   ___\  No. 4   | |   A.T. & S.F.      |
 *     /// [___+/-+-\-/-+-\-/-+ \\_________|=|____________________|=
 *   //// @-=-@ \___/ \___/ \___/  @-==-@      @-==-@      @-==-@
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *------------------------------------------------
 ***************************************************************************/

namespace roxas {

namespace detail {

/**
 * @brief Operator precedence check of the queue and operator stack
 */
inline void _associativity_operator_precedence(
    type::Operator op1,
    RValue_Queue* rvalues_queue,
    std::stack<type::Operator>& operator_stack)
{
    while (!operator_stack.empty()) {
        auto op2 = operator_stack.top();
        if ((is_left_associative(op1) &&
             get_precedence(op2) <= get_precedence(op2)) ||
            (!is_left_associative(op1) &&
             get_precedence(op1) < get_precedence(op2))) {
            rvalues_queue->push_back(operator_stack.top());
            operator_stack.pop();
        } else {
            break;
        }
    }
}

/**
 * @brief Re-balance the queue if the stack is empty
 */
inline void _balance_queue(RValue_Queue* rvalues_queue,
                           std::stack<type::Operator>& operator_stack)
{
    if (operator_stack.size() == 1) {
        rvalues_queue->push_back(operator_stack.top());
        operator_stack.pop();
    }
}

/**
 * @brief Mutual recursive queue construction via operators and rvalues
 */
RValue_Queue* _rvalue_pointer_to_queue(
    type::RValue::Type_Pointer rvalue_pointer,
    RValue_Queue* rvalues_queue,
    std::stack<type::Operator>& operator_stack)
{
    using namespace type;
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](type::RValue::RValue_Pointer& s) {
                auto value = std::make_shared<type::RValue::Type>(s->value);
                _rvalue_pointer_to_queue(value, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Value&) {
                rvalues_queue->push_back(rvalue_pointer);
            },
            [&](type::RValue::LValue&) {
                rvalues_queue->push_back(rvalue_pointer);
            },
            [&](type::RValue::Unary& s) {
                auto op1 = s.first;
                auto rhs =
                    std::make_shared<type::RValue::Type>(s.second->value);
                _rvalue_pointer_to_queue(rhs, rvalues_queue, operator_stack);
                operator_stack.push(op1);
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Relation& s) {
                auto op1 = s.first;
                if (s.second.size() == 2) {
                    auto lhs = std::make_shared<type::RValue::Type>(
                        s.second.at(0)->value);
                    auto rhs = std::make_shared<type::RValue::Type>(
                        s.second.at(1)->value);
                    _rvalue_pointer_to_queue(
                        lhs, rvalues_queue, operator_stack);
                    operator_stack.push(op1);
                    _rvalue_pointer_to_queue(
                        rhs, rvalues_queue, operator_stack);
                } else if (s.second.size() == 4) {
                    // ternary
                    operator_stack.push(op1);
                    for (auto& operand : s.second) {
                        auto value = std::make_shared<type::RValue::Type>(
                            operand->value);
                        _rvalue_pointer_to_queue(
                            value, rvalues_queue, operator_stack);
                    }
                    operator_stack.push(Operator::B_TERNARY);
                }
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Function& s) {
                auto op1 = Operator::U_CALL;
                auto lhs = std::make_shared<type::RValue::Type>(s.first);
                operator_stack.push(op1);
                _rvalue_pointer_to_queue(lhs, rvalues_queue, operator_stack);
                for (auto& parameter : s.second) {
                    auto param =
                        std::make_shared<type::RValue::Type>(parameter->value);
                    operator_stack.push(Operator::U_PUSH);
                    _rvalue_pointer_to_queue(
                        param, rvalues_queue, operator_stack);
                }
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Symbol& s) {
                auto op1 = Operator::B_ASSIGN;
                auto lhs = std::make_shared<type::RValue::Type>(s.first);
                auto rhs =
                    std::make_shared<type::RValue::Type>(s.second->value);
                _rvalue_pointer_to_queue(lhs, rvalues_queue, operator_stack);
                _rvalue_pointer_to_queue(rhs, rvalues_queue, operator_stack);
                operator_stack.push(op1);
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            } },
        *rvalue_pointer);
    return rvalues_queue;
}

} // namespace detail

/**
 * @brief List of rvalues to queue of operators and operands
 */
RValue_Queue* rvalues_to_queue(std::vector<type::RValue::Type_Pointer>& rvalues,
                               RValue_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    for (type::RValue::Type_Pointer& rvalue : rvalues) {
        roxas::detail::_rvalue_pointer_to_queue(
            rvalue, rvalues_queue, operator_stack);
    }

    return rvalues_queue;
}

/**
 * @brief RValue to queue of operators and operands
 */
RValue_Queue* rvalues_to_queue(type::RValue::Type_Pointer& rvalue,
                               RValue_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    roxas::detail::_rvalue_pointer_to_queue(
        rvalue, rvalues_queue, operator_stack);
    return rvalues_queue;
}

} // namespace roxas