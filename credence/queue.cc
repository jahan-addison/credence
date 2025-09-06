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

#include <algorithm>            // for copy, max
#include <credence/operators.h> // for Operator, get_precedence, is_left_as...
#include <credence/types.h>     // for RValue, Type_, Byte
#include <credence/util.h>      // for overload
#include <map>                  // for map
#include <memory>               // for make_shared, shared_ptr, __shared_pt...
#include <sstream>              // for basic_ostream, basic_ostringstream
#include <stack>                // for stack
#include <utility>              // for pair
#include <variant>              // for variant, get, visit, monostate

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

namespace credence {

/**
 * @brief RValue::Value tuple as a string
 */
std::string dump_value_type(type::RValue::Value type,
                            std::string_view separator)
{
    using namespace type;
    std::ostringstream os;
    os << "(";
    std::visit(util::overload{
                   [&](int i) {
                       os << i << separator << LITERAL_TYPE.at("int").first
                          << separator << LITERAL_TYPE.at("int").second;
                   },
                   [&](long i) {
                       os << i << separator << LITERAL_TYPE.at("long").first
                          << separator << LITERAL_TYPE.at("long").second;
                   },
                   [&](float i) {
                       os << i << separator << LITERAL_TYPE.at("float").first
                          << separator << LITERAL_TYPE.at("float").second;
                   },
                   [&](double i) {
                       os << i << separator << LITERAL_TYPE.at("double").first
                          << separator << LITERAL_TYPE.at("double").second;
                   },
                   [&](bool i) {
                       os << std::boolalpha << i << separator
                          << LITERAL_TYPE.at("bool").first << separator
                          << LITERAL_TYPE.at("bool").second;
                   },
                   [&]([[maybe_unused]] std::monostate i) {
                       os << "null" << separator
                          << LITERAL_TYPE.at("null").first << separator
                          << LITERAL_TYPE.at("null").second;
                   },
                   [&](credence::type::Byte i) {
                       os << i << separator << LITERAL_TYPE.at("byte").first
                          << separator << type.second.second;
                   },
                   [&](char i) {
                       os << i << LITERAL_TYPE.at("char").first << separator
                          << LITERAL_TYPE.at("char").second;
                   },
                   [&]([[maybe_unused]] std::string const& s) {
                       if (s == "__WORD_") {
                           // pointer
                           os << "__WORD_" << separator
                              << LITERAL_TYPE.at("word").first << separator
                              << LITERAL_TYPE.at("word").second;
                       } else {
                           os << std::get<std::string>(type.first) << separator
                              << "string" << separator
                              << std::get<std::string>(type.first).size();
                       }
                   },
               },
               type.first);
    os << ")";
    return os.str();
}

/**
 * @brief Rvalue type to string of unwrapped data
 */
std::string rvalue_to_string(type::RValue::Type const& rvalue, bool separate)
{
    using namespace credence::type;
    auto oss = std::ostringstream();
    auto space = separate ? " " : "";
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](RValue::RValue_Pointer const&) {},
            [&](RValue::Value const& s) { oss << dump_value_type(s) << space; },
            [&](RValue::Value_Pointer const& s) {
                for (auto const& value : s) {
                    oss << dump_value_type(value) << space;
                }
            },
            [&](RValue::LValue const& s) { oss << s.first << space; },
            [&](RValue::Unary const& s) {
                oss << s.first << rvalue_to_string(s.second->value) << space;
            },
            [&](RValue::Relation const& s) {
                for (auto const& relation : s.second) {
                    oss << rvalue_to_string(relation->value) << space;
                }
            },
            [&](RValue::Function const& s) { oss << s.first.first << space; },
            [&](RValue::Symbol const& s) { oss << s.first.first << space; } },
        rvalue);
    return oss.str();
}

/**
 * @brief Queue to string of operators and operands in reverse-polish notation
 */
std::string queue_of_rvalues_to_string(RValue_Queue* rvalues_queue)
{
    using namespace type;
    auto oss = std::ostringstream();
    for (auto& item : *rvalues_queue) {
        std::visit(util::overload{ [&](type::Operator op) {
                                      oss << type::operator_to_string(op)
                                          << " ";
                                  },
                                   [&](type::RValue::Type_Pointer const& s) {
                                       oss << rvalue_to_string(*s);
                                   }

                   },
                   item);
    }
    return oss.str();
}

