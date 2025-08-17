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

#include <cstddef> // for size_t
#include <map>     // for map
#include <memory>  // for make_shared, shared_ptr
#include <ostream> // for basic_ostream, operator<<
#include <roxas/ir/util.h>
#include <roxas/operators.h> // for get_precedence, is_left_associa...
#include <roxas/types.h>     // for Type_, RValue, Value_Type, Byte
#include <roxas/util.h>      // for overload
#include <sstream>           // for basic_ostringstream, ostringstream
#include <stack>             // for stack
#include <string>            // for basic_string, char_traits, allo...
#include <utility>           // for pair
#include <variant>           // for variant, get, monostate, visit

namespace roxas {

namespace ir {

namespace detail {

RValue_Operator_Queue* _rvalue_to_operator_queue(
    type::RValue::Type_Pointer _rvalue,
    RValue_Operator_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](type::RValue::_RValue& s) {
                auto value = std::make_shared<type::RValue::Type>(s->value);
                auto _rvalues = _rvalue_to_operator_queue(value, rvalues_queue);
                for (std::size_t i = 0; i < _rvalues->size(); i++) {
                    rvalues_queue->push_back(_rvalues->front());
                    _rvalues->pop_back();
                }
            },
            [&](type::RValue::Value&) { rvalues_queue->push_back(_rvalue); },
            [&](type::RValue::LValue&) { rvalues_queue->push_back(_rvalue); },
            [&](type::RValue::Unary& s) {
                auto op1 = s.first;
                operator_stack.push(s.first);
                auto value =
                    std::make_shared<type::RValue::Type>(s.second->value);
                rvalues_queue->push_back(value);
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
            },
            [&](type::RValue::Relation& s) {
                auto op1 = s.first;
                operator_stack.push(op1);
                for (auto& operand : s.second) {
                    auto value =
                        std::make_shared<type::RValue::Type>(operand->value);
                    auto _rvalues =
                        _rvalue_to_operator_queue(value, rvalues_queue);
                    for (std::size_t i = 0; i < _rvalues->size(); i++) {
                        rvalues_queue->push_back(_rvalues->front());
                        _rvalues->pop_back();
                    }
                }
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
            },
            [&](type::RValue::Function& s) {
                auto op1 = Operator::U_CALL;
                operator_stack.push(op1);
                for (auto& parameter : s.second) {
                    operator_stack.push(Operator::U_PUSH);
                    auto param =
                        std::make_shared<type::RValue::Type>(parameter->value);
                    auto _rvalues =
                        _rvalue_to_operator_queue(param, rvalues_queue);
                    for (std::size_t i = 0; i < _rvalues->size(); i++) {
                        rvalues_queue->push_back(_rvalues->front());
                        _rvalues->pop_back();
                    }
                }
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
                rvalues_queue->push_back(_rvalue);
            },
            [&](type::RValue::Symbol&) {
                auto op1 = Operator::B_ASSIGN;
                operator_stack.push(Operator::B_ASSIGN);
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
                rvalues_queue->push_back(_rvalue);
            } },
        *_rvalue);
    return rvalues_queue;
}

} // namespace detail

RValue_Operator_Queue* rvalues_to_operator_queue(
    std::vector<type::RValue::Type_Pointer>& rvalues,
    RValue_Operator_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    for (type::RValue::Type_Pointer& rvalue : rvalues) {
        detail::_rvalue_to_operator_queue(rvalue, rvalues_queue);
    }

    return rvalues_queue;
}

std::string dump_value_type(type::Value_Type const& type,
                            std::string_view separator)
{
    using namespace type;
    std::ostringstream os;
    os << "(";
    std::visit(util::overload{
                   [&](int i) {
                       os << i << separator << Type_["int"].first << separator
                          << Type_["int"].second;
                   },
                   [&](long i) {
                       os << i << separator << Type_["long"].first << separator
                          << Type_["long"].second;
                   },
                   [&](float i) {
                       os << i << separator << Type_["float"].first << separator
                          << Type_["float"].second;
                   },
                   [&](double i) {
                       os << i << separator << Type_["double"].first
                          << separator << Type_["double"].second;
                   },
                   [&](bool i) {
                       os << std::boolalpha << i << separator
                          << Type_["bool"].first << separator
                          << Type_["bool"].second;
                   },
                   [&]([[maybe_unused]] std::monostate i) {
                       os << "null" << separator << Type_["null"].first
                          << separator << Type_["null"].second;
                   },
                   [&](type::Byte i) {
                       os << i << separator << Type_["byte"].first << separator
                          << type.second.second;
                   },
                   [&](char i) {
                       os << i << Type_["char"].first << separator
                          << Type_["char"].second;
                   },
                   [&]([[maybe_unused]] std::string const& s) {
                       if (s == "__WORD_") {
                           // pointer
                           os << "__WORD_" << separator << Type_["word"].first
                              << separator << Type_["word"].second;
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

} // namespace ir

} // namespace roxas