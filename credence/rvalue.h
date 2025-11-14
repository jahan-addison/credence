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

#include <array>             // for array
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
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

namespace symbol {

/**
 * Symbolic type definitions
 */
using Label = std::string;
using Type = std::string;
using LValue = std::string;
using Labels = std::set<Label>;
using Size = std::size_t;
using RValue = std::string;
using Stack = std::deque<RValue>;
using Globals = type::Value_Pointer;
using Address = std::size_t;

/**
 * Algebraic type definitions
 */
using Data_Type = std::tuple<RValue, Type, Size>;
using RValue_Reference = std::string_view;
using Binary_Expression = std::tuple<std::string, std::string, std::string>;
using RValue_Reference_Type = std::variant<RValue, Data_Type>;
using Locals = Symbol_Table<Data_Type, LValue>;
using Temporary = std::pair<LValue, RValue>;
using Parameters = std::vector<std::string>;

// clang-format off
constexpr auto UNARY_TYPES = {
    "++", "--", "*", "&",
    "-", "+", "~", "!", "~"
};
constexpr Data_Type NULL_RVALUE_LITERAL =
    Data_Type{ "NULL", "null", sizeof(void*) };
constexpr auto integral_unary =
  { "int", "double", "float", "long" };
// clang-format on

/**
 * @brief Check if a symbol is in the symbol::Data_Type form
 */
constexpr bool is_rvalue_data_type(RValue const& rvalue)
{
    return util::substring_count_of(rvalue, ":") == 2 and
           rvalue.starts_with("(") and rvalue.ends_with(")");
}

/**
 * @brief Get a label as human readable object
 *    e.g. "__main(argc, argv)" -> "main"
 */
constexpr Label get_label_as_human_readable(Label const& label)
{
    if (util::contains(label, "("))
        return Label{ label.begin() + 2,
                      label.begin() + label.find_first_of("(") };
    else
        return label;
}

/**
 * @brief Get unary rvalue from ITA rvalue string
 */
constexpr RValue get_unary_rvalue_reference(
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
 * @brief Parse RValue::Value_Type string into a 3-tuple of value, type, and
 * size
 *
 * e.g. "(10:int:4)" -> 10, "int", 4UL
 */
inline Data_Type get_symbol_type_size_from_rvalue_string(RValue const& rvalue)
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
constexpr Binary_Expression from_rvalue_binary_expression(RValue const& rvalue)
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
 * @brief Get unary operator from ITA rvalue string
 */
constexpr RValue get_unary(RValue_Reference rvalue)
{
    auto it = std::ranges::find_if(UNARY_TYPES, [&](std::string_view op) {
        return rvalue.find(op) != std::string_view::npos;
    });
    if (it == UNARY_TYPES.end())
        return "";
    return *it;
}

/**
 * @brief Check if is unary
 */
constexpr bool is_unary(RValue_Reference rvalue)
{
    if (util::substring_count_of(rvalue, " ") >= 2)
        return false;
    return std::ranges::any_of(UNARY_TYPES, [&](std::string_view x) {
        return rvalue.starts_with(x) or rvalue.ends_with(x);
    });
}

/**
 * @brief Get the type from a local in the stack frame
 */
constexpr inline Type get_type_from_rvalue_data_type(Data_Type const& rvalue)
{
    return std::get<1>(rvalue);
}

/**
 * @brief Get the value from an rvalue data type object
 */
constexpr inline RValue get_value_from_rvalue_data_type(Data_Type const& rvalue)
{
    return std::get<0>(rvalue);
}

/**
 * @brief Get the lvalue from a vector or pointer offset
 *
 *   * v[19]        = v
 *   * sidno[errno] = sidno
 *
 */
constexpr inline RValue from_lvalue_offset(RValue_Reference rvalue)
{
    return std::string{ rvalue.begin(),
                        rvalue.begin() + rvalue.find_first_of("[") };
}

/**
 * @brief Get the integer or rvalue reference offset
 *   * v[20]        = 20
 *   * sidno[errno] = errno
 */
constexpr inline RValue from_pointer_offset(RValue_Reference rvalue)
{
    return std::string{ rvalue.begin() + rvalue.find_first_of("[") + 1,
                        rvalue.begin() + rvalue.find_first_of("]") };
}

/**
 * @brief Check if symbol is an expression with two symbol::Data_Types
 */
constexpr bool is_binary_rvalue_data_expression(RValue const& rvalue)
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

} // namespace symbol

/**
 * @brief
 * LL(1) top-down Parser of expression ast nodes to
 * type::RValue data structures.
 *
 * See types.h for details.
 */
class RValue_Parser
{

  public:
    RValue_Parser(RValue_Parser const&) = delete;
    RValue_Parser& operator=(RValue_Parser const&) = delete;

  public:
    explicit RValue_Parser(
        util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols = {})
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
    {
    }

    explicit RValue_Parser(
        util::AST_Node const& internal_symbols,
        Symbol_Table<> const& symbols,
        Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }

    ~RValue_Parser() = default;

  public:
    static inline type::RValue make_rvalue(
        util::AST_Node const& node,
        util::AST_Node const& internals,
        Symbol_Table<> const& symbols = {},
        Symbol_Table<> const& globals = {})
    {
        auto rvalue = RValue_Parser{ internals, symbols, globals };
        return rvalue.from_rvalue(node);
    }

  public:
    using Node = util::AST_Node;
    using Parameters = std::vector<type::RValue::RValue_Pointer>;
    type::RValue from_rvalue(Node const& node);

    inline type::RValue::RValue_Pointer shared_ptr_from_rvalue(Node const& node)
    {
        return std::make_shared<type::RValue>(from_rvalue(node));
    }

    inline type::RValue from_rvalue_expression(Node const& node)
    {
        return from_rvalue(node);
    }

  public:
    inline bool is_symbol(Node const& node)
    {
        auto lvalue = node["root"].to_string();
        return symbols_.is_defined(lvalue) or globals_.is_defined(lvalue);
    }

    inline bool is_defined(std::string const& label)
    {
        return internal_symbols_.has_key(label);
    }

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_evaluated_expression(Node const& node);
    type::RValue from_function_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_relation_expression(Node const& node);

  private:
    type::RValue from_ternary_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_unary_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue::LValue from_lvalue_expression(Node const& node);
    type::RValue::Value from_indirect_identifier(Node const& node);
    type::RValue::Value from_vector_idenfitier(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue from_assignment_expression(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::RValue::Value from_constant_expression(Node const& node);
    type::RValue::Value from_number_literal(Node const& node);
    type::RValue::Value from_string_literal(Node const& node);
    type::RValue::Value from_constant_literal(Node const& node);

  private:
    // clang-format on
    const std::array<std::string, 6> unary_types_ = {
        "pre_inc_dec_expression", "post_inc_dec_expression", "indirect_lvalue",
        "unary_indirection",      "address_of_expression",   "unary_expression"
    };
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

// clang-format on

inline type::RValue::RValue_Pointer make_rvalue(
    util::AST_Node const& node,
    util::AST_Node const& internals,
    Symbol_Table<> const& symbols = {},
    Symbol_Table<> const& globals = {})
{
    return std::make_shared<type::RValue>(
        RValue_Parser::make_rvalue(node, internals, symbols, globals));
}

} // namespace credence
