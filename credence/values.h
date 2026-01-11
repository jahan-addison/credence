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

/****************************************************************************
 *
 * Value representation and type literals
 *
 * This module defines the internal representation of values and types in
 * the B language. Values can be literals (int, float, double, string),
 * arrays, or expressions. The type system uses type inference - variables
 * aren't declared with explicit types, their type is determined by the
 * value assigned to them.
 *
 * Example:
 *
 *   main() {
 *     auto x, y, *z;
 *     x = 42;        // x inferred as int
 *     y = 3.14f;     // y inferred as float
 *     z = &x;        // z is pointer to x
 *   }
 *
 * Vectors (arrays) may be non-homogeneous but their type is determined by
 * their initial values:
 *
 *   numbers [3] 10, 20, 30;     // array of ints
 *   mixed [2] 5, 2.5f;          // first element determines type
 *
 *****************************************************************************/

#pragma once

#include <credence/operators.h> // for Operator
#include <credence/util.h>      // for is_variant
#include <cstddef>              // for size_t
#include <memory>               // for shared_ptr, make_shared
#include <string>               // for basic_string, string
#include <string_view>          // for basic_string_view, string_view
#include <type_traits>          // for is_constructible_v, underlying_type_t
#include <utility>              // for pair, make_pair
#include <variant>              // for get, monostate, variant
#include <vector>               // for vector

namespace credence {

/**
 * @brief Internal data structures for value and type representation
 */

namespace value {

struct Expression;

const auto TYPE_LITERAL =
    std::map<std::string_view, std::pair<std::string, std::size_t>>({
        { "word",   { "word", sizeof(void*) }         },
        { "byte",   { "byte", sizeof(unsigned char) } },
        { "int",    { "int", sizeof(int) }            },
        { "long",   { "long", sizeof(long) }          },
        { "float",  { "float", sizeof(float) }        },
        { "double", { "double", sizeof(double) }      },
        { "bool",   { "bool", sizeof(bool) }          },
        { "null",   { "null", 0 }                     },
        { "char",   { "char", sizeof(char) }          }
});

const std::pair<std::monostate, std::pair<std::string, std::size_t>>
    NULL_LITERAL =
        std::make_pair(std::monostate{}, std::make_pair("null", 0UL));

const std::pair<std::string, std::pair<std::string, std::size_t>> WORD_LITERAL =
    std::make_pair("__WORD__", std::make_pair("word", sizeof(void*)));

namespace detail {
using Literal = std::variant<std::monostate,
    int,
    long,
    unsigned char,
    float,
    double,
    bool,
    std::string,
    char>;

} // namespace detail

using Size = std::pair<std::string, std::size_t>;

using Literal = std::pair<detail::Literal, Size>;

using Array = std::vector<Literal>;

struct Expression
{
    explicit constexpr Expression() = default;

    using Pointer = std::shared_ptr<Expression>;
    using LValue = std::pair<std::string, Literal>;
    using Symbol = std::pair<LValue, Pointer>;
    using Unary = std::pair<type::Operator, Pointer>;
    using Relation = std::pair<type::Operator, std::vector<Pointer>>;
    using Function = std::pair<LValue, std::vector<Pointer>>;
    using Type = std::variant<std::monostate,
        Pointer,
        Array,
        Symbol,
        Unary,
        Relation,
        Function,
        LValue,
        Literal>;
    using Type_Pointer = std::shared_ptr<Type>;
    Type value;
};

/**
 * @brief Get the type of an Expression::Type variant
 */
constexpr std::string get_expression_type(Expression::Type const& value)
{
    std::string type{};
    // clang-format off
    std::visit(
        util::overload{
            [&](std::monostate) { },
            [&](Array const&) { type = "array"; },
            [&](Literal const&) { type = "literal"; },
            [&](Expression::Pointer const&) { type = "pointer"; },
            [&](Expression::Symbol const&) { type = "symbol"; },
            [&](Expression::Unary const&) { type = "unary"; },
            [&](Expression::Relation const&) { type = "relation"; },
            [&](Expression::Function const&) { type = "function"; },
            [&](Expression::LValue const&) { type = "lvalue"; } },
        value);
    // clang-format on
    return type;
}

constexpr bool is_integer_string(std::string_view const& str)
{
    return std::ranges::all_of(str,
        [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

std::string literal_to_string(Literal const& literal,
    std::string_view separator = ":");

std::string expression_type_to_string(Expression::Type const& item,
    bool separate = true,
    std::string_view separator = ":");

inline Expression::LValue make_lvalue(std::string const& name)
{
    return std::make_pair(name, WORD_LITERAL);
}

template<typename T>
inline Expression::LValue make_lvalue(std::string name, T value)
{
    static_assert(std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in "
        "Expression::Literal_");
    return { std::move(name), std::move(value) };
}

template<typename T>
inline Literal make_literal_value(T value, Size size)
{
    static_assert(std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in "
        "Expression::Literal_");
    return std::pair{ std::move(value), std::move(size) };
}

inline Expression::Type_Pointer make_value_type_pointer(
    Expression::Type type) // not constexpr until C++23
{
    return std::make_shared<Expression::Type>(type);
}

template<typename T>
constexpr T get_literal_from_type_pointer(Expression::Type_Pointer const& type)
{
    static_assert(std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in value::Expression");
    return std::get<T>(std::get<Literal>(*type).first);
}

inline bool is_value_type_pointer_type(Expression::Type_Pointer const& value,
    std::string_view type)
{
    return get_expression_type(*value) == type;
}

constexpr bool is_value_type(Expression const& expression,
    std::string_view type)
{
    return get_expression_type(expression.value) == type;
}

} // namespace value

} // namespace credence