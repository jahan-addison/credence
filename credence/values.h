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
#pragma once

#include <credence/operators.h> // for Operator
#include <credence/util.h>      // for is_variant
#include <cstddef>              // for size_t
#include <mapbox/eternal.hpp>   // for map
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

#ifdef __clang__
constexpr auto TYPE_LITERAL =
    mapbox::eternal::map<std::string_view, std::pair<std::string, std::size_t>>(
        { { "word", { "word", sizeof(void*) } },
          { "byte", { "byte", sizeof(unsigned char) } },
          { "int", { "int", sizeof(int) } },
          { "long", { "long", sizeof(long) } },
          { "float", { "float", sizeof(float) } },
          { "double", { "double", sizeof(double) } },
          { "bool", { "bool", sizeof(bool) } },
          { "null", { "null", 0 } },
          { "char", { "char", sizeof(char) } } });
#else
static auto TYPE_LITERAL =
    mapbox::eternal::map<std::string_view, std::pair<std::string, std::size_t>>(
        { { "word", { "word", sizeof(void*) } },
          { "byte", { "byte", sizeof(unsigned char) } },
          { "int", { "int", sizeof(int) } },
          { "long", { "long", sizeof(long) } },
          { "float", { "float", sizeof(float) } },
          { "double", { "double", sizeof(double) } },
          { "bool", { "bool", sizeof(bool) } },
          { "null", { "null", 0 } },
          { "char", { "char", sizeof(char) } } });
#endif

namespace detail {

using Literal = std::variant<
    std::monostate,
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

    static constexpr auto NULL_LITERAL =
        std::pair<std::monostate, std::pair<std::string, std::size_t>>{
            std::monostate{},
            std::pair<std::string, std::size_t>{ "null", 0 }
        };

    static constexpr auto WORD_LITERAL =
        std::pair<std::string, std::pair<std::string, std::size_t>>{
            "__WORD__",
            std::pair<std::string, std::size_t>{ "word", sizeof(void*) }
        };

    using Pointer = std::shared_ptr<Expression>;

    using LValue = std::pair<std::string, Literal>;

    using Symbol = std::pair<LValue, Pointer>;

    using Unary = std::pair<type::Operator, Pointer>;

    using Relation = std::pair<type::Operator, std::vector<Pointer>>;

    using Function = std::pair<LValue, std::vector<Pointer>>;

    using Type = std::variant<
        std::monostate,
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

std::string literal_to_string(
    Literal const& literal,
    std::string_view separator = ":");

std::string expression_type_to_string(
    Expression::Type const& item,
    bool separate = true,
    std::string_view separator = ":");

inline Expression::LValue make_lvalue(std::string const& name)
{
    return std::make_pair(name, Expression::WORD_LITERAL);
}

template<typename T>
constexpr Expression::LValue make_lvalue(std::string name, T value)
{
    static_assert(
        std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in Expression::Literal_");
    return { std::move(name), std::move(value) };
}

template<typename T>
constexpr Literal make_literal_value(T value, Size size)
{
    static_assert(
        std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in Expression::Literal_");
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
    static_assert(
        std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in value::Expression");
    return std::get<T>(std::get<Literal>(*type).first);
}

inline bool is_value_type_pointer_type(
    Expression::Type_Pointer const& value,
    std::string_view type)
{
    return get_expression_type(*value) == type;
}

constexpr bool is_value_type(
    Expression const& expression,
    std::string_view type)
{
    return get_expression_type(expression.value) == type;
}

} // namespace value

} // namespace credence