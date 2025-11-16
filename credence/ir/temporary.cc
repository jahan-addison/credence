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

#include <credence/ir/temporary.h>

#include <algorithm>             // for copy
#include <credence/expression.h> // for Expression_Parser
#include <credence/ir/ita.h>     // for ITA
#include <credence/operators.h>  // for Operator, operator_to_string
#include <credence/queue.h>      // for make_queue_from_expression_operands
#include <credence/symbol.h>     // for Symbol_Table
#include <credence/util.h>       // for AST_Node, overload
#include <credence/value.h>      // for Expression, make_value_type_pointer
#include <deque>                 // for deque, operator==, _Deque_iterator
#include <format>                // for format, format_string
#include <mapbox/eternal.hpp>    // for element, map
#include <matchit.h>             // for Ds, Meet, _, pattern, ds, match, Wil...
#include <memory>                // for shared_ptr, __shared_ptr_access, uni...
#include <simplejson.h>          // for JSON
#include <string>                // for basic_string, allocator, string, to_...
#include <string_view>           // for basic_string_view, operator<=>
#include <tuple>                 // for tuple, get
#include <utility>               // for pair, make_pair, cmp_equal
#include <variant>               // for variant, visit, monostate
#include <vector>                // for vector

/****************************************************************************
 *  A set of functions that construct temporary lvalues "_tX" that aid in
 *  breaking expressions into 3- or 4- tuples for linear instructions. The
 *  expression stack should be ordered by operator precedence.
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
instruction_temporary_from_expression_operand(Temporary::Operand& operand);

/**
 * @brief
 * Pop exactly 1 operand and 1 temporary
 * from each stack onto instruction tuple
 */
void Temporary::binary_operands_balanced_temporary_stack(type::Operator op)
{
    auto operand1 = operand_stack.top();
    auto rhs = temporary_stack.top();

    if (operand_stack.size() > 1) {
        operand_stack.pop();
    }
    temporary_stack.pop();

    auto lhs = instruction_temporary_from_expression_operand(operand1);
    instructions.insert(
        instructions.end(), lhs.second.begin(), lhs.second.end());
    auto temp_rhs = ITA::make_temporary(temporary_index, rhs);
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
            temporary_index,
            std::format(
                "{} {} {}",
                lhs.first,
                operator_to_string(op),
                std::get<1>(temp_rhs)));
        instructions.emplace_back(temp_lhs);
    }
}

/**
 * @brief Create and insert instructions from an expression operand
 *  See Expression in `value.h' for details.
 */
internal::value::Size Temporary::insert_and_create_temporary_from_operand(
    Operand& operand)
{
    auto inst_temp = instruction_temporary_from_expression_operand(operand);
    if (inst_temp.second.size() > 0) {
        ITA::insert_instructions(instructions, inst_temp.second);
        return std::make_pair(inst_temp.first, inst_temp.second.size());
    } else {
        return std::make_pair(
            internal::value::expression_type_to_string(*operand, false), 0);
    }
}

/**
 * @brief
 * There is only one operand on the stack, and no temporaries,
 * so use the lvalue from the last instruction for the LHS.
 */
