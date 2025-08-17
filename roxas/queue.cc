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

#include <cstddef>           // for size_t
#include <memory>            // for make_shared, shared_ptr
#include <roxas/operators.h> // for get_precedence, is_left_associative
#include <roxas/queue.h>
#include <roxas/types.h> // for RValue
#include <roxas/util.h>  // for overload
#include <stack>         // for stack
#include <string>        // for basic_string
#include <variant>       // for variant, monostate, visit

namespace roxas {

namespace detail {

/**
 * @brief RValue pointer to mutual recursive queue of operators and values
 *
 * @param RValue_Pointer
 * @param rvalues_queue
 * @return RValue_Evaluation_Queue*
 */
RValue_Evaluation_Queue* _rvalue_pointer_to_queue(
    type::RValue::Type_Pointer RValue_Pointer,
    RValue_Evaluation_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](type::RValue::RValue_Pointer& s) {
                auto value = std::make_shared<type::RValue::Type>(s->value);
                auto RValue_Pointers =
                    _rvalue_pointer_to_queue(value, rvalues_queue);
                for (std::size_t i = 0; i < RValue_Pointers->size(); i++) {
                    rvalues_queue->push_back(RValue_Pointers->front());
                    RValue_Pointers->pop_back();
                }
            },
            [&](type::RValue::Value&) {
                rvalues_queue->push_back(RValue_Pointer);
            },
            [&](type::RValue::LValue&) {
                rvalues_queue->push_back(RValue_Pointer);
            },
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
                    auto RValue_Pointers =
                        _rvalue_pointer_to_queue(value, rvalues_queue);
                    for (std::size_t i = 0; i < RValue_Pointers->size(); i++) {
                        rvalues_queue->push_back(RValue_Pointers->front());
                        RValue_Pointers->pop_back();
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
                    auto RValue_Pointers =
                        _rvalue_pointer_to_queue(param, rvalues_queue);
                    for (std::size_t i = 0; i < RValue_Pointers->size(); i++) {
                        rvalues_queue->push_back(RValue_Pointers->front());
                        RValue_Pointers->pop_back();
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
                rvalues_queue->push_back(RValue_Pointer);
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
                rvalues_queue->push_back(RValue_Pointer);
            } },
        *RValue_Pointer);
    return rvalues_queue;
}

} // namespace detail

/**
 * @brief List of rvalues to mutual recursive queue of operators and values
 *
 * @param rvalues
 * @param rvalues_queue
 * @return RValue_Evaluation_Queue*
 */
RValue_Evaluation_Queue* rvalues_to_queue(
    std::vector<type::RValue::Type_Pointer>& rvalues,
    RValue_Evaluation_Queue* rvalues_queue)
{
    using namespace type;
    std::stack<Operator> operator_stack{};
    for (type::RValue::Type_Pointer& rvalue : rvalues) {
        roxas::detail::_rvalue_pointer_to_queue(rvalue, rvalues_queue);
    }

    return rvalues_queue;
}

} // namespace roxas