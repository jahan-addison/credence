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
#include <roxas/ir/qaud.h>
#include <assert.h>           // for assert
#include <matchit.h>          // for pattern, match, PatternHelper, PatternP...
#include <roxas/ir/table.h>   // for Table
#include <roxas/json.h>       // for JSON
#include <roxas/operators.h>  // for Operator, operator_to_string
#include <roxas/queue.h>      // for RValue_Queue, rvalues_to_queue
#include <roxas/symbol.h>     // for Symbol_Table
#include <roxas/types.h>      // for RValue, rvalue_type_pointer_from_rvalue
#include <roxas/util.h>       // for rvalue_to_string, overload
#include <deque>              // for deque, operator==, _Deque_iterator
#include <format>             // for format, format_string
#include <list>               // for operator==, _List_iterator
#include <memory>             // for __shared_ptr_access, shared_ptr
#include <stack>              // for stack
#include <stdexcept>          // for runtime_error
#include <utility>            // for pair, make_pair
#include <variant>            // for visit, monostate, variant
// clang-format on

namespace roxas {

namespace ir {

namespace qaud {

using namespace type;

namespace detail {

using LValue_Instruction = std::pair<std::string, Instructions>;

inline void instruction_error(type::Operator op)
{
    throw std::runtime_error(
        std::format("runtime error: invalid stack for operator \"{}\"",
                    type::operator_to_string(op)));
}

inline Quadruple make_quadruple(Instruction op,
                                std::string const& s1,
                                std::string const& s2,
                                std::string const& s3 = "")
{
    return std::make_tuple(op, s1, s2, s3);
}

inline Quadruple make_temporary(int* temporary_size, std::string const& temp)
{
    auto lhs = std::format("t{}", ++(*temporary_size), temp);
    return make_quadruple(Instruction::VARIABLE, lhs, temp);
}

LValue_Instruction instruction_temporary_from_rvalue_operand(
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
                temp_name = util::rvalue_to_string(*operand, false);
            },
            [&](RValue::Unary& s) {
                auto op = s.first;
                auto rhs = util::rvalue_to_string(*operand, false);
                auto temp = make_temporary(temporary_size, rhs);
                instructions.push_back(temp);
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
                    auto lhs = util::rvalue_to_string(s.second.at(0), false);
                    auto rhs = util::rvalue_to_string(s.second.at(1), false);
                    auto temp_lhs = make_temporary(temporary_size, lhs);
                    auto temp_rhs = make_temporary(temporary_size, rhs);
                    auto relation =
                        make_temporary(temporary_size,
                                       std::format("{} {} {}",
                                                   std::get<1>(temp_lhs),
                                                   operator_to_string(op),
                                                   std::get<1>(temp_rhs)));

                    instructions.push_back(temp_rhs);
                    instructions.push_back(temp_lhs);
                    instructions.push_back(relation);
                    temp_name = std::get<1>(relation);

                } else if (s.second.size() == 4) {
                    // ternary
                }
            },
            [&](RValue::Function&) {},
            [&](RValue::Symbol&) {} },
        *operand);

    return std::make_pair(temp_name, instructions);
}

void binary_operands_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    Operator op,
    int* temporary)
{
    if (operand_stack.size() < 2)
        instruction_error(op);
    if (temporary_stack.size() >= 2) {
        auto lhs = temporary_stack.top();
        temporary_stack.pop();
        auto rhs = temporary_stack.top();
        temporary_stack.pop();
        temporary_stack.push(
            std::format("{} {} {}", lhs, operator_to_string(op), rhs));
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

Instructions rvalue_queue_to_instructions(RValue_Queue* queue, int* temporary)
{
    Instructions instructions{};
    std::stack<std::string> temporary_stack{};
    std::stack<type::RValue::Type_Pointer> operand_stack{};
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
                            break;
                        case Operator::POST_INC:
                            break;

                        case Operator::PRE_DEC:
                            break;
                        case Operator::POST_DEC:
                            break;

                        // bit ops
                        case Operator::RSHIFT:
                            break;
                        case Operator::OR:
                            break;
                        case Operator::AND:
                            break;
                        case Operator::LSHIFT:
                            break;
                        case Operator::XOR:
                            break;

                        case Operator::U_NOT:
                            break;
                        case Operator::U_ONES_COMPLEMENT:
                            break;

                        // pointer operators
                        case Operator::U_SUBSCRIPT:
                        case Operator::U_INDIRECTION:
                            break;
                        case Operator::U_ADDR_OF:
                            break;
                        case Operator::U_MINUS:
                            break;

                        // lvalue and address operators
                        case Operator::U_CALL:
                            break;
                        case Operator::U_PUSH:
                            break;
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

} // namespace detail

/**
 * @brief auto statement symbol construction
 */
void build_from_auto_statement(Symbol_Table<>& symbols, Node& node)
{
    using namespace matchit;
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("auto") == 0);
    assert(node.hasKey("left"));
    auto left_child_node = node["left"];
    for (auto& ident : left_child_node.ArrayRange()) {
        match(ident["node"].ToString())(
            pattern | "lvalue" =
                [&] {
                    symbols.set_symbol_by_name(ident["root"].ToString(),
                                               NULL_DATA_TYPE);
                },
            pattern | "vector_lvalue" =
                [&] {
                    auto size = ident["left"]["root"].ToInt();
                    symbols.set_symbol_by_name(
                        ident["root"].ToString(),
                        { static_cast<type::Byte>('0'), { "byte", size } });
                },
            pattern | "indirect_lvalue" =
                [&] {
                    symbols.set_symbol_by_name(
                        ident["left"]["root"].ToString(),
                        { "__WORD__", type::Type_["word"] });
                });
    }
}

Instructions build_from_rvalue_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details)
{
    assert(node["node"].ToString().compare("statement") == 0);
    assert(node["root"].ToString().compare("rvalue") == 0);
    int temporary{ 0 };
    Instructions instructions{};
    assert(node.hasKey("left"));
    std::vector<type::RValue::Type_Pointer> rvalues{};
    RValue_Queue list{};
    auto statement = node["left"];
    Table table{ details, symbols };
    // for each line:
    for (auto& expression : statement.ArrayRange()) {
        if (expression.JSONType() == json::JSON::Class::Array) {
            for (auto& rvalue : expression.ArrayRange()) {
                rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                    table.from_rvalue(rvalue).value));
            }
        } else {
            rvalues.push_back(type::rvalue_type_pointer_from_rvalue(
                table.from_rvalue(expression).value));
        }
        rvalues_to_queue(rvalues, &list);
        auto line = detail::rvalue_queue_to_instructions(&list, &temporary);
        instructions.insert(instructions.end(), line.begin(), line.end());
        rvalues.clear();
        list.clear();
    }
    return instructions;
}

} // namespace qaud

} // namespace ir

} // namespace roxas