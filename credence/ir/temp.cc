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

#include <credence/ir/qaud.h> // for make_quadruple, Instructions, Instruction
#include <credence/ir/temp.h>
#include <credence/operators.h> // for Operator, operator_to_string
#include <credence/queue.h>     // for RValue_Queue
#include <credence/types.h>     // for RValue, rvalue_type_pointer_from_rvalue
#include <credence/util.h>      // for rvalue_to_string, overload
#include <deque>                // for deque
#include <format>               // for format
#include <map>                  // for map
#include <matchit.h>            // for Wildcard, Ds, _, Meet, pattern, ds, match
#include <memory>               // for shared_ptr
#include <stack>                // for stack
#include <string>               // for basic_string, string, to_string
#include <tuple>                // for tuple, get, make_tuple
#include <utility>              // for pair, make_pair, cmp_equal
#include <variant>              // for visit, monostate, variant
#include <vector>               // for vector

/****************************************************************************
 *  A set of functions that aid construction of 3- or 4- tuple temporaries
 *  and linear instructions from an rvalue stack. The rvalue stack should
 *  be ordered by operator precedence, thus constituting an IR that closely
 *  resembles a generic platform assembly language.
 *
 *  Example:
 *
 *  main() {
 *    auto x;
 *    x = (5 + 5) * (6 + 6);
 *  }
 *
 *  Becomes:
 *
 *  __main:
 *   BeginFunc ;
 *.   _t1 = (5:int:4) + (5:int:4);
 *.   _t2 = (6:int:4) + (6:int:4);
 *.   _t3 = _t1 * _t2;
 *.   x = _t3;
 *   EndFunc ;
 *
 ****************************************************************************/

