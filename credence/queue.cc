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

#include <credence/queue.h>

#include <credence/operators.h> // for Operator, get_precedence, is_le...
#include <credence/util.h>      // for overload
#include <credence/value.h>     // for make_value_type_pointer, Expres...
#include <format>               // for format
#include <mapbox/eternal.hpp>   // for element, map
#include <memory>               // for shared_ptr, allocator, make_unique
#include <ostream>              // for operator<<, basic_ostream
#include <sstream>              // for basic_ostringstream, ostringstream
#include <stack>                // for stack
#include <utility>              // for pair
#include <variant>              // for get, visit, monostate, variant

/**
 * @brief
 *
 *  Shunting-yard queue and operator stack of expressions.
 */

/**************************************************************************
 *
 *           [~]
 *           | | (~)  (~)  (~)    /~~~~~~~~~~~~
 *        /~~~~~~~~~~~~~~~~~~~~~~~  [~_~_] |    * * * /~~~~~~~~~~~|
 *      [|  %___________________           | |~~~~~~~~            |
 *        \[___] ___   ___   ___\  No. 4   | |   A.T. & S.F.      |
 *     /// [___+/-+-\-/-+-\-/-+ \\_________|=|____________________|=
 *   //// @-=-@ \___/ \___/ \___/  @-==-@      @-==-@      @-==-@
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ***************************************************************************/

