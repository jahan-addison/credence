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

#include "checker.h"

#include "../literal.h"     // for Literal
#include "../operators.h"   // for Operator
#include "hir.h"            // for Data_Kind, data_kind_to_string
#include "types.h"          // for LValue, RValue, Type
#include <algorithm>        // for find
#include <credence/error.h> // for throw_compiletime_error
#include <credence/types.h> // for from_decay_offset, from_lvalue...
#include <credence/util.h>  // for contains, is_numeric, overload
#include <easyjson.h>       // for JSON
#include <fmt/format.h>     // for format
#include <memory>           // for shared_ptr
#include <string>           // for basic_string, stoul, string
#include <utility>          // for pair
#include <variant>          // for get, holds_alternative, visit

/****************************************************************************
 *  Type Checker
 *
 *  Type checker on the HIR. Each expression evaluates to the storage and
 *  type of its value in a stack frame, and an assignment between storage
 *  or types that do not agree is a compile-time error.
 *
 *  Example type checking:
 *
 *  main() {
 *    auto x, *p;
 *    x = 5;
 *    p = &x;
 *    p = x;
 *  }
 *
 *  Becomes:
 *
 *    x  -> scalar int
 *    p  -> pointer to int
 *    error: invalid pointer assignment, right-hand-side 'x' is not a pointer
 *
 ****************************************************************************/

namespace credence::language::hir::checking {

using type::Operator;

namespace {

/**
 * @brief The innermost expression of a chain of HIR pointers
 */
Data_Kind::Kind const& unwrap(Data_Kind::Kind const& expression)
{
    if (std::holds_alternative<Data_Kind::Pointer>(expression))
        return unwrap(std::get<Data_Kind::Pointer>(expression)->value);
    return expression;
}

} // namespace

/**
 * @brief The human-readable symbol of an expression for error messages
 */
RValue get_symbol_from_expression(Data_Kind::Kind const& expression)
{
    auto const& kind = unwrap(expression);
    if (std::holds_alternative<Data_Kind::LValue>(kind))
        return std::get<Data_Kind::LValue>(kind).first;
    if (std::holds_alternative<literal::Literal>(kind))
        return credence::type::get_value_from_rvalue_data_type(
            credence::type::get_rvalue_datatype_from_string(
                data_kind_to_string(kind, false)));
    if (std::holds_alternative<Data_Kind::Unary>(kind)) {
        auto const& [op, operand] = std::get<Data_Kind::Unary>(kind);
        return fmt::format("{}{}",
            util::str_trim_ws(type::operator_to_string(op)),
            get_symbol_from_expression(operand->value));
    }
    return data_kind_to_string(kind, false);
}

/**
 * @brief Whether an lvalue is a tag of a union, e.g. `m.ab` or `data_t.ab`
 */
bool is_union_member(LValue const& lvalue)
{
    return util::contains(lvalue, ".") and not lvalue.starts_with("*");
}

/**
 * @brief The value of a literal and its constant
 *
 * A string literal is a pointer
 */
Value get_value_from_literal(literal::Literal const& literal)
{
    auto const& type = literal.second.first;
    if (type == "string")
        return Value{ Storage::POINTER, type };
    auto constant = credence::type::get_value_from_rvalue_data_type(
        credence::type::get_rvalue_datatype_from_string(
            literal::literal_to_string(literal)));
    return Value{ Storage::SCALAR, type, {}, constant };
}

/**
 * @brief Type equality of a left-hand-side and right-hand-side type
 *
 * An unassigned "null" type or an untyped "word" agrees with every type
 */
bool Type_Checker::is_compatible(Type const& lhs, Type const& rhs)
{
    return lhs == "null" or rhs == "null" or lhs == "word" or rhs == "word" or
           lhs == rhs;
}

/**
 * @brief Check that a type is a numeric type
 */
bool Type_Checker::is_numeric(Type const& type)
{
    return type == "word" or
           std::ranges::find(credence::type::integral_unary_types, type) !=
               credence::type::integral_unary_types.end();
}

/**
 * @brief Evaluate the storage and type of an HIR expression
 */
Value Type_Checker::from_expression(Data_Kind::Kind const& expression)
{
    Value value{};
    // clang-format off
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](Data_Kind::Pointer const& pointer) {
                value = from_expression(pointer->value);
            },
            [&](literal::Array const&) {
                value = Value{ Storage::SCALAR, "word" };
            },
            [&](literal::Literal const& literal) {
                value = from_literal(literal);
            },
            [&](Data_Kind::Symbol const& assignment) {
                value = from_assignment_expression(assignment);
            },
            [&](Data_Kind::Unary const& unary) {
                value = from_unary_expression(unary);
            },
            [&](Data_Kind::Relation const& relation) {
                value = from_relation_expression(relation);
            },
            [&](Data_Kind::Function const& function) {
                value = from_function_expression(function);
            },
            [&](Data_Kind::LValue const& lvalue) {
                value = from_lvalue(lvalue.first);
            } },
        expression);
    // clang-format on
    return value;
}