namespace credence {

namespace ir {

using namespace type;

namespace detail {

/**
 * @brief
 * Pop exactly 1 operand and 1 temporary
 * from each stack into instruction tuple
 */
void binary_operands_balanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    type::Operator op,
    int* temporary)
{
    auto operand1 = operand_stack.top();
    auto rhs = temporary_stack.top();

    if (operand_stack.size() > 1) {
        operand_stack.pop();
    }
    temporary_stack.pop();

    auto lhs = instruction_temporary_from_rvalue_operand(operand1, temporary);
    instructions.insert(
        instructions.end(), lhs.second.begin(), lhs.second.end());
    auto temp_rhs = make_temporary(temporary, rhs);
    instructions.push_back(temp_rhs);
    temporary_stack.push(std::format(
        "{} {} {}", lhs.first, operator_to_string(op), std::get<1>(temp_rhs)));
    // an lvalue at the end of a call stack
    if (operand1->index() == 6 and operand_stack.size() == 0) {
        auto temp_lhs = make_temporary(temporary,
                                       std::format("{} {} {}",
                                                   lhs.first,
                                                   operator_to_string(op),
                                                   std::get<1>(temp_rhs)));
        instructions.push_back(temp_lhs);
    }
}

/**
 * @brief Create and insert instructions from an rvalue operand
 *  See RValue in `types.h' for details.
 */
std::pair<std::string, std::size_t> insert_create_temp_from_operand(
    type::RValue::Type_Pointer operand,
    Instructions& instructions,
    int* temporary)
{
    auto inst_temp =
        instruction_temporary_from_rvalue_operand(operand, temporary);
    if (inst_temp.second.size() > 0) {
        instructions.insert(instructions.end(),
                            inst_temp.second.begin(),
                            inst_temp.second.end());
        return std::make_pair(inst_temp.first, inst_temp.second.size());
    } else {
        return std::make_pair(util::rvalue_to_string(*operand, false), 0);
    }
}

/**
 * @brief
 * There is only one operand on the stack, and no temporaries,
 * so use the lvalue from the last instruction for the LHS.
 */
void binary_operands_unbalanced_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    type::Operator op,
    int* temporary)
{
    using namespace matchit;
    auto rhs_lvalue = util::rvalue_to_string(*operand_stack.top(), false);
    auto operand = operand_stack.top();

    if (instructions.empty())
        return;

    auto last = instructions[instructions.size() - 1];

    match(static_cast<int>(instructions.size()))(
        pattern | (_ > 1) =
            [&] {
                auto last_by_one = instructions[instructions.size() - 2];
                auto operand_temp =
                    make_temporary(temporary,
                                   std::format("{} {} {}",
                                               std::get<1>(last_by_one),
                                               operator_to_string(op),
                                               std::get<1>(last)));
                instructions.push_back(operand_temp);
                temporary_stack.push(std::get<1>(operand_temp));
            },
        pattern | 1 =
            [&] {
                auto operand_temp =
                    make_temporary(temporary,
                                   std::format("{} {} {}",
                                               rhs_lvalue,
                                               operator_to_string(op),
                                               std::get<1>(last)));
                instructions.push_back(operand_temp);
                temporary_stack.push(std::get<1>(operand_temp));
            }

    );
}

/**
 * @brief
 * Construct a temporary lvalue from a recursive rvalue
 *  See RValue in `types.h' for details.
 */
std::pair<std::string, Instructions> instruction_temporary_from_rvalue_operand(
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
                auto unwrap_rhs_type =
                    type::rvalue_type_pointer_from_rvalue(s.second->value);
                auto rhs = instruction_temporary_from_rvalue_operand(
                    unwrap_rhs_type, temporary_size);
                instructions.insert(
                    instructions.end(), rhs.second.begin(), rhs.second.end());
                auto temp = make_temporary(temporary_size, rhs.first);

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
                    auto unwrap_lhs_type =
                        type::rvalue_type_pointer_from_rvalue(
                            s.second.at(0)->value);
                    auto unwrap_rhs_type =
                        type::rvalue_type_pointer_from_rvalue(
                            s.second.at(1)->value);
                    auto lhs = instruction_temporary_from_rvalue_operand(
                        unwrap_lhs_type, temporary_size);
                    auto rhs = instruction_temporary_from_rvalue_operand(
                        unwrap_rhs_type, temporary_size);
                    auto relation =
                        make_temporary(temporary_size,
                                       std::format("{} {} {}",
                                                   lhs.first,
                                                   operator_to_string(op),
                                                   rhs.first));

                    instructions.push_back(relation);
                    temp_name = std::get<1>(relation);

                } else if (s.second.size() == 4) {
                    // ternary
                }
            },
            [&](RValue::Function& s) {
                temp_name = util::rvalue_to_string(s.first, false);
            },
            [&](RValue::Symbol& s) {
                temp_name = util::rvalue_to_string(s.first, false);
            } },
        *operand);

    return std::make_pair(temp_name, instructions);
}

/**
 * @brief Construct temporary instructions from assignment operator
 */
void assignment_operands_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    int* temporary)
{
    using namespace matchit;
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    match(oss, tss)(
        pattern | ds(_ >= 1, _ >= 1) =
            [&] {
                auto rhs = temporary_stack.top();
                temporary_stack.pop();
                auto lvalue = operand_stack.top();
                operand_stack.pop();
                auto lhs = instruction_temporary_from_rvalue_operand(lvalue,
                                                                     temporary);
                instructions.insert(
                    instructions.end(), lhs.second.begin(), lhs.second.end());
                instructions.push_back(
                    make_quadruple(Instruction::VARIABLE, lhs.first, rhs));
            },
        pattern | ds(_ == 1, _ == 0) =
            [&] {
                auto lhs_rvalue =
                    util::rvalue_to_string(*operand_stack.top(), false);
                operand_stack.pop();
                if (instructions.size() > 1) {
                    auto last = instructions[instructions.size() - 1];
                    instructions.push_back(make_quadruple(
                        Instruction::VARIABLE, lhs_rvalue, std::get<1>(last)));
                }
            },
        pattern | ds(_ >= 2, _ == 0) =
            [&] {
                auto operand1 = operand_stack.top();
                operand_stack.pop();
                auto operand2 = operand_stack.top();
                operand_stack.pop();

                auto lhs = instruction_temporary_from_rvalue_operand(operand2,
                                                                     temporary);
                auto rhs = instruction_temporary_from_rvalue_operand(operand1,
                                                                     temporary);

                instructions.insert(
                    instructions.end(), lhs.second.begin(), lhs.second.end());
                instructions.insert(
                    instructions.end(), rhs.second.begin(), rhs.second.end());
                instructions.push_back(make_quadruple(
                    Instruction::VARIABLE, lhs.first, rhs.first));
            });
}

} // namespace detail

