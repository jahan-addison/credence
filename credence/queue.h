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

#include <algorithm>            // for copy, max
#include <credence/operators.h> // for Operator
#include <credence/value.h>     // for Expression, ...
#include <deque>                // for deque
#include <memory>               // for make_unique, unique_ptr
#include <stack>                // for stack
#include <string>               // for string
#include <string_view>          // for string_view
#include <variant>              // for variant
#include <vector>               // for vector

namespace credence {

namespace queue {

/**
 * @brief
 *
 *  Shunting-yard queue and operator stack of expressions
 */

using Expression = internal::value::Expression::Type_Pointer;
using Expressions = std::vector<Expression>;
using Operator_Stack = std::stack<type::Operator>;

using Type = internal::value::Expression::Type;
using Item = std::variant<type::Operator, Expression>;

using Queue = std::deque<Item>;

std::unique_ptr<Queue> make_queue_from_expression_operands(
    Expressions const& items);

std::unique_ptr<Queue> make_queue_from_expression_operands(
    Expression const& item);

std::string queue_of_expressions_to_string(
    Queue const& queue,
    std::string_view separator = ":");

} // namespace queue

} // namespace credence
