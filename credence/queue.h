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

#include <credence/operators.h> // for Operator
#include <credence/values.h>    // for Expression
#include <deque>                // for deque
#include <memory>               // for unique_ptr
#include <stack>                // for stack
#include <string>               // for string
#include <string_view>          // for basic_string_view, string_view
#include <variant>              // for variant
#include <vector>               // for vector

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

namespace credence::queue {

using Expression = value::Expression::Type_Pointer;
using Expressions = std::vector<Expression>;
using Operator_Stack = std::stack<type::Operator>;

using Type = value::Expression::Type;

namespace detail {

/**
 * @brief
 *
 *  Shunting-yard queue via operator stack of expressions
 */
class Queue
{
  public:
    Queue() = delete;
    explicit Queue(int* parameter_index, int* identifier_index)
        : parameter_size_(parameter_index)
        , parameter_ident_(identifier_index)
    {
    }

  public:
    using Item = std::variant<type::Operator, Expression>;
    using Container = std::deque<Item>;

    void shunt_expression_pointer_into_queue(Expression const& pointer,
        Operator_Stack* operator_stack = nullptr);
    void shunt_argument_expressions_into_queue(
        value::Expression::Function const& s);

    inline std::unique_ptr<Container> get()
    {
        return std::make_unique<Container>(queue_);
    }

  private:
    void balance_operator_precedence(type::Operator op1);
    void balance_queue();
    void balance_operator_precedence(Operator_Stack* operator_stack,
        type::Operator op1);
    void balance_queue(Operator_Stack* operator_stack);

  private:
    Container queue_;

  private:
    Operator_Stack operator_stack_{};
    int* parameter_size_;
    int* parameter_ident_;
};

} // namespace detail;

std::unique_ptr<detail::Queue::Container> queue_from_expression_operands(
    Expressions const& items,
    int* parameter,
    int* identifier);

std::unique_ptr<detail::Queue::Container> queue_from_expression_operands(
    Expression const& item,
    int* parameter,
    int* identifier);

std::string queue_of_expressions_to_string(
    detail::Queue::Container const& queue,
    std::string_view separator = ":");

} // namespace queue