/**
 * @brief
 * Unary operators and temporary stack to instructions
 *
 * clang-format off
 *
 *  _t1 = glt(6) || glt(2)
 *  _t2 = ~ 5
 *  _t3 = _t1 || _t2
 *    x = _t3
 *
 * clang-format on
 *
 * The final temporary is "_t3", which we set to our x operand.
 *
 * If there is a stack of temporaries from a previous operand, we pop them
 * and use the newest temporary's idenfitier for the instruction name of the
 * top of the temporary stack.
 *
 * I.e., the sub expression "~ 5" was pushed on to a temporary stack, with
 * identifier _t2. We popped it off and used it in our final temporary.
 */
void unary_operand_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    Operator op,
    int* temporary)
{
    using namespace detail;
    using namespace matchit;
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    match(oss, tss)(
        // the primary operand stack is empty, do nothing
        pattern | ds(_ == 0, _) = [&] { return; },
        //  the temporary stack is empty, create our first temp lvalue
        pattern | ds(_, _ > 1) =
            [&] {
                auto operand1 = temporary_stack.top();
                temporary_stack.pop();
                auto unary = make_temporary(
                    temporary,
                    std::format("{} {}", operator_to_string(op), operand1));
                temporary_stack.push(std::format(
                    "{} {}", operator_to_string(op), std::get<1>(unary), ""));
            },
        pattern | _ =
            [&] {
                auto operand1 = operand_stack.top();
                operand_stack.pop();
                if (temporary_stack.size() == 1) {
                    // if the temporary stack has a single
                    // lvalue, pop to its own temporary first
                    auto operand1 = temporary_stack.top();
                    temporary_stack.pop();
                    auto last_expression = make_temporary(temporary, operand1);
                    instructions.push_back(last_expression);
                    temporary_stack.push(std::get<1>(last_expression));
                }
                auto rhs = instruction_temporary_from_rvalue_operand(operand1,
                                                                     temporary);
                instructions.insert(
                    instructions.end(), rhs.second.begin(), rhs.second.end());
                match(std::cmp_equal(tss, 0))(
                    pattern | true =
                        [&] {
                            // No temporary stack available, so push the newest
                            // temporary as an lvalue operand on the main stack
                            auto operand_temp = make_temporary(
                                temporary,
                                std::format("{} {}",
                                            operator_to_string(op),
                                            rhs.first));
                            type::RValue::LValue temp_lvalue = std::make_pair(
                                std::get<1>(operand_temp), NULL_DATA_TYPE);
                            operand_stack.push(
                                rvalue_type_pointer_from_rvalue(temp_lvalue));
                            instructions.push_back(operand_temp);
                        },
                    pattern | _ =
                        [&] {
                            // pop the last expression as the unary operand
                            auto unary = make_temporary(
                                temporary,
                                std::format("{} {}",
                                            operator_to_string(op),
                                            rhs.first));
                            instructions.push_back(unary);
                            temporary_stack.push(std::get<1>(unary));
                        }

                );
            });
}

