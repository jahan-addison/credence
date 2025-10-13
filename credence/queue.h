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
#include <credence/types.h>     // for RValue
#include <deque>                // for deque
#include <memory>               // for make_unique, unique_ptr
#include <stack>                // for stack
#include <string>               // for string
#include <string_view>          // for string_view
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

namespace credence {

using RValue_Queue_Item =
    std::variant<type::Operator, type::RValue::Type_Pointer>;

using RValue_Operator_Stack = std::stack<type::Operator>;

using RValue_Types = std::vector<type::RValue::Type_Pointer>;

using RValue_Queue = std::deque<RValue_Queue_Item>;

using RValue_Queue_PTR = std::unique_ptr<RValue_Queue>;

inline RValue_Queue_PTR make_rvalue_queue()
{
    return std::make_unique<RValue_Queue>();
}

void rvalues_to_queue(
    RValue_Types const& rvalues,
    RValue_Queue_PTR& rvalues_queue);

void rvalues_to_queue(
    type::RValue::Type_Pointer const& rvalue,
    RValue_Queue_PTR& rvalues_queue);

std::string rvalue_to_string(
    type::RValue::Type const& rvalue,
    bool separate = true,
    std::string_view separator = ":");

std::string queue_of_rvalues_to_string(
    RValue_Queue_PTR const& rvalues_queue,
    std::string_view separator = ":");

std::string dump_value_type(
    type::RValue::Value,
    std::string_view separator = ":");

} // namespace credence

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