/**
 * @brief Type check a return value against the function's previous returns
 *
 *  Examples:
 *
 *    if (x) return(1);
 *    return(2);          // allowed
 *    return(2.5);        // not allowed, int and double
 */
void Type_Checker::from_return_expression(Data_Kind::Kind const& expression)
{
    auto value = from_expression(expression);
    Type type = value.storage == Storage::POINTER ? "word" : value.type;
    if (frame_.ret.has_value() and not is_compatible(*frame_.ret, type))
        type_error(
            fmt::format("invalid return statement, right-hand-side '{}' with "
                        "type '{}' is not the same type as a previous return "
                        "({})",
                get_symbol_from_expression(expression),
                type,
                *frame_.ret),
            frame_.symbol);
    if (!frame_.ret.has_value() or *frame_.ret == "null" or
        *frame_.ret == "word")
        frame_.ret = type;
}

/**
 * @brief The value of a literal, where a string literal is a pointer
 */
Value Type_Checker::from_literal(literal::Literal const& literal)
{
    return get_value_from_literal(literal);
}

/**
 * @brief The value of a local, parameter, vector, or vector offset lvalue
 *
 *  * A vector name is its first element, e.g. the vector `unit 10;`
 *
 *  * An indirect lvalue, `*p`, is the dereference of its pointer
 */
Value Type_Checker::from_lvalue(LValue const& lvalue)
{
    if (lvalue.starts_with("*")) {
        auto pointer = lvalue.substr(1);
        return from_dereference(from_lvalue(pointer), pointer);
    }
    if (is_union_member(lvalue))
        return from_union_member(lvalue);
    if (frame_.unions.contains(lvalue))
        type_error(fmt::format("invalid use of union '{}', a tag of a union "
                               "is accessed with '{}.<tag>'",
                       lvalue,
                       lvalue),
            lvalue);
    if (unions_.contains(lvalue))
        type_error(fmt::format("invalid use of union definition '{}', a tag "
                               "of a union is accessed with '{}.<tag>'",
                       lvalue,
                       lvalue),
            lvalue);
    if (util::contains(lvalue, "["))
        return from_vector_offset(lvalue);
    if (frame_.locals.contains(lvalue))
        return frame_.locals.at(lvalue);
    if (auto* vector = get_vector(lvalue)) {
        if (vector->data.contains("0"))
            return vector->data.at("0");
        return Value{ Storage::POINTER, "word" };
    }
    return Value{ Storage::SCALAR, "word" };
}

/**
 * @brief The value of a tag of a union variable
 *
 * A tag is the type of its initial value. The values of a tuple tag are
 * read by binding them to locals in an `is` expression.
 *
 *  Examples:
 *
 *    union data_t { ab "", cd 0, list (0, "") };
 *    union data_t m;
 *    m.ab;           // pointer to string
 *    m.cd;           // int
 *    m.list;         // not allowed, m is data_t.list(a, s) reads it
 *    m.list[0];      // not allowed, m is data_t.list(a, s) reads it
 *    m.xy;           // not allowed, xy is not a tag of data_t
 */
