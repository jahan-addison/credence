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

#include <algorithm>   // for all_of
#include <cctype>      // for isdigit
#include <cstddef>     // for size_t
#include <map>         // for map
#include <string>      // for basic_string, string
#include <string_view> // for basic_string_view, string_view
#include <type_traits> // for is_constructible_v
#include <utility>     // for pair, make_pair, move
#include <variant>     // for monostate, variant
#include <vector>      // for vector

/****************************************************************************
 *
 * Type Data
 *
 * The internal representation of values and types during code translation.
 *
 * We enforce strict typing via type inference by storing all data types
 * assigned through a data structure called internally "Type Data" - the
 * leaves of the HIR in hir.h.
 *
 *  I.e. A tuple of ( Value : Type : Size )
 *
 *  Examples:
 *
 *  (10:int:4)
 *  ("hello":string:5)
 *  (55.5:float:4)
 *  ('c':char:1)
 *
 *  ---
 *
 *   main() {
 *     auto x, y, z;
 *     x = 42;           // x is (42:int:4)
 *     y = 3.14;         // y is (3.14:double:8)
 *   }
 *****************************************************************************/

namespace credence::language::literal {

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

constexpr bool is_integer_string(std::string_view const& str)
{
    return std::ranges::all_of(str,
        [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

std::string literal_to_string(Literal const& literal,
    std::string_view separator = ":");

template<typename T>
inline Literal make_literal_value(T value, Size size)
{
    static_assert(std::is_constructible_v<Literal, T>,
        "Error: Type T is not a valid alternative in "
        "literal::Literal");
    return std::pair{ std::move(value), std::move(size) };
}

} // namespace literal
