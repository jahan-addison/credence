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