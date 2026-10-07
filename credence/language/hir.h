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

#pragma once

#include "literal.h"       // for Literal, Array, WORD_LITERAL
#include "operators.h"     // for Operator
#include <credence/util.h> // for overload
#include <memory>          // for shared_ptr, make_shared
#include <string>          // for basic_string, string
#include <string_view>     // for basic_string_view, string_view
#include <type_traits>     // for is_constructible_v
#include <utility>         // for pair, make_pair, move
#include <variant>         // for get, monostate, variant, visit
#include <vector>          // for vector

/****************************************************************************
 *
 * High level intermediate representation
 *
 * The HIR is a tree of Data_Kind nodes. AST_Lowering (ast_lowering.h)
 * lowers the Parser's AST_Node expressions into it, and Shunting_Yard
 * (precedence.h) orders it into a queue for the IR.
 *
 * Data_Kind::Kind is the algebraic sum of every expression shape the rest
 * of the compiler cares about - Literal, Array, Symbol, Unary, Relation,
 * Function, and LValue. The leaves are the ( Value : Type : Size ) tuple
 * from literal.h.
 *
 *  Example:
 *
 *   main() {
 *     auto x;
 *     x = 5 + 3 * 2;
 *   }
 *
 *   HIR:  Symbol(LValue("x"),
 *                Relation(B_ADD,
 *                         Literal(5:int:4),
 *                         Relation(B_MUL, ...)))
 *
 *****************************************************************************/

namespace credence::language::hir {

/**
 * @brief HIR node, the algebraic sum of expression kinds
 */
struct Data_Kind
{
    explicit constexpr Data_Kind() = default;
    using Pointer = std::shared_ptr<Data_Kind>;
    using LValue = std::pair<std::string, literal::Literal>;
    using Symbol = std::pair<LValue, Pointer>;
    using Unary = std::pair<type::Operator, Pointer>;
    using Relation = std::pair<type::Operator, std::vector<Pointer>>;
    using Function = std::pair<LValue, std::vector<Pointer>>;
    using Kind = std::variant<std::monostate,
        Pointer,
        literal::Array,
        Symbol,
        Unary,
        Relation,
        Function,
        LValue,
        literal::Literal>;
    using Kind_Pointer = std::shared_ptr<Kind>;
    Kind value;
};

/**
 * @brief Get the name of a Data_Kind::Kind variant's alternative
 */
constexpr std::string get_kind_name(Data_Kind::Kind const& value)
{
    std::string type{};
    // clang-format off
    std::visit(
        util::overload{
            [&](std::monostate) { },
            [&](literal::Array const&) { type = "array"; },
            [&](literal::Literal const&) { type = "literal"; },
            [&](Data_Kind::Pointer const&) { type = "pointer"; },
            [&](Data_Kind::Symbol const&) { type = "symbol"; },
            [&](Data_Kind::Unary const&) { type = "unary"; },
            [&](Data_Kind::Relation const&) { type = "relation"; },
            [&](Data_Kind::Function const&) { type = "function"; },
            [&](Data_Kind::LValue const&) { type = "lvalue"; } },
        value);
    // clang-format on
    return type;
}

std::string data_kind_to_string(Data_Kind::Kind const& item,
    bool separate = true,
    std::string_view separator = ":");

inline Data_Kind::LValue make_lvalue(std::string const& name)
{
    return std::make_pair(name, literal::WORD_LITERAL);
}

template<typename T>
inline Data_Kind::LValue make_lvalue(std::string name, T value)
{
    static_assert(std::is_constructible_v<literal::Literal, T>,
        "Error: Type T is not a valid alternative in "
        "literal::Literal");
    return { std::move(name), std::move(value) };
}

inline Data_Kind::Kind_Pointer make_kind_pointer(
    Data_Kind::Kind type) // not constexpr until C++23
{
    return std::make_shared<Data_Kind::Kind>(type);
}

template<typename T>
constexpr T get_literal_from_kind_pointer(Data_Kind::Kind_Pointer const& type)
{
    static_assert(std::is_constructible_v<literal::Literal, T>,
        "Error: Type T is not a valid alternative in literal::Literal");
    return std::get<T>(std::get<literal::Literal>(*type).first);
}

inline bool is_kind(Data_Kind::Kind_Pointer const& value, std::string_view type)
{
    return get_kind_name(*value) == type;
}

constexpr bool is_kind(Data_Kind const& expression, std::string_view type)
{
    return get_kind_name(expression.value) == type;
}

} // namespace hir