Value Type_Checker::from_union_member(LValue const& lvalue)
{
    auto tag = credence::type::from_lvalue_offset(lvalue);
    auto const& values = get_union_tag(tag);
    if (values.size() != 1 or util::contains(lvalue, "["))
        type_error(fmt::format("invalid union access, the values of tuple "
                               "tag '{}' are read with an 'is' expression, "
                               "e.g. '{} is <union>.{}(a, b)'",
                       tag,
                       tag.substr(0, tag.find('.')),
                       tag.substr(tag.find('.') + 1)),
            lvalue);
    return Value{ values.front().storage, values.front().type };
}

/**
 * @brief The value of an element at a vector offset
 *
 *  * An offset not known at compile time is the type of the first element
 *
 *  * An element that was never assigned is an error
 */
Value Type_Checker::from_vector_offset(LValue const& lvalue)
{
    auto* vector = get_vector_at_offset(lvalue);
    if (vector == nullptr)
        return Value{ Storage::SCALAR, "word" };
    auto offset = get_constant_offset(lvalue);
    if (vector->data.contains(offset))
        return vector->data.at(offset);
    if (vector->unknown_offset.has_value())
        return *vector->unknown_offset;
    if (!util::is_numeric(offset) and not vector->data.empty())
        return vector->data.begin()->second;
    type_error(
        fmt::format(
            "invalid vector access, element at '{}' does not exist", offset),
        lvalue);
    return Value{};
}

/**
 * @brief The value of a unary expression
 */
Value Type_Checker::from_unary_expression(Data_Kind::Unary const& unary)
{
    auto const& [op, operand] = unary;
    switch (op) {
        case Operator::U_ADDR_OF:
            return from_address_of_expression(operand->value);
        case Operator::U_INDIRECTION:
            return from_dereference_expression(operand->value);
        case Operator::PRE_INC:
        case Operator::POST_INC:
        case Operator::PRE_DEC:
        case Operator::POST_DEC:
            return from_increment_expression(operand->value);
        default:
            break;
    }
    auto value = from_expression(operand->value);
    if (value.storage == Storage::POINTER)
        return Value{ Storage::SCALAR, "word" };
    return Value{ Storage::SCALAR, value.type };
}

/**
 * @brief The value of an address-of expression
 *
 *  Examples:
 *
 *    p = &x;          // allowed
 *    p = &array[2];   // allowed
 *    p = &strings[1]; // not allowed, a pointer to a string pointer
 */
Value Type_Checker::from_address_of_expression(Data_Kind::Kind const& operand)
{
    auto const& kind = unwrap(operand);
    auto symbol = get_symbol_from_expression(operand);
    if (!std::holds_alternative<Data_Kind::LValue>(kind))
        type_error("invalid address-of expression, the operand is not an "
                   "lvalue",
            symbol);
    auto value = from_lvalue(symbol);
    if (util::contains(symbol, "[") and value.type == "string")
        type_error(fmt::format("invalid address-of expression, '&{}' is a "
                               "pointer to a string pointer, which is not "
                               "allowed",
                       symbol),
            symbol);
    return Value{ Storage::POINTER, value.type, symbol };
}

/**
 * @brief The value of a dereference expression
 *
 *  * The operand must be a pointer
 *
 *  * The pointer must not be null, or point to an uninitialized object
 */
Value Type_Checker::from_dereference_expression(Data_Kind::Kind const& operand)
{
    return from_dereference(
        from_expression(operand), get_symbol_from_expression(operand));
}

/**
 * @brief The value of the object a pointer value points to
 */
Value Type_Checker::from_dereference(Value const& value, RValue const& symbol)
{
    if (value.storage != Storage::POINTER)
        type_error(
            fmt::format(
                "invalid pointer dereference, '{}' is not a pointer", symbol),
            symbol);
    if (value.address.has_value()) {
        auto object = from_lvalue(*value.address);
        if (object.storage == Storage::SCALAR and object.type == "null")
            type_error(fmt::format("invalid pointer dereference, '{}' points "
                                   "to an uninitialized object",
                           symbol),
                symbol);
        return object;
    }
    if (value.type == "null")
        type_error(
            fmt::format(
                "invalid pointer dereference, '{}' is a null pointer", symbol),
            symbol);
    if (value.type == "string")
        return Value{ Storage::SCALAR, "char" };
    return Value{ Storage::SCALAR, value.type };
}

