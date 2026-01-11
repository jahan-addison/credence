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

/****************************************************************************
 *
 * Shunting-yard algorithm implementation
 *
 * Implements the classic shunting-yard algorithm for expression evaluation.
 * Converts infix notation (what programmers write) to postfix notation
 * (what's easier to evaluate), respecting operator precedence.
 *
 * Example - step-by-step conversion:
 *
 *   Input: 5 + 3 * 2
 *
 *   Step 1: Push 5 to output
 *   Step 2: Push + to operator stack
 *   Step 3: Push 3 to output
 *   Step 4: * has higher precedence, push to operator stack
 *   Step 5: Push 2 to output
 *   Step 6: Pop * and + to output
 *
 *   Result: 5 3 2 * +
 *
 * This postfix form is then easy to evaluate: 3*2=6, then 5+6=11
 *
 *****************************************************************************/

#include <credence/queue.h>

#include <credence/operators.h> // for Operator, get_precedence, is_le...
#include <credence/util.h>      // for overload
#include <credence/values.h>    // for make_value_type_pointer, Expres...
#include <fmt/format.h>         // for format
#include <memory>               // for shared_ptr, make_unique, allocator
#include <ostream>              // for operator<<, basic_ostream
#include <sstream>              // for basic_ostringstream, ostringstream
#include <variant>              // for variant, visit, monostate

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