/**
 * @brief
 * Binary operators and temporary stack to instructions
 *
 * Consider the expression `(x > 1 || x < 1)`
 *
 * In order to express this in a set of 3 or 4 expressions, we create the
 * temporaries:
 *
 * clang-format off
 *
 *  _t1 = x > 1
 *  _t2 = x < 1
 *  _t3 = _t1 || _t2
 *
 * clang-format on
 *
 * The binary temporary is "_t3", which we return.
 *
 * We must also keep note of evaluated expressions, i.e wrapped in parenthesis:
 *
 * `(5 + 5) * (6 * 6)`
 *
 * Becomes:
 *
 * clang-format off
 *
 * _t1 = (5:int:4) + (5:int:4);
 * _t2 = (6:int:4) + (6:int:4);
 * _t3 = _t1 * _t2;
 *
 * clang-format on
 *
 * If there is a stack of temporaries from a sub-expression in an operand,
 * we pop them and use the last temporary's idenfitier for the instruction
 * name of the top of the temporary stack.
 *
 * I.e., the sub-expressions `x > 1` and `x < 1` were popped of the
 * temporary stack, which were assigned _t1 and _t2, and assigned _t3 the
 * final binary expression.
 */
void binary_operands_to_temporary_stack(
    std::stack<type::RValue::Type_Pointer>& operand_stack,
    std::stack<std::string>& temporary_stack,
    Instructions& instructions,
    Operator op,
    int* temporary)
{
    using namespace detail;
    using namespace matchit;
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    match(oss, tss)(
        // there are at least two items on the temporary
        // stack, pop them as use them as operands
        pattern | ds(_, _ >= 2) =
            [&] {
                auto rhs = temporary_stack.top();
                temporary_stack.pop();
                auto lhs = temporary_stack.top();
                temporary_stack.pop();
                auto last_instruction = make_temporary(
                    temporary,
                    std::format("{} {} {}", lhs, operator_to_string(op), rhs));
                instructions.push_back(last_instruction);
            },
        // there is exactly one temporary lvalue,
        // and at least one rvalue operand to use
        pattern | ds(_ >= 1, _ == 1) =
            [&] {
                binary_operands_balanced_temporary_stack(operand_stack,
                                                         temporary_stack,
                                                         instructions,
                                                         op,
                                                         temporary);
            },
        pattern | _ =
            [&] {
                match(oss)(
                    // if there is only one operand on the stack, the next
                    // result was already evaluated and we take the temporary
                    // lvalues from the last two instructions as operands
                    pattern | (oss == 1) =
                        [&] {
                            binary_operands_unbalanced_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                        },

                    // empty rvalue operand stack, do nothing
                    pattern | (_ < 1) = [&] { return; },

                    // there are two or more operands on
                    // the primary rvalue stack, use them
                    pattern | _ =
                        [&] {
                            auto operand1 = operand_stack.top();
                            operand_stack.pop();
                            auto operand2 = operand_stack.top();
                            operand_stack.pop();
                            auto rhs_name = insert_create_temp_from_operand(
                                operand1, instructions, temporary);
                            auto lhs_name = insert_create_temp_from_operand(
                                operand2, instructions, temporary);

                            if (lhs_name.second == 0 and
                                rhs_name.second == 0 and tss == 0) {
                                // if the lhs is an lvalue, use the
                                // temporary from the last instruction
                                auto operand_temp = make_temporary(
                                    temporary,
                                    std::format("{} {} {}",
                                                lhs_name.first,
                                                operator_to_string(op),
                                                rhs_name.first));
                                type::RValue::LValue temp_lvalue =
                                    std::make_pair(std::get<1>(operand_temp),
                                                   NULL_DATA_TYPE);
                                operand_stack.push(
                                    rvalue_type_pointer_from_rvalue(
                                        temp_lvalue));
                                instructions.push_back(operand_temp);

                            } else {
                                temporary_stack.push(
                                    std::format("{} {} {}",
                                                lhs_name.first,
                                                operator_to_string(op),
                                                rhs_name.first));
                            }
                        }

                );
            });
}

/**
 * @brief
 * Construct a set of linear instructions in qaudruple form based on an rvalue
 * queue.
 *
 * Uses `unary_operand_to_temporary_stack',
 * `binary_operands_to_temporary_stack'
 *   for operator translation.
 *
 */