/**
 * @brief The value of an increment or decrement expression
 *
 *  Examples:
 *
 *    auto x, y, *p;
 *    x = 5;
 *    x++;            // allowed
 *    y = x + 1;
 *    y++;            // allowed, a word
 *    p = &x;
 *    p++;            // allowed, pointer arithmetic
 *    auto z;
 *    z++;            // not allowed, z has no type
 */
Value Type_Checker::from_increment_expression(Data_Kind::Kind const& operand)
{
    auto value = from_expression(operand);
    auto symbol = get_symbol_from_expression(operand);
    if (value.storage == Storage::POINTER)
        return value;
    if (!is_numeric(value.type))
        type_error(fmt::format("invalid numeric unary expression, '{}' with "
                               "type '{}' is not a numeric type",
                       symbol,
                       value.type),
            symbol);
    return value;
}

/**
 * @brief The value of a binary or ternary relation is a word
 */
Value Type_Checker::from_relation_expression(
    Data_Kind::Relation const& relation)
{
    if (relation.first == Operator::R_IS)
        return from_is_expression(relation);
    for (auto const& block : relation.second)
        from_expression(block->value);
    return Value{ Storage::SCALAR, "word" };
}

/**
 * @brief The value of an `is` expression is a word
 *
 * The left-hand-side is a union variable, and the right-hand-side is a tag
 * of its union
 *
 *  Examples:
 *
 *    union data_t { ab "", cd 0 };
 *    union other_t { ef 0 };
 *    union data_t m;
 *    m is data_t.ab;       // allowed
 *    m is other_t.ef;      // not allowed, m is a data_t
 *    m is data_t.xy;       // not allowed, xy is not a tag of data_t
 *    m.cd is data_t.cd;    // not allowed, m.cd is not a union variable
 */
Value Type_Checker::from_is_expression(Data_Kind::Relation const& relation)
{
    credence_assert_equal(relation.second.size(), 2UL);
    auto const& lhs = unwrap(relation.second.front()->value);
    auto const& pattern = unwrap(relation.second.back()->value);
    auto lhs_symbol = get_symbol_from_expression(lhs);
    // a tag that binds its values is the tag and the lvalue of each local
    auto const* bindings = std::holds_alternative<Data_Kind::Function>(pattern)
                               ? &std::get<Data_Kind::Function>(pattern).second
                               : nullptr;
    Data_Kind::Kind rhs = pattern;
    if (bindings != nullptr)
        rhs = std::get<Data_Kind::Function>(pattern).first;
    auto rhs_symbol = get_symbol_from_expression(rhs);
    if (!std::holds_alternative<Data_Kind::LValue>(lhs) or
        not frame_.unions.contains(lhs_symbol))
        type_error(fmt::format("invalid 'is' expression, left-hand-side '{}' "
                               "is not a union variable",
                       lhs_symbol),
            lhs_symbol);
    auto const& label = frame_.unions.at(lhs_symbol);
    auto separator = rhs_symbol.find('.');
    if (!std::holds_alternative<Data_Kind::LValue>(rhs) or
        separator == RValue::npos or rhs_symbol.substr(0, separator) != label or
        not unions_.at(label).tags.contains(rhs_symbol.substr(separator + 1)))
        type_error(fmt::format("invalid 'is' expression, right-hand-side '{}' "
                               "is not a tag of union '{}', the union of '{}'",
                       rhs_symbol,
                       label,
                       lhs_symbol),
            rhs_symbol);
    if (bindings != nullptr)
        bind_union_tag(rhs_symbol, *bindings);
    return Value{ Storage::SCALAR, "word" };
}

/**
 * @brief Assign the values of a union tag to the locals of an `is` pattern
 *
 * Each value of the tag is assigned to its local, so a local is the type of
 * its value, and a string value is bound to a pointer
 *
 *  Examples:
 *
 *    union data_t { list (0, "") };
 *    auto n, *s, x;
 *    union data_t m;
 *    m is data_t.list(n, s);    // allowed, n is an int, s a string pointer
 *    m is data_t.list(n);       // not allowed, list has 2 values
 *    m is data_t.list(n, x);    // not allowed, x is not a pointer
 */