namespace detail {

/**
 * @brief Operator precedence check of the queue and operator stack
 */
void Queue::balance_operator_precedence(type::Operator op1)
{
    while (!operator_stack_.empty()) {
        auto op2 = operator_stack_.top();
        if ((is_left_associative(op1) &&
                get_precedence(op2) <= get_precedence(op2)) ||
            (!is_left_associative(op1) &&
                get_precedence(op1) < get_precedence(op2))) {
            queue_.emplace_back(operator_stack_.top());
            operator_stack_.pop();
        } else {
            break;
        }
    }
}

/**
 * @brief Re-balance the queue if the stack is empty
 */
void Queue::balance_queue()
{
    if (operator_stack_.size() == 1) {
        queue_.emplace_back(operator_stack_.top());
        operator_stack_.pop();
    }
}

/**
 * @brief Operator precedence check of the queue and operator stack
 */
void Queue::balance_operator_precedence(Operator_Stack* operator_stack,
    type::Operator op1)
{
    while (!operator_stack->empty()) {
        auto op2 = operator_stack->top();
        if ((is_left_associative(op1) &&
                get_precedence(op2) <= get_precedence(op2)) ||
            (!is_left_associative(op1) &&
                get_precedence(op1) < get_precedence(op2))) {
            queue_.emplace_back(operator_stack->top());
            operator_stack->pop();
        } else {
            break;
        }
    }
}

/**
 * @brief Re-balance the queue if the stack is empty
 */
void Queue::balance_queue(Operator_Stack* operator_stack)
{
    if (operator_stack->size() == 1) {
        queue_.emplace_back(operator_stack->top());
        operator_stack->pop();
    }
}

/**
 * @brief Shunt arguments that are PUSH'd into function calls
 */
void Queue::shunt_argument_expressions_into_queue(
    value::Expression::Function const& s)
{
    Operator_Stack operator_stack{};
    auto op1 = type::Operator::U_CALL;
    auto lhs = value::make_value_type_pointer(s.first);

    queue_.emplace_back(lhs);

    std::deque<Expression> parameters{};

    for (auto const& parameter : s.second) {
        auto param = value::make_value_type_pointer(parameter->value);
        auto name = value::make_lvalue(fmt::format(
            "_p{}_{}", ++(*parameter_size_), ++(*parameter_ident_)));
        auto lvalue = value::make_value_type_pointer(name);
        parameters.emplace_back(lvalue);
        shunt_expression_pointer_into_queue(lvalue, &operator_stack);
        shunt_expression_pointer_into_queue(param, &operator_stack);
        operator_stack.emplace(type::Operator::B_ASSIGN);
        balance_queue(&operator_stack);
        balance_operator_precedence(&operator_stack, type::Operator::B_ASSIGN);
    }

    operator_stack.emplace(op1);

    for (auto const& param_lvalue : parameters) {
        operator_stack.emplace(type::Operator::U_PUSH);
        shunt_expression_pointer_into_queue(param_lvalue);
    }
    balance_queue(&operator_stack);
    balance_operator_precedence(&operator_stack, op1);
}

/**
 * @brief Shunt the operator stack into an ordered expression queue
 */
void Queue::shunt_expression_pointer_into_queue(Expression const& pointer,
    Operator_Stack* operator_stack)
{
    Operator_Stack& operator_stack_ref =
        operator_stack != nullptr ? *operator_stack : operator_stack_;
    std::visit(
        util::overload{

            [&](std::monostate) {},
            [&](value::Array const&) { queue_.emplace_back(pointer); },
            [&](value::Literal const&) { queue_.emplace_back(pointer); },
            [&](value::Expression::Pointer const& s) {
                auto value = value::make_value_type_pointer(s->value);
                shunt_expression_pointer_into_queue(value);
            },
            [&](value::Expression::Unary const& s) {
                auto op1 = s.first;
                auto rhs = value::make_value_type_pointer(s.second->value);
                shunt_expression_pointer_into_queue(rhs);
                operator_stack_ref.emplace(op1);
                balance_queue();
                balance_operator_precedence(op1);
            },
            [&](value::Expression::LValue const&) {
                queue_.emplace_back(pointer);
            },
            [&](value::Expression::Relation const& s) {
                auto op1 = s.first;
                if (s.second.size() == 2) {
                    auto lhs =
                        value::make_value_type_pointer(s.second.at(0)->value);
                    auto rhs =
                        value::make_value_type_pointer(s.second.at(1)->value);
                    shunt_expression_pointer_into_queue(lhs);
                    operator_stack_ref.emplace(op1);
                    shunt_expression_pointer_into_queue(rhs);
                } else if (s.second.size() == 4) {
                    // ternary
                    operator_stack_ref.emplace(type::Operator::B_TERNARY);
                    operator_stack_ref.emplace(type::Operator::U_PUSH);
                    auto ternary_lhs =
                        value::make_value_type_pointer(s.second.at(2)->value);
                    auto ternary_rhs =
                        value::make_value_type_pointer(s.second.at(3)->value);
                    shunt_expression_pointer_into_queue(ternary_lhs);
                    shunt_expression_pointer_into_queue(ternary_rhs);
                    auto ternary_truthy =
                        value::make_value_type_pointer(s.second.at(0)->value);
                    operator_stack_ref.emplace(op1);
                    auto ternary_falsey =
                        value::make_value_type_pointer(s.second.at(1)->value);
                    shunt_expression_pointer_into_queue(ternary_truthy);
                    shunt_expression_pointer_into_queue(ternary_falsey);
                }
                balance_queue();
                balance_operator_precedence(op1);
            },
            [&](value::Expression::Function const& s) {
                shunt_argument_expressions_into_queue(s);
            },
            [&](value::Expression::Symbol const& s) {
                auto op1 = type::Operator::B_ASSIGN;
                auto lhs = value::make_value_type_pointer(s.first);
                auto rhs = value::make_value_type_pointer(s.second->value);
                shunt_expression_pointer_into_queue(lhs);
                shunt_expression_pointer_into_queue(rhs);
                operator_stack_ref.emplace(op1);
                balance_queue();
                balance_operator_precedence(op1);
            } },
        *pointer);
}

} // namespace detail

/**
 * @brief List of expressions to queue of operators and operands
 */
std::unique_ptr<detail::Queue::Container> queue_from_expression_operands(
    Expressions const& items,
    int* parameter,
    int* identifier)
{
    detail::Queue queue{ parameter, identifier };
    for (Expression const& item : items)
        queue.shunt_expression_pointer_into_queue(item);

    return queue.get();
}

/**
 * @brief Single expression to queue of operators and operands
 */
std::unique_ptr<detail::Queue::Container> queue_from_expression_operands(
    Expression const& item,
    int* parameter,
    int* identifier)
{
    detail::Queue queue{ parameter, identifier };
    queue.shunt_expression_pointer_into_queue(item);

    return queue.get();
}

/**
 * @brief Queue to string of operators and operands in reverse-polish
 * notation
 */
std::string queue_of_expressions_to_string(
    detail::Queue::Container const& queue,
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
                    oss << value::expression_type_to_string(
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