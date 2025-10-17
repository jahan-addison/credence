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

#include <credence/ir/temp.h>

#include <algorithm>            // for copy
#include <credence/ir/ita.h>    // for ITA
#include <credence/operators.h> // for Operator, operator_to_string
#include <credence/queue.h>     // for rvalue_to_string, rvalues_to_queue
#include <credence/rvalue.h>    // for RValue_Parser
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for RValue, rvalue_type_pointer_from_rvalue
#include <credence/util.h>      // for AST_Node, overload
#include <deque>                // for deque, operator==, _Deque_iterator
#include <format>               // for format, format_string
#include <mapbox/eternal.hpp>   // for element, map
#include <matchit.h>            // for Ds, Meet, _, pattern, ds, match, Wil...
#include <memory>               // for shared_ptr, __shared_ptr_access, uni...
#include <simplejson.h>         // for JSON
#include <string>               // for basic_string, allocator, string, to_...
#include <string_view>          // for basic_string_view, operator<=>
#include <tuple>                // for tuple, get
#include <utility>              // for pair, make_pair, cmp_equal
#include <variant>              // for variant, visit, monostate
#include <vector>               // for vector

/****************************************************************************
 *  A set of functions that construct temporary lvalues "_tX" that aid in
 *  breaking expressions into 3- or 4- tuples for linear instructions. The
 *  rvalue stack should be ordered by operator precedence.
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

namespace m = matchit;
using namespace type;

namespace detail {

std::pair<std::string, ITA::Instructions>
instruction_temporary_from_rvalue_operand(
    type::RValue::Type_Pointer& operand,
    int* temporary_size);

/**
 * @brief
 * Pop exactly 1 operand and 1 temporary
 * from each stack into instruction tuple
 */
void binary_operands_balanced_temporary_stack(
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
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
    auto temp_rhs = ITA::make_temporary(temporary, rhs);
    instructions.emplace_back(temp_rhs);
    temporary_stack.emplace(
        std::format(
            "{} {} {}",
            lhs.first,
            operator_to_string(op),
            std::get<1>(temp_rhs)));
    // an lvalue at the end of a call stack
    if (operand1->index() == 6 and operand_stack.size() == 0) {
        auto temp_lhs = ITA::make_temporary(
            temporary,
            std::format(
                "{} {} {}",
                lhs.first,
                operator_to_string(op),
                std::get<1>(temp_rhs)));
        instructions.emplace_back(temp_lhs);
    }
}

/**
 * @brief Create and insert instructions from an rvalue operand
 *  See type::RValue in `types.h' for details.
 */
std::pair<std::string, std::size_t> insert_create_temp_from_operand(
    type::RValue::Type_Pointer operand,
    ITA::Instructions& instructions,
    int* temporary)
{
    auto inst_temp =
        instruction_temporary_from_rvalue_operand(operand, temporary);
    if (inst_temp.second.size() > 0) {
        ITA::insert_instructions(instructions, inst_temp.second);
        return std::make_pair(inst_temp.first, inst_temp.second.size());
    } else {
        return std::make_pair(rvalue_to_string(*operand, false), 0);
    }
}

/**
 * @brief
 * There is only one operand on the stack, and no temporaries,
 * so use the lvalue from the last instruction for the LHS.
 */
void binary_operands_unbalanced_temporary_stack(
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
    type::Operator op,
    int* temporary)
{
    using namespace matchit;
    auto rhs_lvalue = rvalue_to_string(*operand_stack.top(), false);
    auto operand = operand_stack.top();

    if (instructions.empty())
        return;

    auto last = instructions[instructions.size() - 1];

    m::match(static_cast<int>(instructions.size()))(
        m::pattern | (_ > 1) =
            [&] {
                size_t last_index = 2;
                ITA::Quadruple last_lvalue =
                    instructions[instructions.size() - last_index];
                // backtrack the instruction stack and grab the last lvalue
                while (std::get<0>(last_lvalue) !=
                           ITA::Instruction::VARIABLE and
                       last_index < instructions.size()) {
                    last_lvalue =
                        instructions[instructions.size() - last_index];
                    last_index++;
                }
                auto operand_temp = ITA::make_temporary(
                    temporary,
                    std::format(
                        "{} {} {}",
                        std::get<1>(last_lvalue),
                        operator_to_string(op),
                        std::get<1>(last)));
                instructions.emplace_back(operand_temp);
                temporary_stack.emplace(std::get<1>(operand_temp));
            },
        // cppcheck-suppress syntaxError
        m::pattern | 1 =
            [&] {
                // clang-format off
                auto operand_temp =
                    ITA::make_temporary(temporary,
                    std::format("{} {} {}",
                        rhs_lvalue,
                        operator_to_string(op),
                        std::get<1>(last)));
                // clang-format on
                instructions.emplace_back(operand_temp);
                temporary_stack.emplace(std::get<1>(operand_temp));
            }

    );
}