void Type_Checker::bind_union_tag(LValue const& tag,
    std::vector<Data_Kind::Pointer> const& bindings)
{
    auto label = tag.substr(0, tag.find('.'));
    auto const& values =
        unions_.at(label).tags.at(tag.substr(tag.find('.') + 1));
    if (bindings.size() != values.size())
        type_error(fmt::format("invalid 'is' expression, tag '{}' has {} "
                               "values, and {} are bound",
                       tag,
                       values.size(),
                       bindings.size()),
            tag);
    for (std::size_t index = 0; index < bindings.size(); index++) {
        auto symbol = get_symbol_from_expression(bindings[index]->value);
        if (!frame_.locals.contains(symbol))
            type_error(fmt::format("invalid 'is' expression, '{}' is not a "
                                   "local",
                           symbol),
                symbol);
        auto const& value = values[index];
        assign(symbol, Value{ value.storage, value.type }, tag);
    }
}

/**
 * @brief The value of a function call is a word
 *
 * Each argument is recorded, so a parameter that is passed a pointer is a
 * pointer. A string, a string element of a vector, and an element of `argv`
 * are addresses.
 */
Value Type_Checker::from_function_expression(
    Data_Kind::Function const& function)
{
    auto const& [callee, arguments] = function;
    for (std::size_t index = 0; index < arguments.size(); index++) {
        auto value = from_expression(arguments[index]->value);
        if (call_arguments_ == nullptr)
            continue;
        auto symbol = get_symbol_from_expression(arguments[index]->value);
        auto is_argv =
            frame_.symbol == "main" and
            ::credence::type::from_lvalue_offset(symbol) == "argv" and
            util::contains(symbol, "[");
        call_arguments_->emplace_back(Call_Argument{ frame_.symbol,
            callee.first,
            index,
            value.storage == Storage::POINTER or value.type == "string" or
                is_argv,
            frame_.parameters.contains(symbol) ? symbol : LValue{} });
    }
    return Value{ Storage::SCALAR, "word" };
}

/**
 * @brief Type check an assignment expression
 */
Value Type_Checker::from_assignment_expression(
    Data_Kind::Symbol const& assignment)
{
    auto const& [lvalue, expression] = assignment;
    auto symbol = get_symbol_from_expression(expression->value);
    auto rvalue = from_expression(expression->value);
    assign(lvalue.first, rvalue, symbol);
    return rvalue;
}

/**
 * @brief Assign a value to a local, parameter, dereference, or vector lvalue
 */
void Type_Checker::assign(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    if (lvalue.starts_with("*")) {
        assign_to_dereference(lvalue.substr(1), rvalue, symbol);
        return;
    }
    if (is_union_member(lvalue)) {
        assign_to_union_member(lvalue, rvalue, symbol);
        return;
    }
    if (frame_.unions.contains(lvalue))
        type_error(fmt::format("invalid assignment, union '{}' is assigned "
                               "through a tag, '{}.<tag>'",
                       lvalue,
                       lvalue),
            symbol);
    if (util::contains(lvalue, "[")) {
        assign_to_vector(lvalue, rvalue, symbol);
        return;
    }
    if (frame_.locals.contains(lvalue)) {
        if (frame_.locals.at(lvalue).storage == Storage::POINTER)
            assign_to_pointer(lvalue, rvalue, symbol);
        else
            assign_to_scalar(lvalue, rvalue, symbol);
        return;
    }
    if (get_vector(lvalue) != nullptr)
        assign_to_vector(fmt::format("{}[0]", lvalue), rvalue, symbol);
}

/**
 * @brief Type check an assignment to a scalar local or parameter
 *
 *  Examples:
 *
 *    auto x, y, *p;
 *    x = 5;            // allowed
 *    y = x;            // allowed
 *    y = x + 1;        // allowed, a word
 *    x = 2.5;          // not allowed, int and double
 *    x = p;            // not allowed, x is not a pointer
 *    x = "hello";      // not allowed, x is not a pointer
 */