namespace detail {

/**
 * @brief Operator precedence check of the queue and operator stack
 */
constexpr void _associativity_operator_precedence(
    type::Operator op1,
    RValue_Queue* rvalues_queue,
    std::stack<type::Operator>& operator_stack)
{
    using namespace type;
    while (!operator_stack.empty()) {
        auto op2 = operator_stack.top();
        if ((is_left_associative(op1) &&
             get_precedence(op2) <= get_precedence(op2)) ||
            (!is_left_associative(op1) &&
             get_precedence(op1) < get_precedence(op2))) {
            rvalues_queue->emplace_back(operator_stack.top());
            operator_stack.pop();
        } else {
            break;
        }
    }
}

/**
 * @brief Re-balance the queue if the stack is empty
 */
constexpr void _balance_queue(RValue_Queue* rvalues_queue,
                              std::stack<type::Operator>& operator_stack)
{
    if (operator_stack.size() == 1) {
        rvalues_queue->emplace_back(operator_stack.top());
        operator_stack.pop();
    }
}

/**
 * @brief Queue construction via operators and rvalues ordered by precedence
 */
RValue_Queue* _rvalue_pointer_to_queue(
    type::RValue::Type_Pointer rvalue_pointer,
    RValue_Queue* rvalues_queue,
    std::stack<type::Operator>& operator_stack) // cant be constexpr until C++26
{
    using namespace type;
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](type::RValue::RValue_Pointer const& s) {
                auto value = rvalue_type_pointer_from_rvalue(s->value);
                _rvalue_pointer_to_queue(value, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Value_Pointer&) {
                rvalues_queue->emplace_back(rvalue_pointer);
            },
            [&](type::RValue::Value&) {
                rvalues_queue->emplace_back(rvalue_pointer);
            },
            [&](type::RValue::LValue&) {
                rvalues_queue->emplace_back(rvalue_pointer);
            },
            [&](type::RValue::Unary const& s) {
                auto op1 = s.first;
                auto rhs = rvalue_type_pointer_from_rvalue(s.second->value);
                _rvalue_pointer_to_queue(rhs, rvalues_queue, operator_stack);
                operator_stack.emplace(op1);
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Relation const& s) {
                auto op1 = s.first;
                if (s.second.size() == 2) {
                    auto lhs =
                        rvalue_type_pointer_from_rvalue(s.second.at(0)->value);
                    auto rhs =
                        rvalue_type_pointer_from_rvalue(s.second.at(1)->value);
                    _rvalue_pointer_to_queue(
                        lhs, rvalues_queue, operator_stack);
                    operator_stack.emplace(op1);
                    _rvalue_pointer_to_queue(
                        rhs, rvalues_queue, operator_stack);
                } else if (s.second.size() == 4) {
                    // ternary
                    operator_stack.emplace(Operator::B_TERNARY);
                    operator_stack.emplace(Operator::U_PUSH);
                    auto ternary_lhs =
                        rvalue_type_pointer_from_rvalue(s.second.at(2)->value);
                    auto ternary_rhs =
                        rvalue_type_pointer_from_rvalue(s.second.at(3)->value);
                    _rvalue_pointer_to_queue(
                        ternary_lhs, rvalues_queue, operator_stack);
                    _rvalue_pointer_to_queue(
                        ternary_rhs, rvalues_queue, operator_stack);
                    auto ternary_truthy =
                        rvalue_type_pointer_from_rvalue(s.second.at(0)->value);
                    operator_stack.emplace(op1);
                    auto ternary_falsey =
                        rvalue_type_pointer_from_rvalue(s.second.at(1)->value);
                    _rvalue_pointer_to_queue(
                        ternary_truthy, rvalues_queue, operator_stack);
                    _rvalue_pointer_to_queue(
                        ternary_falsey, rvalues_queue, operator_stack);
                }
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Function const& s) {
                auto op1 = Operator::U_CALL;
                auto lhs = rvalue_type_pointer_from_rvalue(s.first);
                operator_stack.emplace(op1);
                _rvalue_pointer_to_queue(lhs, rvalues_queue, operator_stack);
                for (auto const& parameter : s.second) {
                    auto param =
                        rvalue_type_pointer_from_rvalue(parameter->value);
                    operator_stack.emplace(Operator::U_PUSH);
                    _rvalue_pointer_to_queue(
                        param, rvalues_queue, operator_stack);
                }
                _balance_queue(rvalues_queue, operator_stack);
                _associativity_operator_precedence(
                    op1, rvalues_queue, operator_stack);
            },
            [&](type::RValue::Symbol const& s) {
                auto op1 = Operator::B_ASSIGN;
                auto lhs = rvalue_type_pointer_from_rvalue(s.first);
                auto rhs = rvalue_type_pointer_from_rvalue(s.second->value);
                _rvalue_pointer_to_queue(lhs, rvalues_queue, operator_stack);
                _rvalue_pointer_to_queue(rhs, rvalues_queue, operator_stack);
                operator_stack.emplace(op1);
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
RValue_Queue* rvalues_to_queue(
    std::vector<type::RValue::Type_Pointer> const& rvalues,
    RValue_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    for (type::RValue::Type_Pointer const& rvalue : rvalues) {
        credence::detail::_rvalue_pointer_to_queue(
            rvalue, rvalues_queue, operator_stack);
    }

    return rvalues_queue;
}

/**
 * @brief type::RValue to queue of operators and operands
 */
RValue_Queue* rvalues_to_queue(type::RValue::Type_Pointer const& rvalue,
                               RValue_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    credence::detail::_rvalue_pointer_to_queue(
        rvalue, rvalues_queue, operator_stack);
    return rvalues_queue;
}

} // namespace credence