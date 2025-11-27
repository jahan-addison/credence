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

#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <credence/value.h>  // for Expression
#include <deque>             // for deque
#include <initializer_list>  // for initializer_list
#include <memory>            // for allocator, make_shared
#include <set>               // for set
#include <simplejson.h>      // for JSON
#include <string>            // for basic_string, string
#include <string_view>       // for string_view
#include <tuple>             // for tuple
#include <variant>           // for variant
#include <vector>            // for vector

namespace credence {

namespace type {

namespace semantic {

/**
 * Semantic type definitions
 */
using Label = std::string;
using Type = std::string;
using LValue = std::string;
using Size = std::size_t;
using RValue = std::string;
using Address = std::size_t;

} // namespace semantic

/**
 * Algebraic type definitions
 */
using Data_Type = std::tuple<semantic::RValue, semantic::Type, semantic::Size>;
using RValue_Reference = std::string_view;
using Stack = std::deque<semantic::RValue>;
using Labels = std::set<semantic::Label>;
using Globals = internal::value::Array;
using Binary_Expression = std::tuple<std::string, std::string, std::string>;
using RValue_Reference_Type = std::variant<semantic::RValue, Data_Type>;
using Locals = Symbol_Table<Data_Type, semantic::LValue>;
using Temporary = std::pair<semantic::LValue, semantic::RValue>;
using Parameters = std::vector<std::string>;

// clang-format off
constexpr auto unary_operators =
    { "++", "--", "*", "&", "-", "+", "~", "!" };
constexpr auto arithmetic_unary_operators =
    { "++", "--", "-", "+" };
constexpr auto arithmetic_binary_operators =
    { "*", "/", "-", "+", "%" };
constexpr auto bitwise_binary_operators =
    { "<<", ">>", "|", "^", "&" };
constexpr auto relation_binary_operators =
    { "==", "!=", "<",  "&&",
      "||", ">",  "<=", ">=" };
constexpr auto integral_unary_types =
    { "int", "double", "float", "long" };

constexpr Data_Type NULL_RVALUE_LITERAL =
    Data_Type{ "NULL", "null", sizeof(void*) };

// clang-format on

inline int integral_from_type_int(std::string const& t)
{
    return std::stoi(t);
}

inline long integral_from_type_long(std::string const& t)
{
    return std::stol(t);
}

inline float integral_from_type_float(std::string const& t)
{
    return std::stof(t);
}

inline double integral_from_type_double(std::string const& t)
{
    return std::stod(t);
}

template<typename T>
T integral_from_type(std::string const& t)
{
    if constexpr (std::is_same_v<T, int>) {
        return integral_from_type_int(t);
    } else if constexpr (std::is_same_v<T, long>) {
        return integral_from_type_long(t);
    } else if constexpr (std::is_same_v<T, float>) {
        return integral_from_type_float(t);
    } else if constexpr (std::is_same_v<T, double>) {
        return integral_from_type_double(t);
    } else {
        return integral_from_type_int(t);
    }
}

/**
 * @brief Check if expression contains arithmetic expression
 */
constexpr bool is_binary_arithmetic_expression(semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        arithmetic_binary_operators.begin(),
        arithmetic_binary_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != arithmetic_binary_operators.end();
}

constexpr bool is_binary_arithmetic_operator(RValue_Reference rvalue)
{
    auto test = std::ranges::find_if(
        arithmetic_binary_operators.begin(),
        arithmetic_binary_operators.end(),
        [&](std::string_view s) { return rvalue == s; });
    return test != arithmetic_binary_operators.end();
}

constexpr bool is_unary_arithmetic_operator(RValue_Reference rvalue)
{
    auto test = std::ranges::find_if(
        arithmetic_unary_operators.begin(),
        arithmetic_unary_operators.end(),
        [&](std::string_view s) { return rvalue == s; });
    return test != arithmetic_unary_operators.end();
}

/**
 * @brief Check if expression contains bitwise expression
 */
constexpr bool is_bitwise_binary_expression(semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        bitwise_binary_operators.begin(),
        bitwise_binary_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != bitwise_binary_operators.end();
}

constexpr bool is_bitwise_binary_operator(RValue_Reference rvalue)
{
    auto test = std::ranges::find_if(
        bitwise_binary_operators.begin(),
        bitwise_binary_operators.end(),
        [&](std::string_view s) { return rvalue == s; });
    return test != bitwise_binary_operators.end();
}

/**
 * @brief Check if expression contains relational expression
 */
constexpr bool is_relation_binary_expression(semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        relation_binary_operators.begin(),
        relation_binary_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != relation_binary_operators.end();
}

constexpr bool is_relation_binary_operator(RValue_Reference rvalue)
{
    auto test = std::ranges::find_if(
        relation_binary_operators.begin(),
        relation_binary_operators.end(),
        [&](std::string_view s) { return rvalue == s; });
    return test != relation_binary_operators.end();
}

/**
 * @brief Check if a symbol is in the symbol::Data_Type form
 */
constexpr bool is_rvalue_data_type(semantic::RValue const& rvalue)
{
    return util::substring_count_of(rvalue, ":") == 2 and
           rvalue.starts_with("(") and rvalue.ends_with(")");
}

/**
 * @brief Data type tuple to string
 */
inline std::string data_type_value_to_string(Data_Type const& value)
{
    return std::format(
        "({}:{}:{})",
        std::get<0>(value),
        std::get<1>(value),
        std::get<2>(value));
}

/**
 * @brief Get a label as human readable object
 *    e.g. "__main(argc, argv)" -> "main"
 */
constexpr semantic::Label get_label_as_human_readable(
    semantic::Label const& label)
{
    if (util::contains(label, "("))
        return semantic::Label{ label.begin() + 2,
                                label.begin() + label.find_first_of("(") };
    else
        return label;
}

/**
 * @brief Get unary rvalue from ITA rvalue string
 */
constexpr semantic::RValue get_unary_rvalue_reference(
    RValue_Reference rvalue,
    std::string_view unary_chracters = "+-*&+~!")
{
    auto lvalue = std::string{ rvalue.begin(), rvalue.end() };
    lvalue.erase(
        std::remove_if(
            lvalue.begin(),
            lvalue.end(),
            [&](char ch) {
                return ::isspace(ch) or
                       unary_chracters.find(ch) != std::string_view::npos;
            }),
        lvalue.end());
    return lvalue;
}

/**
 * @brief Get unary operator from ITA rvalue string
 */
constexpr semantic::RValue get_unary_operator(RValue_Reference rvalue)
{
    auto it = std::ranges::find_if(unary_operators, [&](std::string_view op) {
        return rvalue.find(op) != std::string_view::npos;
    });
    if (it == unary_operators.end())
        return "";
    return *it;
}

/**
 * @brief Check if an expression contains unary operator
 */
constexpr bool is_unary_expression(RValue_Reference rvalue)
{
    if (util::substring_count_of(rvalue, " ") >= 2)
        return false;
    return std::ranges::any_of(unary_operators, [&](std::string_view x) {
        return rvalue.starts_with(x) or rvalue.ends_with(x);
    });
}

/**
 * @brief Check if an expression contains unary operator
 */
constexpr bool is_dereference_expression(RValue_Reference rvalue)
{
    if (util::substring_count_of(rvalue, " ") >= 2)
        return false;
    return get_unary_operator(rvalue) == "*";
}

/**
 * @brief Check if an expression contains binary operators
 */
constexpr bool is_binary_expression(semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    return is_binary_arithmetic_expression(rvalue) or
           is_relation_binary_expression(rvalue) or
           is_bitwise_binary_expression(rvalue);
}

/**
 * @brief Check if an operator is binary operator
 */
constexpr bool is_binary_operator(semantic::RValue const& op)
{
    return is_binary_arithmetic_operator(op) or
           is_relation_binary_operator(op) or is_bitwise_binary_operator(op);
}

/**
 * @brief Check if symbol is a temporary (i.e. "_t1" )
 */
constexpr bool is_temporary(RValue_Reference rvalue)
{
    for (std::size_t i = 0; i < rvalue.size(); i++) {
        const auto c = rvalue[i];
        if (i == 0 and c != '_')
            return false;
        if (i == 1 and c != 't')
            return false;
        if (i > 1 and not std::isdigit(c))
            return false;
    }
    return true;
}

/**
 * @brief Parse semantic::RValue::Value_Type string into a 3-tuple of value,
 * type, and size
 *
 * e.g. "(10:int:4)" -> 10, "int", 4UL
 */
inline Data_Type get_rvalue_datatype_from_string(semantic::RValue const& rvalue)
{
    CREDENCE_ASSERT(util::substring_count_of(rvalue, ":") == 2);
    size_t search = rvalue.find_last_of(":");
    auto bytes = std::string{ rvalue.begin() + search + 1, rvalue.end() - 1 };
    auto type_search = rvalue.substr(0, search - 1).find_last_of(":") + 1;
    auto type =
        std::string{ rvalue.begin() + type_search, rvalue.begin() + search };

    auto value = std::ranges::find(rvalue, '"') == rvalue.end()
                     ? rvalue.substr(1, type_search - 2)
                     : rvalue.substr(2, type_search - 4);

    return { value, type, std::stoul(bytes) };
}

/**
 * @brief Parse ITA binary expression into its operator and operands in the
 * table
 */
constexpr Binary_Expression from_rvalue_binary_expression(
    semantic::RValue const& rvalue)
{
    auto lhs = rvalue.find_first_of(" ");
    auto rhs = rvalue.find_last_of(" ");
    auto lhs_lvalue = std::string{ rvalue.begin(), rvalue.begin() + lhs };
    auto rhs_lvalue = std::string{ rvalue.begin() + 1 + rhs, rvalue.end() };
    auto binary_operator =
        std::string{ rvalue.begin() + lhs + 1, rvalue.begin() + rhs };

    return { lhs_lvalue, rhs_lvalue, binary_operator };
}

/**
 * @brief Get binary operator from ITA rvalue string
 */
constexpr semantic::RValue get_binary_operator(RValue_Reference rvalue)
{
    semantic::RValue op{};
    auto it = std::ranges::find_if(
        arithmetic_binary_operators, [&](std::string_view op) {
            return rvalue.find(op) != std::string_view::npos;
        });
    if (it != arithmetic_binary_operators.end())
        op = *it;
    it = std::ranges::find_if(
        relation_binary_operators, [&](std::string_view op) {
            return rvalue.find(op) != std::string_view::npos;
        });
    if (it != relation_binary_operators.end())
        op = *it;
    it = std::ranges::find_if(
        bitwise_binary_operators, [&](std::string_view op) {
            return rvalue.find(op) != std::string_view::npos;
        });
    if (it != bitwise_binary_operators.end())
        op = *it;

    return op;
}

/**
 * @brief Get the type from a local in the stack frame
 */
constexpr semantic::Type get_type_from_rvalue_data_type(Data_Type const& rvalue)
{
    return std::get<1>(rvalue);
}

/**
 * @brief Get the value from an rvalue data type object
 */
constexpr semantic::RValue get_value_from_rvalue_data_type(
    Data_Type const& rvalue)
{
    return std::get<0>(rvalue);
}

/**
 * @brief Get the type from a local in the stack frame
 */
constexpr semantic::Size get_size_from_rvalue_data_type(Data_Type const& rvalue)
{
    return std::get<2>(rvalue);
}

/**
 * @brief Get the lvalue from a vector or pointer offset
 *
 *   * v[19]        = v
 *   * sidno[errno] = sidno
 *
 */
constexpr semantic::RValue from_lvalue_offset(RValue_Reference rvalue)
{
    return std::string{ rvalue.begin(),
                        rvalue.begin() + rvalue.find_first_of("[") };
}

/**
 * @brief Get the integer or rvalue reference offset
 *   * v[20]        = 20
 *   * sidno[errno] = errno
 */
constexpr semantic::RValue from_pointer_offset(RValue_Reference rvalue)
{
    return std::string{ rvalue.begin() + rvalue.find_first_of("[") + 1,
                        rvalue.begin() + rvalue.find_first_of("]") };
}

/**
 * @brief Check if symbol is an expression with two symbol::Data_Types
 */
constexpr bool is_binary_datatype_expression(semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = from_rvalue_binary_expression(rvalue);
    if (!is_rvalue_data_type(std::get<0>(test)))
        return false;
    if (!is_rvalue_data_type(std::get<1>(test)))
        return false;
    return true;
}

/**
 * @brief Check if symbol is an expression with two temporaries
 */
constexpr bool is_temporary_datatype_binary_expression(
    semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = from_rvalue_binary_expression(rvalue);
    if (!is_temporary(std::get<0>(test)))
        return false;
    if (!is_temporary(std::get<1>(test)))
        return false;
    return true;
}

/**
 * @brief Check if an operand is a temporary lvalue
 */
constexpr std::string is_temporary_operand_binary_expression(
    semantic::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return std::string{};
    auto test = from_rvalue_binary_expression(rvalue);
    if (is_temporary(std::get<0>(test)))
        return "left";
    if (is_temporary(std::get<1>(test)))
        return "right";

    return std::string{};
}

} // namespace type

} // namespace credence