void Type_Checker::assign_to_scalar(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    auto& object = frame_.locals.at(lvalue);
    if (rvalue.storage == Storage::POINTER)
        type_error(fmt::format("invalid pointer assignment, left-hand-side "
                               "'{}' is not a pointer",
                       lvalue),
            symbol);
    if (!is_compatible(object.type, rvalue.type))
        type_error(fmt::format("invalid assignment, right-hand-side '{}' with "
                               "type '{}' is not the same type ({})",
                       symbol,
                       rvalue.type,
                       object.type),
            lvalue);
    if (rvalue.type != "null")
        object.type = rvalue.type;
    object.constant = rvalue.constant;
}

/**
 * @brief Type check an assignment to a pointer local or parameter
 *
 * Once a pointer points to a type, it may only point to that type
 *
 *  Examples:
 *
 *    auto *p, *q, x, d;
 *    x = 5;
 *    d = 2.5;
 *    p = &x;           // allowed
 *    q = p;            // allowed
 *    p = p + 1;        // allowed, a word
 *    p = &d;           // not allowed, a pointer to int and double
 *    p = x;            // not allowed, x is not a pointer
 */
void Type_Checker::assign_to_pointer(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    auto& object = frame_.locals.at(lvalue);
    if (rvalue.storage == Storage::POINTER) {
        auto type = get_pointer_type(object);
        if (!is_compatible(type, get_pointer_type(rvalue)))
            type_error(fmt::format("invalid pointer assignment, left-hand-side "
                                   "'{}' is a pointer to '{}', and '{}' is a "
                                   "pointer to '{}'",
                           lvalue,
                           type,
                           symbol,
                           get_pointer_type(rvalue)),
                lvalue);
        object = rvalue;
        return;
    }
    if (rvalue.type == "word" or rvalue.type == "string") {
        object = Value{ Storage::POINTER, rvalue.type };
        return;
    }
    type_error(fmt::format("invalid pointer assignment, right-hand-side '{}' "
                           "is not a pointer",
                   symbol),
        lvalue);
}

/**
 * @brief Type check an assignment through a dereferenced pointer
 *
 * A pointer from an address-of expression assigns to the object it points
 * to. Otherwise, the right-hand-side must agree with the pointer's type,
 * and the assignment through a null pointer sets the type it points to.
 *
 *  Examples:
 *
 *    auto *p, *q, x;
 *    x = 5;
 *    p = &x;
 *    *p = 10;          // allowed, assigns to x
 *    *p = 2.5;         // not allowed, int and double
 *    *q = 5;           // allowed, q is now a pointer to int
 *    *q = 2.5;         // not allowed, int and double
 *    *q = p;           // not allowed, the right-hand-side is a pointer
 */
void Type_Checker::assign_to_dereference(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    if (!frame_.locals.contains(lvalue) or
        frame_.locals.at(lvalue).storage != Storage::POINTER)
        type_error(fmt::format("invalid pointer dereference assignment, "
                               "left-hand-side '{}' is not a pointer",
                       lvalue),
            symbol);
    auto& object = frame_.locals.at(lvalue);
    if (object.address.has_value()) {
        assign(*object.address, rvalue, symbol);
        return;
    }
    if (rvalue.storage == Storage::POINTER and object.type != "word")
        type_error(fmt::format("invalid pointer dereference assignment, "
                               "right-hand-side '{}' is a pointer",
                       symbol),
            lvalue);
    if (!is_compatible(object.type, rvalue.type))
        type_error(fmt::format("invalid dereference assignment, '*{}' with "
                               "type '{}' is not the same type ({})",
                       lvalue,
                       object.type,
                       rvalue.type),
            symbol);
    if (object.type == "null")
        object.type = rvalue.type;
}

/**
 * @brief Type check an assignment to a vector element
 *
 *  Examples:
 *
 *    extrn numbers, strings;
 *    numbers[0] = 5;           // allowed
 *    strings[1] = "hi";        // allowed
 *    strings[1] = numbers[2];  // not allowed, string and int
 *    numbers[5] = 5;           // not allowed, numbers has 3 elements
 */