/**
 * @brief
 * Construct a temporary lvalue from a recursive rvalue
 *  See type::RValue in `types.h' for details.
 */
// clang-format off
std::pair<std::string, ITA::Instructions>
instruction_temporary_from_rvalue_operand(
    RValue::Type_Pointer& operand,
    int* temporary_size)
{
    // clang-format on
    ITA::Instructions instructions{};
    std::string temp_name{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](RValue::RValue_Pointer& s) {
                auto unwrap_type =
                    type::rvalue_type_pointer_from_rvalue(s->value);
                auto rvalue_pointer = instruction_temporary_from_rvalue_operand(
                    unwrap_type, temporary_size);
                ITA::insert_instructions(instructions, rvalue_pointer.second);
                temp_name = rvalue_pointer.first;
            },
            [&](RValue::Value_Pointer&) {},
            [&](RValue::Value&) {
                temp_name = rvalue_to_string(*operand, false);
            },
            [&](RValue::LValue&) {
                temp_name = rvalue_to_string(*operand, false);
            },
            [&](RValue::Unary& s) {
                auto op = s.first;
                auto rhs_rvalue = s.second;
                auto unwrap_rhs_type =
                    type::rvalue_type_pointer_from_rvalue(s.second->value);
                auto rhs = instruction_temporary_from_rvalue_operand(
                    unwrap_rhs_type, temporary_size);
                ITA::insert_instructions(instructions, rhs.second);
                auto temp = ITA::make_temporary(temporary_size, rhs.first);

                auto unary = ITA::make_temporary(
                    temporary_size,
                    std::format(
                        "{} {}", operator_to_string(op), std::get<1>(temp)));
                instructions.emplace_back(unary);
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
                    // clang-format off
                    auto relation =
                        ITA::make_temporary(temporary_size,
                            std::format("{} {} {}",
                                lhs.first,
                                operator_to_string(op),
                                rhs.first));
                    // clang-format on
                    instructions.emplace_back(relation);
                    temp_name = std::get<1>(relation);
                }
            },
            [&](RValue::Function& s) {
                temp_name = rvalue_to_string(s.first, false);
            },
            [&](RValue::Symbol& s) {
                temp_name = rvalue_to_string(s.first, false);
            } },
        *operand);

    return std::make_pair(temp_name, instructions);
}

/**
 * @brief Construct temporary lvalues from assignment operator
 */
void assignment_operands_to_temporary_stack(
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
    int* temporary)
{
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    m::match(oss, tss)(
        m::pattern | m::ds(m::_ >= 1, m::_ >= 1) =
            [&] {
                auto rhs = temporary_stack.top();
                temporary_stack.pop();
                auto lvalue = operand_stack.top();
                operand_stack.pop();
                auto lhs = instruction_temporary_from_rvalue_operand(
                    lvalue, temporary);
                ITA::insert_instructions(instructions, lhs.second);
                instructions.emplace_back(
                    ITA::make_quadruple(
                        ITA::Instruction::VARIABLE, lhs.first, rhs));
            },
        m::pattern | ds(m::_ == 1, m::_ == 0) =
            [&] {
                auto lhs_rvalue = rvalue_to_string(*operand_stack.top(), false);
                operand_stack.pop();
                if (instructions.size() > 1) {
                    // clang-format off
                    auto last = instructions[instructions.size() - 1];
                    instructions.emplace_back(
                        ITA::make_quadruple(ITA::Instruction::VARIABLE,
                            lhs_rvalue,
                            std::get<1>(last)));
                    // clang-format on
                }
            },
        m::pattern | m::ds(m::_ >= 2, m::_ == 0) =
            [&] {
                auto operand1 = operand_stack.top();
                operand_stack.pop();
                auto operand2 = operand_stack.top();
                operand_stack.pop();

                auto lhs = instruction_temporary_from_rvalue_operand(
                    operand2, temporary);
                auto rhs = instruction_temporary_from_rvalue_operand(
                    operand1, temporary);
                ITA::insert_instructions(instructions, lhs.second);
                ITA::insert_instructions(instructions, rhs.second);
                instructions.emplace_back(
                    ITA::make_quadruple(
                        ITA::Instruction::VARIABLE, lhs.first, rhs.first));
            });
}

