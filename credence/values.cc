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
 * Value representation implementation
 *
 * Implements string conversion and manipulation of literal values.
 * Handles the internal representation of B language literals including
 * integers, floats, doubles, and pointers.
 *
 * Example - literal string representation:
 *
 *   42     → "(42:int:4)"
 *   3.14f  → "(3.14:float:4)"
 *   3.14   → "(3.14:double:8)"
 *
 * Used throughout the compiler for debug output, type tracking, and
 * generating intermediate representation.
 *
 *****************************************************************************/

#include <credence/values.h>

#include <credence/operators.h> // for Operator
#include <credence/util.h>      // for overload
#include <memory>               // for shared_ptr, make_shared
#include <sstream>              // for basic_ostringstream, ostringstream
#include <string>               // for basic_string, string, char_traits
#include <string_view>          // for basic_string_view, string_view, oper...
#include <utility>              // for pair, get, move, make_pair
#include <variant>              // for get, monostate, variant, visit
#include <vector>               // for vector

namespace credence::value {

/**
 * @brief value::Expression tuple as a string
 */
std::string literal_to_string(Literal const& literal,
    std::string_view separator)
{
    std::ostringstream os;
    os << "(";
    std::visit(util::overload{
                   [&](int i) {
                       os << i << separator << TYPE_LITERAL.at("int").first
                          << separator << TYPE_LITERAL.at("int").second;
                   },
                   [&](long i) {
                       os << i << separator << TYPE_LITERAL.at("long").first
                          << separator << TYPE_LITERAL.at("long").second;
                   },
                   [&](float i) {
                       os << i << separator << TYPE_LITERAL.at("float").first
                          << separator << TYPE_LITERAL.at("float").second;
                   },
                   [&](double i) {
                       os << i << separator << TYPE_LITERAL.at("double").first
                          << separator << TYPE_LITERAL.at("double").second;
                   },
                   [&](bool i) {
                       os << std::boolalpha << i << separator
                          << TYPE_LITERAL.at("bool").first << separator
                          << TYPE_LITERAL.at("bool").second;
                   },
                   [&]([[maybe_unused]] std::monostate i) {
                       os << "null" << separator
                          << TYPE_LITERAL.at("null").first << separator
                          << TYPE_LITERAL.at("null").second;
                   },
                   [&](unsigned char i) {
                       os << i << separator << TYPE_LITERAL.at("byte").first
                          << separator << literal.second.second;
                   },
                   [&](char i) {
                       os << "'" << static_cast<unsigned int>(i) << "'"
                          << separator << TYPE_LITERAL.at("char").first
                          << separator << TYPE_LITERAL.at("char").second;
                   },
                   [&]([[maybe_unused]] std::string const& s) {
                       if (s == "__WORD__") {
                           // pointer
                           os << "__WORD__" << separator
                              << TYPE_LITERAL.at("word").first << separator
                              << TYPE_LITERAL.at("word").second;
                       } else {
                           os << "\"" << std::get<std::string>(literal.first)
                              << "\"" << separator << "string" << separator
                              << std::get<std::string>(literal.first).size();
                       }
                   },
               },
        literal.first);
    os << ")";
    return os.str();
}

/**
 * @brief Expression types to string in reverse polish notation
 */
std::string expression_type_to_string(Expression::Type const& item,
    bool separate,
    std::string_view separator)
{
    auto oss = std::ostringstream();
    auto space = separate ? " " : "";
    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](Expression::Pointer const&) {},
            [&](Literal const& s) {
                oss << literal_to_string(s, separator) << space;
            },
            [&](Array const& s) {
                for (auto const& value : s) {
                    oss << literal_to_string(value, separator) << space;
                }
            },
            [&](Expression::LValue const& s) { oss << s.first << space; },
            [&](Expression::Unary const& s) {
                oss << s.first
                    << expression_type_to_string(
                           s.second->value, true, separator)
                    << space;
            },
            [&](Expression::Relation const& s) {
                for (auto const& relation : s.second) {
                    oss << expression_type_to_string(
                               relation->value, true, separator)
                        << space;
                }
            },
            [&](Expression::Function const& s) {
                oss << s.first.first << space;
            },
            [&](Expression::Symbol const& s) {
                oss << s.first.first << space;
            } },
        item);
    return oss.str();
}

} // namespace credence::value::