void Type_Checker::assign_to_vector(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    auto* vector = get_vector_at_offset(lvalue);
    if (vector == nullptr)
        return;
    auto offset = credence::type::from_decay_offset(lvalue);
    Type type = rvalue.type;
    if (rvalue.storage == Storage::POINTER and rvalue.type != "string")
        type = "word";
    std::optional<Value> element{};
    if (vector->data.contains(offset))
        element = vector->data.at(offset);
    else if (vector->unknown_offset.has_value())
        element = vector->unknown_offset;
    else if (!util::is_numeric(offset) and not vector->data.empty())
        element = vector->data.begin()->second;
    if (element.has_value() and not is_compatible(element->type, type))
        type_error(fmt::format("invalid vector assignment, left-hand-side "
                               "'{}' with type '{}' is not the same type ({})",
                       lvalue,
                       element->type,
                       type),
            symbol);
    if (type == "null")
        return;
    // an offset not known at compile time may be any element in a loop
    if (util::is_numeric(offset))
        vector->data[offset] =
            Value{ Storage::SCALAR, type, {}, rvalue.constant };
    else
        vector->unknown_offset = Value{ Storage::SCALAR, type };
}

/**
 * @brief Type check an assignment to a tag of a union variable
 *
 * The right-hand-side must be the type of the tag. Each value of a tuple tag
 * is assigned at a constant offset, and must be the type of that value.
 *
 *  Examples:
 *
 *    union data_t { ab "", ll 0.0f, list (0, "") };
 *    union data_t m;
 *    m.ab = "hello";       // allowed
 *    m.ll = 5.5f;          // allowed
 *    m.list[1] = "hi";     // allowed
 *    m.ll = 5;             // not allowed, float and int
 *    m.ab = 5;             // not allowed, ab is a string pointer
 *    m.list = 5;           // not allowed, list is a tuple
 *    m.list[0] = "hi";     // not allowed, int and string
 *    m.list[2] = 5;        // not allowed, list has 2 values
 */
void Type_Checker::assign_to_union_member(LValue const& lvalue,
    Value const& rvalue,
    RValue const& symbol)
{
    auto member = credence::type::from_lvalue_offset(lvalue);
    auto const& values = get_union_tag(member);
    std::size_t index = 0;
    if (util::contains(lvalue, "[")) {
        auto offset = credence::type::from_decay_offset(lvalue);
        if (values.size() == 1 or not util::is_numeric(offset))
            type_error(fmt::format("invalid union assignment, '{}' is not a "
                                   "constant offset of a tuple tag",
                           lvalue),
                symbol);
        index = std::stoul(offset);
        if (index >= values.size())
            type_error(fmt::format("invalid union assignment, '{}' is out of "
                                   "range of tuple tag '{}' with {} values",
                           lvalue,
                           member,
                           values.size()),
                symbol);
    } else if (values.size() != 1)
        type_error(fmt::format("invalid union assignment, tag '{}' is a tuple "
                               "of {} values, and each value is assigned with "
                               "'{}[<index>]'",
                       lvalue,
                       values.size(),
                       lvalue),
            symbol);
    auto const& tag = values[index];
    if (tag.storage == Storage::SCALAR and rvalue.storage == Storage::POINTER)
        type_error(fmt::format("invalid union assignment, left-hand-side '{}' "
                               "is not a pointer",
                       lvalue),
            symbol);
    if (tag.storage == Storage::POINTER and
        rvalue.storage == Storage::SCALAR and rvalue.type != "word")
        type_error(
            fmt::format("invalid union assignment, right-hand-side '{}' "
                        "is not a pointer, and '{}' is a pointer to '{}'",
                symbol,
                lvalue,
                tag.type),
            symbol);
    if (!is_compatible(tag.type, rvalue.type))
        type_error(
            fmt::format("invalid union assignment, right-hand-side '{}' "
                        "with type '{}' is not the type of tag '{}' ({})",
                symbol,
                rvalue.type,
                lvalue,
                tag.type),
            symbol);
}

/**
 * @brief Check the boundary of a vector offset and get its vector
 *
 * Returns nullptr for an offset of a pointer or a parameter, which has no
 * size at compile time.
 */