Instructions rvalue_queue_to_linear_ir_instructions(RValue_Queue* queue,
                                                    int* temporary)
{
    using namespace detail;
    using namespace matchit;
    Instructions instructions{};
    if (queue->empty()) {
        return instructions;
    }
    std::stack<std::string> temporary_stack{};
    std::stack<type::RValue::Type_Pointer> operand_stack{};
    int param_on_stack = 0;
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
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::POST_INC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        case Operator::PRE_DEC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::POST_DEC:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // bit ops
                        case Operator::RSHIFT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::OR:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::AND:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::LSHIFT:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;
                        case Operator::XOR:
                            binary_operands_to_temporary_stack(operand_stack,
                                                               temporary_stack,
                                                               instructions,
                                                               op,
                                                               temporary);
                            break;

                        case Operator::U_NOT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_ONES_COMPLEMENT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // pointer operators
                        case Operator::U_SUBSCRIPT:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_INDIRECTION:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_ADDR_OF:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;
                        case Operator::U_MINUS:
                            unary_operand_to_temporary_stack(operand_stack,
                                                             temporary_stack,
                                                             instructions,
                                                             op,
                                                             temporary);
                            break;

                        // lvalue and address operators
                        case Operator::U_CALL: {
                            if (temporary_stack.size() > 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                auto temp_rhs = make_temporary(temporary, rhs);
                                instructions.push_back(temp_rhs);
                                temporary_stack.push(
                                    std::format("{} {}",
                                                operator_to_string(op),
                                                std::get<1>(temp_rhs)));
                                instructions.push_back(
                                    make_quadruple(Instruction::CALL, rhs, ""));
                                temporary_stack.push(rhs);
                            } else {
                                if (temporary_stack.size() == 1 and
                                    operand_stack.size() < 1) {
                                    auto rhs = temporary_stack.top();
                                    temporary_stack.pop();
                                    auto temp_rhs =
                                        make_temporary(temporary, rhs);
                                    instructions.push_back(temp_rhs);
                                    temporary_stack.push(
                                        std::format("{} {}",
                                                    operator_to_string(op),
                                                    std::get<1>(temp_rhs)));
                                    instructions.push_back(make_quadruple(
                                        Instruction::CALL, rhs, ""));
                                    temporary_stack.push(rhs);
                                }
                                if (operand_stack.size() < 1)
                                    return;
                                auto operand = operand_stack.top();
                                operand_stack.pop();
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand, temporary);
                                instructions.insert(instructions.end(),
                                                    rhs.second.begin(),
                                                    rhs.second.end());

                                instructions.push_back(make_quadruple(
                                    Instruction::CALL, rhs.first, ""));

                                temporary_stack.push(
                                    instruction_to_string(Instruction::RETURN));
                            }
                            instructions.push_back(make_quadruple(
                                Instruction::POP,
                                std::to_string(param_on_stack *
                                               type::Type_["word"].second),
                                "",
                                ""));

                            param_on_stack = 0;
                        } break;
                        case Operator::U_PUSH: {
                            if (temporary_stack.size() >= 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                instructions.push_back(
                                    make_quadruple(Instruction::PUSH, rhs, ""));
                            } else {
                                if (operand_stack.size() < 1)
                                    return;
                                auto operand = operand_stack.top();
                                operand_stack.pop();
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand, temporary);
                                instructions.insert(instructions.end(),
                                                    rhs.second.begin(),
                                                    rhs.second.end());
                                instructions.push_back(make_quadruple(
                                    Instruction::PUSH, rhs.first, ""));
                            }
                            param_on_stack++;
                        } break;
                        case Operator::B_ASSIGN:
                            assignment_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                temporary);

                            break;
                        case Operator::B_TERNARY:
                            break;
                    }
                },
                [&](type::RValue::Type_Pointer& s) { operand_stack.push(s); } },
            item);
    }
    return instructions;
}

} // namespace ir

} // namespace credence