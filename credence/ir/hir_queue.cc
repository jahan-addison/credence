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

#include <credence/ir/hir_queue.h>

#include <credence/frontend/hir/hir.h> // for Node, Type, Unit, Span, Node_...
#include <credence/ir/operand.h>       // for TYPE_LITERAL, Operand, Literal
#include <credence/ir/operators.h>     // for Operator
#include <credence/ir/queue.h>         // for Queue
#include <cstddef>                     // for size_t
#include <cstdint>                     // for uint32_t
#include <fmt/format.h>                // for format
#include <map>                         // for map
#include <matchit.h>                   // for PatternPair, pattern, Pattern...
#include <string>                      // for basic_string, string, char_tr...
#include <string_view>                 // for basic_string_view, operator==
#include <utility>                     // for pair, move
#include <vector>                      // for vector

/****************************************************************************
 *
 * HIR expression to instruction queue
 *
 * A walk of the subtree range, which is already in operand stack order. See
 * hir_queue.h.
 *
 ****************************************************************************/

namespace credence::ir {

namespace m = matchit;

namespace {

using operand::Operand;
using operand::TYPE_LITERAL;
using IR_Operator = operators::Operator;

/**
 * @brief The IR operator of a binary HIR operator
 */
IR_Operator binary_operator_of(frontend::ast::Operator op)
{
    using Op = frontend::ast::Operator;
    switch (op) {
        case Op::Add:
            return IR_Operator::B_ADD;
        case Op::Sub:
            return IR_Operator::B_SUBTRACT;
        case Op::Mul:
            return IR_Operator::B_MUL;
        case Op::Div:
            return IR_Operator::B_DIV;
        case Op::Mod:
            return IR_Operator::B_MOD;
        case Op::Eq:
            return IR_Operator::R_EQUAL;
        case Op::Neq:
            return IR_Operator::R_NEQUAL;
        case Op::Lt:
            return IR_Operator::R_LT;
        case Op::Lte:
            return IR_Operator::R_LE;
        case Op::Gt:
            return IR_Operator::R_GT;
        case Op::Gte:
            return IR_Operator::R_GE;
        case Op::And:
            return IR_Operator::R_AND;
        case Op::Or:
            return IR_Operator::R_OR;
        case Op::Bit_And:
            return IR_Operator::AND;
        case Op::Bit_Or:
            return IR_Operator::OR;
        case Op::Xor:
            return IR_Operator::XOR;
        case Op::Lshift:
            return IR_Operator::LSHIFT;
        case Op::Rshift:
            return IR_Operator::RSHIFT;
        default:
            return IR_Operator::B_ADD;
    }
}

/**
 * @brief The IR operator of a unary HIR operator
 */
IR_Operator unary_operator_of(frontend::ast::Operator op)
{
    using Op = frontend::ast::Operator;
    switch (op) {
        case Op::Minus:
            return IR_Operator::U_MINUS;
        case Op::Plus:
            return IR_Operator::U_PLUS;
        case Op::Not:
            return IR_Operator::U_NOT;
        case Op::Ones_Complement:
            return IR_Operator::U_ONES_COMPLEMENT;
        case Op::Address_Of:
            return IR_Operator::U_ADDR_OF;
        case Op::Indirection:
            return IR_Operator::U_INDIRECTION;
        default:
            return IR_Operator::U_PLUS;
    }
}

/**
 * @brief An operand holding a literal value
 */
Operand::Type_Pointer literal_operand(operand::detail::Literal value,
    operand::Size size)
{
    return operand::make_value_type_pointer(Operand::Type{
        operand::Literal{ std::move(value), std::move(size) }
    });
}

/**
 * @brief An operand naming an lvalue
 */
Operand::Type_Pointer lvalue_operand(std::string name)
{
    return operand::make_value_type_pointer(
        Operand::Type{ operand::make_lvalue(std::move(name)) });
}

} // namespace

namespace detail {

/**
 * @brief The bytes of a string literal
 *
 * Only the quotes around the lexeme are dropped. An escape such as "\\n"
 * stays the two characters it was written as, since the assembler is what
 * resolves it, and its width is what the program reserves storage for.
 */
std::string string_value_of(frontend::ast::String_Index handle,
    hir::Unit const& unit)
{
    auto text = std::string{ unit.string(handle) };
    if (text.size() >= 2 and text.front() == '"')
        return text.substr(1, text.size() - 2);
    return text;
}

/**
 * @brief The name a dereference addresses, as "*base"
 */
std::string Builder::dereference_name(hir::Node_Index index) const
{
    auto operand = unit_.nodes[index].data.unary;
    if (operand != hir::null_node_index and
        unit_.nodes[operand].type == hir::Type::Symbol_Ref)
        return fmt::format(
            "*{}", unit_.symbol_name(unit_.nodes[operand].data.symbol));
    return "*";
}

/**
 * @brief The name a subscript addresses, as "base[index]"
 *
 * A constant index is written as its value and anything else as the name
 * of the expression that produces it, which is what the object table and
 * the backends read the offset back out of.
 */
std::string Builder::subscript_name(hir::Node_Index index) const
{
    auto const& node = unit_.nodes[index];
    auto base = node.data.binary.lhs;
    auto subscript = node.data.binary.rhs;

    auto base_name =
        unit_.nodes[base].type == hir::Type::Symbol_Ref
            ? std::string{ unit_.symbol_name(unit_.nodes[base].data.symbol) }
            : std::string{};

    auto const& offset = unit_.nodes[subscript];
    if (offset.type == hir::Type::Integer)
        return fmt::format("{}[{}]", base_name, offset.data.integer);
    if (offset.type == hir::Type::Symbol_Ref)
        return fmt::format(
            "{}[{}]", base_name, unit_.symbol_name(offset.data.symbol));
    return fmt::format("{}[]", base_name);
}

void Builder::visit_children(hir::Node_Index index)
{
    auto const& node = unit_.nodes[index];
    m::match(hir::payload_of(node.type))(
        m::pattern | hir::Payload::Binary =
            [&] {
                visit(node.data.binary.lhs);
                visit(node.data.binary.rhs);
            },
        m::pattern | hir::Payload::Span =
            [&] {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i)
                    visit(unit_.extra[span.start + i]);
            },
        m::pattern | hir::Payload::Unary =
            [&] {
                if (node.data.unary != hir::null_node_index)
                    visit(node.data.unary);
            },
        m::pattern | m::_ = [] {});
}

/**
 * @brief Append one expression, operands before the operator that joins them
 */
void Builder::visit(hir::Node_Index index)
{
    if (index == hir::null_node_index)
        return;

    auto const& node = unit_.nodes[index];

    m::match(node.type)(
        m::pattern | hir::Type::Integer =
            [&] {
                queue_.emplace_back(
                    literal_operand(static_cast<int>(node.data.integer),
                        TYPE_LITERAL.at("int")));
            },
        m::pattern | hir::Type::Double =
            [&] {
                queue_.emplace_back(
                    literal_operand(node.data.real, TYPE_LITERAL.at("double")));
            },

        m::pattern | hir::Type::Float =
            [&] {
                queue_.emplace_back(
                    literal_operand(static_cast<float>(node.data.real),
                        TYPE_LITERAL.at("float")));
            },
        m::pattern | hir::Type::String =
            [&] {
                // the interned text is the lexeme, so the escapes are
                // resolved and the quotes dropped before it becomes storage
                auto text = string_value_of(node.data.string);
                auto size = text.size();
                queue_.emplace_back(
                    literal_operand(text, operand::Size{ "string", size }));
            },
        m::pattern | hir::Type::Char =
            [&] {
                // a character constant is one byte, and the operand keeps
                // it as a char and not as text
                auto text = string_value_of(node.data.string);
                queue_.emplace_back(
                    literal_operand(text.empty() ? char{} : text.front(),
                        TYPE_LITERAL.at("char")));
            },

        m::pattern | hir::Type::Bool =
            [&] {
                // a bool is carried as one or zero under a bool type, which
                // is what the instruction operand prints
                auto text = unit_.string(node.data.string);
                queue_.emplace_back(literal_operand(
                    text == "true" ? 1 : 0, TYPE_LITERAL.at("bool")));
            },
        m::pattern | m::or_(hir::Type::Symbol_Ref, hir::Type::Declaration) =
            [&] {
                queue_.emplace_back(lvalue_operand(
                    std::string{ unit_.symbol_name(node.data.symbol) }));
            },
        m::pattern | hir::Type::Subscript =
            [&] {
                // a subscript names one place in storage and not an
                // operation over two operands, so it reaches the
                // instruction stream as a single lvalue
                queue_.emplace_back(lvalue_operand(subscript_name(index)));
            },
        m::pattern | hir::Type::Binary =
            [&] {
                visit_children(index);
                queue_.emplace_back(binary_operator_of(node.op));
            },
        m::pattern | hir::Type::Assign =
            [&] {
                visit_children(index);
                queue_.emplace_back(IR_Operator::B_ASSIGN);
            },
        m::pattern | hir::Type::Unary =
            [&] {
                visit_children(index);
                queue_.emplace_back(unary_operator_of(node.op));
            },
        m::pattern | hir::Type::Address_Of =
            [&] {
                visit_children(index);
                queue_.emplace_back(IR_Operator::U_ADDR_OF);
            },
        m::pattern | hir::Type::Dereference =
            [&] {
                // a dereference names one place in storage, the same way a
                // subscript does, so it reaches the instruction stream as a
                // single lvalue and not an operation over an operand
                queue_.emplace_back(lvalue_operand(dereference_name(index)));
            },
        m::pattern | hir::Type::Pre_Inc_Dec =
            [&] {
                visit_children(index);
                queue_.emplace_back(node.op == frontend::ast::Operator::Inc
                                        ? IR_Operator::PRE_INC
                                        : IR_Operator::PRE_DEC);
            },
        m::pattern | hir::Type::Post_Inc_Dec =
            [&] {
                visit_children(index);
                queue_.emplace_back(node.op == frontend::ast::Operator::Inc
                                        ? IR_Operator::POST_INC
                                        : IR_Operator::POST_DEC);
            },
        m::pattern | hir::Type::Call =
            [&] {
                // the callee is named first, then each argument is given a
                // parameter lvalue and assigned into it. The lvalues are
                // named again, and the pushes drain in reverse argument
                // order before the call itself.
                visit(node.data.binary.lhs);

                std::vector<std::string> parameters{};
                auto arguments = node.data.binary.rhs;
                if (arguments != hir::null_node_index) {
                    auto span = unit_.nodes[arguments].data.span;
                    for (std::uint32_t i = 0; i < span.count; ++i) {
                        auto name = fmt::format(
                            "_p{}_{}", ++(*parameter_), ++(*identifier_));
                        queue_.emplace_back(lvalue_operand(name));
                        visit(unit_.extra[span.start + i]);
                        queue_.emplace_back(IR_Operator::B_ASSIGN);
                        parameters.emplace_back(std::move(name));
                    }
                }

                for (auto const& name : parameters)
                    queue_.emplace_back(lvalue_operand(name));
                for (std::size_t i = 0; i < parameters.size(); ++i)
                    queue_.emplace_back(IR_Operator::U_PUSH);

                queue_.emplace_back(IR_Operator::U_CALL);
            },
        m::pattern | hir::Type::Ternary =
            [&] {
                auto span = node.data.span;
                for (std::uint32_t i = 0; i < span.count; ++i)
                    visit(unit_.extra[span.start + i]);
                queue_.emplace_back(IR_Operator::B_TERNARY);
            },
        // a statement in an expression position holds its children
        m::pattern | m::_ = [&] { visit_children(index); });
}

} // namespace detail

Queue queue_from_hir(hir::Unit const& unit,
    hir::Node_Index index,
    int* parameter,
    int* identifier)
{
    Queue queue{};
    detail::Builder{ unit, queue, parameter, identifier }.visit(index);
    return queue;
}

} // namespace credence::ir