Vector* Type_Checker::get_vector_at_offset(LValue const& lvalue)
{
    auto label = credence::type::from_lvalue_offset(lvalue);
    auto offset = credence::type::from_decay_offset(lvalue);
    auto* vector = get_vector(label);
    if (vector == nullptr) {
        if (is_parameter_or_pointer(label))
            return nullptr;
        type_error(fmt::format("invalid vector access, vector identifier "
                               "'{}' does not exist",
                       label),
            lvalue);
    }
    if (util::is_numeric(offset)) {
        auto index = std::stoul(offset);
        if (index > max_vector_size)
            type_error(fmt::format("invalid vector offset, integer offset "
                                   "'{}' is a buffer-overflow",
                           index),
                lvalue);
        if (index >= vector->size)
            type_error(fmt::format("invalid out-of-range vector access '{}' "
                                   "at index '{}'",
                           label,
                           index),
                lvalue);
    } else if (!frame_.locals.contains(offset))
        type_error(fmt::format("invalid vector offset '{}'", offset), lvalue);
    auto constant = get_constant_offset(lvalue);
    if (constant != offset and util::is_numeric(constant) and
        std::stoul(constant) >= vector->size)
        type_error(fmt::format("invalid out-of-range vector access '{}' at "
                               "index '{}', the value of '{}'",
                       label,
                       constant,
                       offset),
            lvalue);
    return vector;
}

/**
 * @brief The offset of a vector lvalue, or the constant of its scalar offset
 *
 *  Examples:
 *
 *    x = 2;
 *    v[x];           // 2
 *    v[i];           // i, not known at compile time
 */
RValue Type_Checker::get_constant_offset(LValue const& lvalue)
{
    auto offset = credence::type::from_decay_offset(lvalue);
    if (frame_.locals.contains(offset) and
        frame_.locals.at(offset).constant.has_value())
        return *frame_.locals.at(offset).constant;
    return offset;
}

/**
 * @brief The type a pointer value points to
 *
 * The type of a pointer from an address-of expression is the current type
 * of the object it points to
 */
Type Type_Checker::get_pointer_type(Value const& value)
{
    if (!value.address.has_value() or
        not frame_.locals.contains(*value.address))
        return value.type;
    auto const& object = frame_.locals.at(*value.address);
    return object.storage == Storage::POINTER ? Type{ "word" } : object.type;
}

/**
 * @brief Get a vector in the stack frame or the global vectors
 */
Vector* Type_Checker::get_vector(LValue const& label)
{
    if (frame_.locals.contains(label))
        return nullptr;
    if (frame_.vectors.contains(label))
        return &frame_.vectors.at(label);
    if (globals_.contains(label))
        return &globals_.at(label);
    return nullptr;
}

/**
 * @brief Check if a label is a parameter or a pointer in the stack frame
 */
bool Type_Checker::is_parameter_or_pointer(LValue const& label)
{
    return frame_.parameters.contains(label) or
           (frame_.locals.contains(label) and
               frame_.locals.at(label).storage == Storage::POINTER);
}

/**
 * @brief The values of the tag of a union variable, e.g. `m.ab`
 *
 * A tag of a union definition, `data_t.ab`, is only the right-hand-side of
 * an `is` expression
 */
std::vector<Value> const& Type_Checker::get_union_tag(LValue const& lvalue)
{
    auto separator = lvalue.find('.');
    auto object = lvalue.substr(0, separator);
    auto tag = lvalue.substr(separator + 1);
    if (!frame_.unions.contains(object)) {
        if (unions_.contains(object))
            type_error(fmt::format("invalid union tag, '{}' is only the "
                                   "right-hand-side of an 'is' expression",
                           lvalue),
                lvalue);
        type_error(fmt::format("invalid member access, '{}' is not a union "
                               "variable",
                       object),
            lvalue);
    }
    auto const& label = frame_.unions.at(object);
    auto const& definition = unions_.at(label);
    if (!definition.tags.contains(tag))
        type_error(fmt::format("invalid union member access, '{}' is not a "
                               "tag of union '{}'",
                       tag,
                       label),
            lvalue);
    return definition.tags.at(tag);
}

/**
 * @brief Raise a type check error in the current stack frame
 */
void Type_Checker::type_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location)
{
    throw_compiletime_error(
        message, symbol, location, "symbol", frame_.symbol, hoisted_symbols_);
}

} // namespace credence::language::hir::checking