/**
 * @brief Construct temporary lvalues from function call
 */
void function_call_operands_to_temporary_instructions(
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
    int* temporary,
    int* param_on_stack)
{
    auto op = Operator::U_CALL;
    if (temporary_stack.size() > 1) {
        auto rhs = temporary_stack.top();
        temporary_stack.pop();
        auto temp_rhs = ITA::make_temporary(temporary, rhs);
        instructions.emplace_back(temp_rhs);
        temporary_stack.emplace(
            std::format(
                "{} {}", operator_to_string(op), std::get<1>(temp_rhs)));
        instructions.emplace_back(
            ITA::make_quadruple(ITA::Instruction::CALL, rhs));
        temporary_stack.emplace(rhs);
    } else {
        if (temporary_stack.size() == 1 and operand_stack.size() < 1) {
            auto rhs = temporary_stack.top();
            temporary_stack.pop();
            auto temp_rhs = ITA::make_temporary(temporary, rhs);
            instructions.emplace_back(temp_rhs);
            temporary_stack.emplace(
                std::format(
                    "{} {}", operator_to_string(op), std::get<1>(temp_rhs)));
            instructions.emplace_back(
                ITA::make_quadruple(ITA::Instruction::CALL, rhs));
            temporary_stack.emplace(rhs);
        }
        if (operand_stack.size() < 1)
            return;
        auto operand = operand_stack.top();
        operand_stack.pop();
        auto rhs =
            instruction_temporary_from_rvalue_operand(operand, temporary);
        ITA::insert_instructions(instructions, rhs.second);
        instructions.emplace_back(
            ITA::make_quadruple(ITA::Instruction::CALL, rhs.first));
    }
    if (*param_on_stack > 0)
        instructions.emplace_back(
            ITA::make_quadruple(
                ITA::Instruction::POP,
                std::to_string(
                    (*param_on_stack) * type::LITERAL_TYPE.at("word").second),
                "",
                ""));
    auto call_return = ITA::make_temporary(temporary, "RET");
    instructions.emplace_back(call_return);
    if (operand_stack.size() >= 1) {
        temporary_stack.emplace(std::get<1>(call_return));
    }
    *param_on_stack = 0;
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
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
    Operator op,
    int* temporary)
{
    using namespace detail;
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    m::match(oss, tss)(
        // the primary operand stack is empty, do nothing
        m::pattern | m::ds(m::_ == 0, m::_) = [&] { return; },
        //  the temporary stack is empty, create our first temp lvalue
        m::pattern | m::ds(m::_, m::_ > 1) =
            [&] {
                auto operand1 = temporary_stack.top();
                temporary_stack.pop();
                auto unary = ITA::make_temporary(
                    temporary,
                    std::format("{} {}", operator_to_string(op), operand1));
                temporary_stack.emplace(
                    std::format(
                        "{} {}",
                        operator_to_string(op),
                        std::get<1>(unary),
                        ""));
            },
        m::pattern | m::_ =
            [&] {
                auto operand1 = operand_stack.top();
                operand_stack.pop();
                if (temporary_stack.size() == 1) {
                    // if the temporary stack has a single
                    // lvalue, pop to its own temporary first
                    auto operand1 = temporary_stack.top();
                    temporary_stack.pop();
                    auto last_expression =
                        ITA::make_temporary(temporary, operand1);
                    instructions.emplace_back(last_expression);
                    temporary_stack.emplace(std::get<1>(last_expression));
                }
                auto rhs = instruction_temporary_from_rvalue_operand(
                    operand1, temporary);
                ITA::insert_instructions(instructions, rhs.second);
                m::match(std::cmp_equal(tss, 0))(
                    m::pattern | true =
                        [&] {
                            // If the operand is an lvalue, use it, otherwise
                            // create a temporary and assign it the unary rvalue
                            if (is_rvalue_type_pointer_variant(
                                    operand1, RValue_Type_Variant::LValue) and
                                is_in_place_unary_operator(op)) {
                                auto unary = ITA::make_quadruple(
                                    ITA::Instruction::VARIABLE,
                                    rvalue_to_string(*operand1, false),
                                    operator_to_string(op),
                                    rhs.first);
                                instructions.emplace_back(unary);
                                operand_stack.emplace(operand1);
                            } else {
                                auto operand_temp = ITA::make_temporary(
                                    temporary,
                                    std::format(
                                        "{} {}",
                                        operator_to_string(op),
                                        rhs.first));
                                instructions.emplace_back(operand_temp);
                                temporary_stack.emplace(
                                    std::get<1>(operand_temp));
                            }
                        },
                    m::pattern | m::_ =
                        [&] {
                            // pop the last expression as the unary operand
                            auto unary = ITA::make_temporary(
                                temporary,
                                std::format(
                                    "{} {}",
                                    operator_to_string(op),
                                    rhs.first));
                            instructions.emplace_back(unary);
                            temporary_stack.emplace(std::get<1>(unary));
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
 * I.e., the sub-expressions `x > 1` and `x < 1` were popped off the
 * temporary stack, which were assigned _t1 and _t2, and used in _t3
 * for the final binary expression.
 */
void binary_operands_to_temporary_stack(
    detail::Operand_Stack& operand_stack,
    detail::Temporary_Stack& temporary_stack,
    ITA::Instructions& instructions,
    Operator op,
    int* temporary)
{
    using namespace detail;
    auto oss = static_cast<int>(operand_stack.size());
    auto tss = static_cast<int>(temporary_stack.size());
    m::match(oss, tss)(
        // there are at least two items on the temporary
        // stack, pop them as use them as operands
        m::pattern | m::ds(m::_, m::_ >= 2) =
            [&] {
                auto rhs = temporary_stack.top();
                temporary_stack.pop();
                auto lhs = temporary_stack.top();
                temporary_stack.pop();
                auto last_instruction = ITA::make_temporary(
                    temporary,
                    std::format("{} {} {}", lhs, operator_to_string(op), rhs));
                instructions.emplace_back(last_instruction);
            },
        // there is exactly one temporary lvalue,
        // and at least one rvalue operand to use
        m::pattern | m::ds(m::_ >= 1, m::_ == 1) =
            [&] {
                binary_operands_balanced_temporary_stack(
                    operand_stack,
                    temporary_stack,
                    instructions,
                    op,
                    temporary);
            },
        m::pattern | m::_ =
            [&] {
                m::match(oss)(
                    // if there is only one operand on the stack, the next
                    // result was already evaluated and we take the temporary
                    // lvalues from the last two instructions as operands
                    m::pattern | (oss == 1) =
                        [&] {
                            binary_operands_unbalanced_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                        },

                    // empty rvalue operand stack, do nothing
                    m::pattern | (m::_ < 1) = [&] { return; },

                    // there are two or more operands on
                    // the primary rvalue stack, use them
                    m::pattern | m::_ =
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
                                auto operand_temp = ITA::make_temporary(
                                    temporary,
                                    std::format(
                                        "{} {} {}",
                                        lhs_name.first,
                                        operator_to_string(op),
                                        rhs_name.first));
                                type::RValue::LValue temp_lvalue =
                                    std::make_pair(
                                        std::get<1>(operand_temp),
                                        type::NULL_LITERAL);
                                operand_stack.emplace(
                                    rvalue_type_pointer_from_rvalue(
                                        temp_lvalue));
                                instructions.emplace_back(operand_temp);

                            } else {
                                temporary_stack.emplace(
                                    std::format(
                                        "{} {} {}",
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
ITA::Instructions rvalue_queue_to_temp_instructions(
    RValue_Queue_PTR& queue,
    int* temporary)
{
    using namespace detail;
    ITA::Instructions instructions{};
    if (queue->empty()) {
        return instructions;
    }
    detail::Temporary_Stack temporary_stack{};
    detail::Operand_Stack operand_stack{};
    int param_on_stack = 0;
    for (auto& item : *queue) {
        std::visit(
            util::overload{
                [&](type::Operator op) {
                    switch (op) {
                        // relational operators
                        case Operator::R_EQUAL:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);

                            break;
                        case Operator::R_NEQUAL:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::R_LT:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::R_GT:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::R_LE:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::R_GE:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::R_OR:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);

                            break;
                        case Operator::R_AND:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        // math binary operators
                        case Operator::B_SUBTRACT:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::B_ADD:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::B_MOD:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::B_MUL:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::B_DIV:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        // unary increment/decrement
                        case Operator::PRE_INC:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::POST_INC:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        case Operator::PRE_DEC:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::POST_DEC:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        // bit ops
                        case Operator::RSHIFT:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::OR:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::AND:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::LSHIFT:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::XOR:
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        case Operator::U_NOT:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::U_ONES_COMPLEMENT:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        // pointer operators
                        case Operator::U_SUBSCRIPT:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::U_INDIRECTION:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::U_ADDR_OF:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::U_MINUS:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;
                        case Operator::U_PLUS:
                            unary_operand_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            break;

                        // lvalue and address operators
                        case Operator::U_CALL:
                            function_call_operands_to_temporary_instructions(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                temporary,
                                &param_on_stack);
                            break;
                        case Operator::U_PUSH: {
                            if (temporary_stack.size() >= 1) {
                                auto rhs = temporary_stack.top();
                                temporary_stack.pop();
                                instructions.emplace_back(
                                    ITA::make_quadruple(
                                        ITA::Instruction::PUSH, rhs, ""));
                            } else {
                                if (operand_stack.size() < 1)
                                    return;
                                auto operand = operand_stack.top();
                                operand_stack.pop();
                                auto rhs =
                                    instruction_temporary_from_rvalue_operand(
                                        operand, temporary);
                                ITA::insert_instructions(
                                    instructions, rhs.second);
                                instructions.emplace_back(
                                    ITA::make_quadruple(
                                        ITA::Instruction::PUSH, rhs.first, ""));
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
                            binary_operands_to_temporary_stack(
                                operand_stack,
                                temporary_stack,
                                instructions,
                                op,
                                temporary);
                            instructions.emplace_back(
                                ITA::make_quadruple(
                                    ITA::Instruction::POP,
                                    std::to_string(
                                        type::LITERAL_TYPE.at("word").second),
                                    "",
                                    ""));
                            break;
                    }
                },
                [&](type::RValue::Type_Pointer& s) {
                    operand_stack.emplace(s);
                } },
            item);
    }
    return instructions;
}

/**
 * @brief RValue node to list of linear ir instructions
 */
// clang-format off
RValue_Instructions
rvalue_node_to_list_of_temp_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node const& node,
    util::AST_Node const& details,
    int* temporary)
{
    // clang-format on
    RValue_Parser parser{ details, symbols };
    auto list = make_rvalue_queue();
    std::vector<type::RValue::Type_Pointer> rvalues{};

    if (node.JSON_type() == util::AST_Node::Class::Array) {
        for (auto& expression : node.array_range()) {
            if (expression.JSON_type() == util::AST_Node::Class::Array) {
                for (auto& rvalue : expression.array_range()) {
                    auto expression_rvalue =
                        RValue_Parser::make_rvalue(rvalue, details, symbols);
                    rvalues.emplace_back(
                        type::rvalue_type_pointer_from_rvalue(
                            expression_rvalue.value));
                }
            } else {
                rvalues.emplace_back(
                    type::rvalue_type_pointer_from_rvalue(
                        RValue_Parser::make_rvalue(expression, details, symbols)
                            .value));
            }
        }
        rvalues_to_queue(rvalues, list);
    } else {
        auto rvalue_type_pointer = type::rvalue_type_pointer_from_rvalue(
            RValue_Parser::make_rvalue(node, details, symbols).value);
        rvalues_to_queue(rvalue_type_pointer, list);
    }
    auto instructions = rvalue_queue_to_temp_instructions(list, temporary);
    return std::make_pair(instructions, *list);
}

} // namespace ir

} // namespace credence