namespace credence {

namespace queue {

void expression_pointer_to_queue_in_place(
    Expression const& pointer,
    Queue& queue,
    Operator_Stack& operator_stack,
    int* parameter_size);

namespace {
/**
 * @brief Operator precedence check of the queue and operator stack
 */
void associativity_operator_precedence(
    type::Operator op1,
    Queue& queue,
    std::stack<type::Operator>& operator_stack)
{
    using namespace type;
    while (!operator_stack.empty()) {
        auto op2 = operator_stack.top();
        if ((is_left_associative(op1) &&
             get_precedence(op2) <= get_precedence(op2)) ||
            (!is_left_associative(op1) &&
             get_precedence(op1) < get_precedence(op2))) {
            queue.emplace_back(operator_stack.top());
            operator_stack.pop();
        } else {
            break;
        }
    }
}

/**
 * @brief Re-balance the queue if the stack is empty
 */
inline void balance_queue(
    Queue& queue,
    std::stack<type::Operator>& operator_stack)
{
    if (operator_stack.size() == 1) {
        queue.emplace_back(operator_stack.top());
        operator_stack.pop();
    }
}

} // namespace

/**
 * @brief Queue construction via operators and expressions ordered by precedence
 */
void expression_pointer_to_queue_in_place(
    Expression const& pointer,
    Queue& queue,
    Operator_Stack& operator_stack,
    int* parameter_size)
{
    namespace value = internal::value;

    std::visit(
        util::overload{

            [&](std::monostate) {},
            [&](value::Array const&) { queue.emplace_back(pointer); },
            [&](value::Literal const&) { queue.emplace_back(pointer); },
            [&](value::Expression::Pointer const& s) {
                auto value = value::make_value_type_pointer(s->value);
                expression_pointer_to_queue_in_place(
                    value, queue, operator_stack, parameter_size);
            },
            [&](value::Expression::Unary const& s) {
                auto op1 = s.first;
                auto rhs = value::make_value_type_pointer(s.second->value);
                expression_pointer_to_queue_in_place(
                    rhs, queue, operator_stack, parameter_size);
                operator_stack.emplace(op1);
                balance_queue(queue, operator_stack);
                associativity_operator_precedence(op1, queue, operator_stack);
            },
            [&](value::Expression::LValue const&) {
                queue.emplace_back(pointer);
            },
            [&](value::Expression::Relation const& s) {
                auto op1 = s.first;
                if (s.second.size() == 2) {
                    auto lhs = internal::value::make_value_type_pointer(
                        s.second.at(0)->value);
                    auto rhs = internal::value::make_value_type_pointer(
                        s.second.at(1)->value);
                    expression_pointer_to_queue_in_place(
                        lhs, queue, operator_stack, parameter_size);
                    operator_stack.emplace(op1);
                    expression_pointer_to_queue_in_place(
                        rhs, queue, operator_stack, parameter_size);
                } else if (s.second.size() == 4) {
                    // ternary
                    operator_stack.emplace(type::Operator::B_TERNARY);
                    operator_stack.emplace(type::Operator::U_PUSH);
                    auto ternary_lhs = internal::value::make_value_type_pointer(
                        s.second.at(2)->value);
                    auto ternary_rhs = internal::value::make_value_type_pointer(
                        s.second.at(3)->value);
                    expression_pointer_to_queue_in_place(
                        ternary_lhs, queue, operator_stack, parameter_size);
                    expression_pointer_to_queue_in_place(
                        ternary_rhs, queue, operator_stack, parameter_size);
                    auto ternary_truthy =
                        internal::value::make_value_type_pointer(
                            s.second.at(0)->value);
                    operator_stack.emplace(op1);
                    auto ternary_falsey =
                        internal::value::make_value_type_pointer(
                            s.second.at(1)->value);
                    expression_pointer_to_queue_in_place(
                        ternary_truthy, queue, operator_stack, parameter_size);
                    expression_pointer_to_queue_in_place(
                        ternary_falsey, queue, operator_stack, parameter_size);
                }
                balance_queue(queue, operator_stack);
                associativity_operator_precedence(op1, queue, operator_stack);
            },
            [&](value::Expression::Function const& s) {
                auto op1 = type::Operator::U_CALL;
                Operator_Stack operator_stack{};
                auto lhs = value::make_value_type_pointer(s.first);

                queue.emplace_back(lhs);
                std::deque<Expression> parameters{};

                for (auto const& parameter : s.second) {
                    auto param =
                        value::make_value_type_pointer(parameter->value);
                    auto name = value::make_lvalue(
                        std::format("_p{}", ++(*parameter_size)));
                    auto lvalue =
                        internal::value::make_value_type_pointer(name);
                    parameters.emplace_back(lvalue);
                    expression_pointer_to_queue_in_place(
                        lvalue, queue, operator_stack, parameter_size);
                    expression_pointer_to_queue_in_place(
                        param, queue, operator_stack, parameter_size);
                    operator_stack.emplace(type::Operator::B_ASSIGN);
                    balance_queue(queue, operator_stack);
                    associativity_operator_precedence(
                        type::Operator::B_ASSIGN, queue, operator_stack);
                }
                operator_stack.emplace(op1);
                for (auto const& param_lvalue : parameters) {
                    operator_stack.emplace(type::Operator::U_PUSH);
                    expression_pointer_to_queue_in_place(
                        param_lvalue, queue, operator_stack, parameter_size);
                }
                balance_queue(queue, operator_stack);
                associativity_operator_precedence(op1, queue, operator_stack);
            },
            [&](value::Expression::Symbol const& s) {
                auto op1 = type::Operator::B_ASSIGN;
                auto lhs = internal::value::make_value_type_pointer(s.first);
                auto rhs =
                    internal::value::make_value_type_pointer(s.second->value);
                expression_pointer_to_queue_in_place(
                    lhs, queue, operator_stack, parameter_size);
                expression_pointer_to_queue_in_place(
                    rhs, queue, operator_stack, parameter_size);
                operator_stack.emplace(op1);

                balance_queue(queue, operator_stack);
                associativity_operator_precedence(op1, queue, operator_stack);
            } },
        *pointer);
}

/**
 * @brief List of expressions to queue of operators and operands
 */
std::unique_ptr<Queue> make_queue_from_expression_operands(
    Expressions const& items)
{
    int parameter_size = 0;
    Queue queue = Queue{};
    Operator_Stack operator_stack{};
    for (Expression const& item : items) {
        expression_pointer_to_queue_in_place(
            item, queue, operator_stack, &parameter_size);
    }
    return std::make_unique<Queue>(queue);
}

/**
 * @brief type::RValue to queue of operators and operands
 */
std::unique_ptr<Queue> make_queue_from_expression_operands(
    Expression const& item)
{
    int parameter_size = 0;
    Queue queue = Queue{};
    Operator_Stack operator_stack{};
    expression_pointer_to_queue_in_place(
        item, queue, operator_stack, &parameter_size);

    return std::make_unique<Queue>(queue);
}

/**
 * @brief Queue to string of operators and operands in reverse-polish notation
 */
std::string queue_of_expressions_to_string(
    Queue const& queue,
    std::string_view separator)
{
    auto oss = std::ostringstream();
    for (auto& item : queue) {
        // clang-format off
        std::visit(
            util::overload{
                [&](type::Operator op) {
                    oss << type::operator_to_string(op) << " ";
                },
                [&](Expression const& s) {
                    oss << internal::value::expression_type_to_string(
                        *s, true, separator);
                }
            },
            item);
        // clang-format on
    }
    return oss.str();
}

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

} // namespace queue

} // namespace credence