void Temporary::binary_operands_unbalanced_temporary_stack(type::Operator op)
{
    using namespace matchit;
    auto rhs_lvalue =
        internal::value::expression_type_to_string(*operand_stack.top(), false);
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
                while (std::get<0>(last_lvalue) != ITA::Instruction::MOV and
                       last_index < instructions.size()) {
                    last_lvalue =
                        instructions[instructions.size() - last_index];
                    last_index++;
                }
                auto operand_temp = ITA::make_temporary(
                    temporary_index,
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
                    ITA::make_temporary(temporary_index,
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
 * Construct a temporary lvalue from a recursive expression
 *  See Expression in `value.h' for details.
 */
Temporary_Instructions Temporary::instruction_temporary_from_expression_operand(
    Operand& operand)
{
    namespace value = internal::value;
    ITA::Instructions instructions{};
    std::string temp_name{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](value::Expression::Pointer& s) {
                auto unwrap_type = value::make_value_type_pointer(s->value);
                auto pointer =
                    instruction_temporary_from_expression_operand(unwrap_type);
                ITA::insert_instructions(instructions, pointer.second);
                temp_name = pointer.first;
            },
            [&](value::Array&) {},
            [&](value::Literal&) {
                temp_name =
                    internal::value::expression_type_to_string(*operand, false);
            },
            [&](value::Expression::LValue&) {
                temp_name =
                    internal::value::expression_type_to_string(*operand, false);
            },
            [&](value::Expression::Unary& s) {
                auto op = s.first;
                auto rhs_expression = s.second;
                auto unwrap_rhs_type =
                    value::make_value_type_pointer(s.second->value);
                auto rhs = instruction_temporary_from_expression_operand(
                    unwrap_rhs_type);
                ITA::insert_instructions(instructions, rhs.second);
                auto temp = ITA::make_temporary(temporary_index, rhs.first);

                auto unary = ITA::make_temporary(
                    temporary_index,
                    std::format(
                        "{} {}", operator_to_string(op), std::get<1>(temp)));
                instructions.emplace_back(unary);
                temp_name = std::get<1>(unary);
            },
            [&](value::Expression::Relation& s) {
                auto op = s.first;
                if (s.second.size() == 2) {
                    auto unwrap_lhs_type =
                        value::make_value_type_pointer(s.second.at(0)->value);
                    auto unwrap_rhs_type =
                        value::make_value_type_pointer(s.second.at(1)->value);
                    auto lhs = instruction_temporary_from_expression_operand(
                        unwrap_lhs_type);
                    auto rhs = instruction_temporary_from_expression_operand(
                        unwrap_rhs_type);
                    // clang-format off
                    auto relation =
                        ITA::make_temporary(temporary_index,
                            std::format("{} {} {}",
                                lhs.first,
                                operator_to_string(op),
                                rhs.first));
                    // clang-format on
                    instructions.emplace_back(relation);
                    temp_name = std::get<1>(relation);
                }
            },
            [&](value::Expression::Function& s) {
                temp_name =
                    internal::value::expression_type_to_string(s.first, false);
            },
            [&](value::Expression::Symbol& s) {
                temp_name =
                    internal::value::expression_type_to_string(s.first, false);
            } },
        *operand);

    return std::make_pair(temp_name, instructions);
}

/**
 * @brief Construct temporary lvalues from assignment operator
 */
void Temporary::assignment_operands_to_temporary_stack()
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
                auto lhs =
                    instruction_temporary_from_expression_operand(lvalue);
                ITA::insert_instructions(instructions, lhs.second);
                instructions.emplace_back(
                    ITA::make_quadruple(ITA::Instruction::MOV, lhs.first, rhs));
            },
        m::pattern | ds(m::_ == 1, m::_ == 0) =
            [&] {
                auto lhs_expression =
                    internal::value::expression_type_to_string(
                        *operand_stack.top(), false);
                operand_stack.pop();
                if (instructions.size() > 1) {
                    auto last = instructions[instructions.size() - 1];
                    instructions.emplace_back(
                        ITA::make_quadruple(
                            ITA::Instruction::MOV,
                            lhs_expression,
                            std::get<1>(last)));
                }
            },
        m::pattern | m::ds(m::_ >= 2, m::_ == 0) =
            [&] {
                auto operand1 = operand_stack.top();
                operand_stack.pop();
                auto operand2 = operand_stack.top();
                operand_stack.pop();

                auto lhs =
                    instruction_temporary_from_expression_operand(operand2);
                auto rhs =
                    instruction_temporary_from_expression_operand(operand1);
                ITA::insert_instructions(instructions, lhs.second);
                ITA::insert_instructions(instructions, rhs.second);
                instructions.emplace_back(
                    ITA::make_quadruple(
                        ITA::Instruction::MOV, lhs.first, rhs.first));
            });
}

void Temporary::from_push_operands_to_temporary_instructions()
{
    if (temporary_stack.size() >= 1) {
        auto rhs = temporary_stack.top();
        temporary_stack.pop();
        instructions.emplace_back(
            ITA::make_quadruple(ITA::Instruction::PUSH, rhs, ""));
    } else {
        if (operand_stack.size() < 1)
            return;
        auto operand = operand_stack.top();
        operand_stack.pop();
        auto rhs = instruction_temporary_from_expression_operand(operand);
        ITA::insert_instructions(instructions, rhs.second);
        instructions.emplace_back(
            ITA::make_quadruple(ITA::Instruction::PUSH, rhs.first, ""));
    }
    parameters_size++;
}

/**
 * @brief Construct temporary lvalues from function call
 */
void Temporary::from_call_operands_to_temporary_instructions()
{
    auto op = type::Operator::U_CALL;
    if (temporary_stack.size() > 1) {
        auto rhs = temporary_stack.top();
        temporary_stack.pop();
        auto temp_rhs = ITA::make_temporary(temporary_index, rhs);
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
            auto temp_rhs = ITA::make_temporary(temporary_index, rhs);
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
        auto rhs = instruction_temporary_from_expression_operand(operand);
        ITA::insert_instructions(instructions, rhs.second);
        instructions.emplace_back(
            ITA::make_quadruple(ITA::Instruction::CALL, rhs.first));
    }
    if (parameters_size > 0)
        instructions.emplace_back(
            ITA::make_quadruple(
                ITA::Instruction::POP,
                std::to_string(
                    parameters_size *
                    internal::value::TYPE_LITERAL.at("word").second),
                "",
                ""));
    auto call_return = ITA::make_temporary(temporary_index, "RET");
    instructions.emplace_back(call_return);
    if (operand_stack.size() >= 1) {
        temporary_stack.emplace(std::get<1>(call_return));
    }
    parameters_size = 0;
}

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
void Temporary::unary_operand_to_temporary_stack(Operator op)
{
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
                    temporary_index,
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
                        ITA::make_temporary(temporary_index, operand1);
                    instructions.emplace_back(last_expression);
                    temporary_stack.emplace(std::get<1>(last_expression));
                }
                auto rhs =
                    instruction_temporary_from_expression_operand(operand1);
                ITA::insert_instructions(instructions, rhs.second);
                m::match(std::cmp_equal(tss, 0))(
                    m::pattern | true =
                        [&] {
                            // If the operand is an lvalue, use it, otherwise
                            // create a temporary and assign it the unary
                            // expression
                            if (internal::value::is_value_type_pointer_type(
                                    operand1, "lvalue") and
                                is_in_place_unary_operator(op)) {
                                auto unary = ITA::make_quadruple(
                                    ITA::Instruction::MOV,
                                    internal::value::expression_type_to_string(
                                        *operand1, false),
                                    operator_to_string(op),
                                    rhs.first);
                                instructions.emplace_back(unary);
                                operand_stack.emplace(operand1);
                            } else {
                                auto operand_temp = ITA::make_temporary(
                                    temporary_index,
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
                                temporary_index,
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
void Temporary::binary_operands_to_temporary_stack(Operator op)
{
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
                    temporary_index,
                    std::format("{} {} {}", lhs, operator_to_string(op), rhs));
                instructions.emplace_back(last_instruction);
            },
        // there is exactly one temporary lvalue,
        // and at least one expression operand to use
        m::pattern | m::ds(m::_ >= 1, m::_ == 1) =
            [&] { binary_operands_balanced_temporary_stack(op); },
        m::pattern | m::_ =
            [&] {
                m::match(oss)(
                    // if there is only one operand on the stack, the next
                    // result was already evaluated and we take the temporary
                    // lvalues from the last two instructions as operands
                    m::pattern | (oss == 1) =
                        [&] { binary_operands_unbalanced_temporary_stack(op); },

                    // empty expression operand stack, do nothing
                    m::pattern | (m::_ < 1) = [&] { return; },

                    // there are two or more operands on
                    // the primary expression stack, use them
                    m::pattern | m::_ =
                        [&] {
                            auto operand1 = operand_stack.top();
                            operand_stack.pop();
                            auto operand2 = operand_stack.top();
                            operand_stack.pop();
                            auto rhs_name =
                                insert_and_create_temporary_from_operand(
                                    operand1);
                            auto lhs_name =
                                insert_and_create_temporary_from_operand(
                                    operand2);

                            if (lhs_name.second == 0 and
                                rhs_name.second == 0 and tss == 0) {
                                // if the lhs is an lvalue, use the
                                // temporary from the last instruction
                                auto operand_temp = ITA::make_temporary(
                                    temporary_index,
                                    std::format(
                                        "{} {} {}",
                                        lhs_name.first,
                                        operator_to_string(op),
                                        rhs_name.first));
                                internal::value::Expression::LValue
                                    temp_lvalue = std::make_pair(
                                        std::get<1>(operand_temp),
                                        internal::value::Expression::
                                            NULL_LITERAL);
                                operand_stack.emplace(
                                    internal::value::make_value_type_pointer(
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

} // namespace detail

/**
 * @brief
 * Construct a set of ITA instructions from an expression queue.
 */
ITA::Instructions queue_to_temporary_instructions(
    queue::Queue& queue,
    int* index)
{
    if (queue.empty()) {
        return ITA::Instructions{};
    }
    auto temporary = detail::Temporary{ index };
    for (auto& item : queue) {
        std::visit(
            util::overload{
                [&](type::Operator op) {
                    switch (op) {
                        // relational operators
                        case Operator::R_EQUAL:
                        case Operator::R_NEQUAL:
                        case Operator::R_LT:
                        case Operator::R_GT:
                        case Operator::R_LE:
                        case Operator::R_GE:
                        case Operator::R_OR:
                        case Operator::R_AND:
                        // arithmetic binary operators
                        case Operator::B_SUBTRACT:
                        case Operator::B_ADD:
                        case Operator::B_MOD:
                        case Operator::B_MUL:
                        case Operator::B_DIV:
                            temporary.binary_operands_to_temporary_stack(op);
                            break;
                        // unary increment/decrement
                        case Operator::PRE_INC:
                        case Operator::POST_INC:
                        case Operator::PRE_DEC:
                        case Operator::POST_DEC:
                            temporary.unary_operand_to_temporary_stack(op);
                            break;
                        // bitwise operators
                        case Operator::RSHIFT:
                        case Operator::OR:
                        case Operator::AND:
                        case Operator::LSHIFT:
                        case Operator::XOR:
                            temporary.binary_operands_to_temporary_stack(op);
                            break;
                        case Operator::U_NOT:
                        case Operator::U_ONES_COMPLEMENT:
                            temporary.unary_operand_to_temporary_stack(op);
                            break;
                        // pointer operators
                        case Operator::U_SUBSCRIPT:
                        case Operator::U_INDIRECTION:
                        case Operator::U_ADDR_OF:
                        // unary +/-
                        case Operator::U_MINUS:
                        case Operator::U_PLUS:
                            temporary.unary_operand_to_temporary_stack(op);
                            break;
                        // assignment and address operators
                        case Operator::U_CALL:
                            temporary
                                .from_call_operands_to_temporary_instructions();
                            break;
                        case Operator::U_PUSH:
                            temporary
                                .from_push_operands_to_temporary_instructions();
                            break;
                        case Operator::B_ASSIGN:
                            temporary.assignment_operands_to_temporary_stack();

                            break;
                        case Operator::B_TERNARY:
                            temporary.binary_operands_to_temporary_stack(op);
                            temporary.instructions.emplace_back(
                                ITA::make_quadruple(
                                    ITA::Instruction::POP,
                                    std::to_string(
                                        internal::value::TYPE_LITERAL.at("word")
                                            .second),
                                    "",
                                    ""));
                            break;
                    }
                },
                [&](detail::Temporary::Operand& s) {
                    temporary.operand_stack.emplace(s);
                } },
            item);
    }
    return temporary.instructions;
}

/**
 * @brief Expression node to list of ITA instructions
 */
Expression_Instructions expression_node_to_temporary_instructions(
    Symbol_Table<> const& symbols,
    util::AST_Node const& node,
    util::AST_Node const& details,
    int* temporary_index)
{
    detail::Temporary::Operands operands{};
    Expression_Parser parser{ details, symbols };

    if (node.JSON_type() == util::AST_Node::Class::Array) {
        for (auto& expression : node.array_range()) {
            if (expression.JSON_type() == util::AST_Node::Class::Array) {
                for (auto& expr : expression.array_range()) {
                    auto expression =
                        Expression_Parser::parse(expr, details, symbols);
                    operands.emplace_back(
                        internal::value::make_value_type_pointer(
                            expression.value));
                }
            } else {
                operands.emplace_back(
                    internal::value::make_value_type_pointer(
                        Expression_Parser::parse(expression, details, symbols)
                            .value));
            }
        }
        auto queue = queue::make_queue_from_expression_operands(operands);
        auto instructions =
            queue_to_temporary_instructions(*queue, temporary_index);
        return std::make_pair(instructions, *queue);

    } else {
        auto type_pointer = internal::value::make_value_type_pointer(
            Expression_Parser::parse(node, details, symbols).value);
        auto queue = queue::make_queue_from_expression_operands(type_pointer);
        auto instructions =
            queue_to_temporary_instructions(*queue, temporary_index);
        return std::make_pair(instructions, *queue);
    }
}

} // namespace ir

} // namespace credence