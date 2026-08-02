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

#include <credence/frontend/hir/check.h>

#include <fmt/format.h> // for format
#include <matchit.h>    // for match, pattern, or_
#include <utility>      // for move

/****************************************************************************
 *
 * Type checker
 *
 * Assigns a type to every HIR node and reports the uses a declaration does
 * not allow. Nodes are in post-order, so the pass is a forward loop and not
 * a walk - every operand of a node has already been given a type when the
 * node itself is reached, and the result is written into the types array
 * beside it.
 *
 * What it rejects:
 *
 *    - arithmetic on a type that cannot take part in it
 *    - a bitwise or shift expression on a floating operand
 *    - subscripting something that is neither a vector nor a pointer
 *    - dereferencing something that does not address memory
 *    - taking the address of a value that has no storage
 *    - calling a name that is not a function
 *    - a subscript on a vector whose constant index is out of range
 *    - a goto naming a label that no statement defines
 *
 ****************************************************************************/

namespace credence::frontend::hir {

namespace m = matchit;

namespace {

using ast::Operator;

} // namespace

namespace detail {

void Checker::error(Node_Index index, std::string message)
{
    auto const& meta = unit_.metadata[index];
    diagnostics_.push_back(
        Diagnostic{ std::move(message), meta.line, meta.column });
}

std::vector<Diagnostic> Checker::run()
{
    for (Node_Index index = 0; index < unit_.nodes.size(); ++index)
        check_node(index);

    check_labels();
    return std::move(diagnostics_);
}

/**
 * @brief Confirm every goto names a label a statement defines
 *
 * Lowering declares a label the first time it sees one, whether that was
 * the definition or a goto reaching forward to it, so an undefined target
 * is only visible once the whole unit is in place.
 */
void Checker::check_labels()
{
    std::vector<bool> defined(unit_.symbol_table.symbols().size(), false);

    for (auto const& node : unit_.nodes) {
        if (node.type == Type::Label)
            defined[node.data.symbol] = true;
    }

    for (Node_Index index = 0; index < unit_.nodes.size(); ++index) {
        auto const& node = unit_.nodes[index];
        if (node.type != Type::Goto)
            continue;
        if (!defined[node.data.symbol])
            error(index,
                fmt::format("goto names the label '{}', which is never defined",
                    unit_.symbol_name(node.data.symbol)));
    }
}

void Checker::check_node(Node_Index index)
{
    auto const& node = unit_.nodes[index];

    m::match(node.type)(
        m::pattern | Type::Integer = [&] { unit_.types[index] = type_int; },
        m::pattern | Type::Double = [&] { unit_.types[index] = type_double; },
        m::pattern | Type::Float = [&] { unit_.types[index] = type_float; },
        m::pattern | Type::Bool = [&] { unit_.types[index] = type_bool; },
        m::pattern | Type::String = [&] { unit_.types[index] = type_string; },
        m::pattern | Type::Char = [&] { unit_.types[index] = type_char; },

        m::pattern | m::or_(Type::Symbol_Ref, Type::Declaration) =
            [&] {
                unit_.types[index] =
                    unit_.symbol_table.at(node.data.symbol).type;
            },

        m::pattern | Type::Binary = [&] { check_binary(index); },
        m::pattern | Type::Assign = [&] { check_assign(index); },
        m::pattern | Type::Subscript = [&] { check_subscript(index); },
        m::pattern | Type::Call = [&] { check_call(index); },

        m::pattern | Type::Address_Of = [&] { check_address_of(index); },
        m::pattern | Type::Dereference = [&] { check_dereference(index); },
        m::pattern | Type::Unary = [&] { check_unary(index); },

        m::pattern | m::or_(Type::Pre_Inc_Dec, Type::Post_Inc_Dec) =
            [&] {
                auto operand = node.data.unary;
                if (!is_lvalue(operand)) {
                    error(index, "an increment of a value with no storage");
                    return;
                }
                unit_.types[index] = type_of(operand);
            },

        m::pattern | Type::Ternary = [&] { check_ternary(index); },

        // statements and definitions carry no value
        m::pattern | m::_ = [&] { unit_.types[index] = type_null; });
}

/**
 * @brief Type the address of an lvalue
 */
void Checker::check_address_of(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto operand = node.data.unary;

    if (!is_lvalue(operand)) {
        error(index, "the address of a value with no storage");
        return;
    }

    // a string is itself a pointer, so the address of a character inside
    // one is not a value the program may hold
    if (unit_.nodes[operand].type == Type::Subscript) {
        auto base = unit_.nodes[operand].data.binary.lhs;
        if (unit_.type_table.kind_of(type_of(base)) == Type_Kind::String) {
            error(index,
                "the address of a character inside a string, which is "
                "already a pointer");
            return;
        }
    }

    unit_.types[index] = unit_.type_table.pointer_to(type_of(operand));
}

/**
 * @brief Type a dereference of something that addresses memory
 */
void Checker::check_dereference(Node_Index index)
{
    auto operand = unit_.nodes[index].data.unary;
    auto operand_type = type_of(operand);

    if (!unit_.type_table.is_address(operand_type)) {
        error(index,
            fmt::format("a dereference of '{}', which does not address memory",
                type_to_string(unit_.type_table, operand_type)));
        return;
    }

    unit_.types[index] = unit_.type_table.at(operand_type).element;
}

/**
 * @brief Type a unary expression against what its operator accepts
 */
void Checker::check_unary(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto operand_type = type_of(node.data.unary);

    auto bitwise =
        node.op == Operator::Ones_Complement or node.op == Operator::Not;

    if (bitwise and !unit_.type_table.is_integral(operand_type)) {
        error(index,
            fmt::format("'{}' is not an integral type for a bitwise unary "
                        "expression",
                type_to_string(unit_.type_table, operand_type)));
        return;
    }
    if (!bitwise and !unit_.type_table.is_numeric(operand_type)) {
        error(index,
            fmt::format("'{}' is not a numeric type for a unary expression",
                type_to_string(unit_.type_table, operand_type)));
        return;
    }

    unit_.types[index] = operand_type;
}

/**
 * @brief Type a ternary against the branches it may yield
 */
void Checker::check_ternary(Node_Index index)
{
    auto span = unit_.nodes[index].data.span;
    if (span.count < 3)
        return;

    auto then_type = type_of(unit_.extra[span.start + 1]);
    auto else_type = type_of(unit_.extra[span.start + 2]);
    auto common = unit_.type_table.common_type(then_type, else_type);

    if (common == null_type_index) {
        error(index,
            fmt::format("the branches of a ternary have the unrelated types "
                        "'{}' and '{}'",
                type_to_string(unit_.type_table, then_type),
                type_to_string(unit_.type_table, else_type)));
        return;
    }

    unit_.types[index] = common;
}

void Checker::check_binary(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto lhs = type_of(node.data.binary.lhs);
    auto rhs = type_of(node.data.binary.rhs);

    auto is_bitwise = node.op == Operator::Bit_And or
                      node.op == Operator::Bit_Or or node.op == Operator::Xor or
                      node.op == Operator::Lshift or
                      node.op == Operator::Rshift or node.op == Operator::Mod;

    if (is_bitwise) {
        if (!unit_.type_table.is_integral(lhs) or
            !unit_.type_table.is_integral(rhs)) {
            error(index,
                fmt::format("'{}' expects integral operands, and was given "
                            "'{}' and '{}'",
                    ast::operator_to_string(node.op),
                    type_to_string(unit_.type_table, lhs),
                    type_to_string(unit_.type_table, rhs)));
            return;
        }
    }

    auto common = unit_.type_table.common_type(lhs, rhs);
    if (common == null_type_index) {
        error(index,
            fmt::format("'{}' cannot combine '{}' and '{}'",
                ast::operator_to_string(node.op),
                type_to_string(unit_.type_table, lhs),
                type_to_string(unit_.type_table, rhs)));
        return;
    }

    // a comparison yields a bool and not the type of its operands
    switch (node.op) {
        case Operator::Eq:
        case Operator::Neq:
        case Operator::Lt:
        case Operator::Lte:
        case Operator::Gt:
        case Operator::Gte:
        case Operator::And:
        case Operator::Or:
            unit_.types[index] = type_bool;
            return;
        default:
            unit_.types[index] = common;
            return;
    }
}

/**
 * @brief Reject a pointer assignment the language does not allow
 *
 * Only useful once the target is known to address memory, which for a
 * name declared with auto is after it has been given its first value.
 */
void Checker::check_pointer_assign(Node_Index index,
    Node_Index target,
    Node_Index value)
{
    auto lhs = type_of(target);
    if (!unit_.type_table.is_address(lhs))
        return;

    auto rhs = type_of(value);
    if (rhs == null_type_index)
        return;

    if (!unit_.type_table.is_address(rhs) and
        !unit_.type_table.is_integral(rhs)) {
        error(index,
            fmt::format("'{}' is not an address and cannot be assigned to a "
                        "pointer",
                type_to_string(unit_.type_table, rhs)));
    }
}

void Checker::check_assign(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto target = node.data.binary.lhs;

    if (!is_lvalue(target)) {
        error(index, "an assignment to a value with no storage");
        return;
    }

    check_pointer_assign(index, target, node.data.binary.rhs);

    auto lhs = type_of(target);
    auto rhs = type_of(node.data.binary.rhs);

    // a name declared with auto takes the type of what it is first given
    if (unit_.nodes[target].type == Type::Symbol_Ref) {
        auto symbol = unit_.nodes[target].data.symbol;
        auto& declared = unit_.symbol_table.at(symbol);
        if (declared.type == type_word and rhs != null_type_index and
            declared.storage == Storage::Auto) {
            // a vector decays where it is read, so the name takes a
            // pointer to its element and not the vector itself
            auto inferred = rhs;
            if (unit_.type_table.kind_of(rhs) == Type_Kind::Vector)
                inferred = unit_.type_table.pointer_to(
                    unit_.type_table.at(rhs).element);
            declared.type = inferred;
            unit_.types[target] = inferred;
            lhs = inferred;
        }
    }

    if (lhs != null_type_index and rhs != null_type_index and
        unit_.type_table.common_type(lhs, rhs) == null_type_index) {
        error(index,
            fmt::format("'{}' cannot be assigned to '{}'",
                type_to_string(unit_.type_table, rhs),
                type_to_string(unit_.type_table, lhs)));
        return;
    }

    unit_.types[index] = lhs;
}

void Checker::check_subscript(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto base = node.data.binary.lhs;
    auto subscript = node.data.binary.rhs;

    auto base_type = type_of(base);
    if (!unit_.type_table.is_address(base_type)) {
        error(index,
            fmt::format("'{}' is not a vector or a pointer and cannot be "
                        "subscripted",
                type_to_string(unit_.type_table, base_type)));
        return;
    }

    // any word wide value may index memory, including one holding an
    // address, so only a value that is neither is rejected
    auto subscript_type = type_of(subscript);
    if (!unit_.type_table.is_integral(subscript_type) and
        !unit_.type_table.is_address(subscript_type)) {
        error(index, "a subscript must be an integral expression");
        return;
    }

    // a constant subscript on a declared vector is checked against its
    // length, which is the one bound available before the program runs
    if (unit_.nodes[subscript].type == Type::Integer and
        unit_.nodes[base].type == Type::Symbol_Ref) {
        auto const& declared =
            unit_.symbol_table.at(unit_.nodes[base].data.symbol);
        auto offset = unit_.nodes[subscript].data.integer;
        if (declared.storage == Storage::Vector and declared.count > 0 and
            (offset < 0 or
                static_cast<std::uint32_t>(offset) >= declared.count)) {
            error(index,
                fmt::format("the subscript {} is outside '{}', which holds "
                            "{} elements",
                    offset,
                    unit_.symbol_name(unit_.nodes[base].data.symbol),
                    declared.count));
            return;
        }
    }

    unit_.types[index] = unit_.type_table.at(base_type).element;
}

void Checker::check_call(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto callee = node.data.binary.lhs;

    if (unit_.nodes[callee].type == Type::Symbol_Ref) {
        auto const& declared =
            unit_.symbol_table.at(unit_.nodes[callee].data.symbol);
        // a name brought in with extrn is a function the linker resolves,
        // so only a definition in this unit can be contradicted here
        if (declared.storage == Storage::Vector) {
            error(index,
                fmt::format("'{}' is a vector and cannot be called",
                    unit_.symbol_name(unit_.nodes[callee].data.symbol)));
            return;
        }
    }

    unit_.types[index] = type_word;
}

} // namespace detail

std::vector<Diagnostic> check(Unit& unit)
{
    detail::Checker checker{ unit };
    return checker.run();
}

} // namespace credence::frontend::hir
