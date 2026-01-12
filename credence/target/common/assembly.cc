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

#include "assembly.h"
#include "types.h"          // for Immediate
#include <credence/error.h> // for credence_error
#include <credence/types.h> // for get_value_from_rvalue_data_type, get_typ...
#include <matchit.h>        // for match, MatchHelper
#include <string>           // for basic_string, operator==, char_traits
#include <string_view>      // for basic_string_view

/****************************************************************************
 *
 * Common assembly instruction representation and utilities
 *
 * Common abstractions for assembly instructions across x86-64 and ARM64.
 * Defines immediate values, operands, directives, and architecture/OS types.
 * Handles instruction formatting and operand representation.
 *
 * Example - emitting a comparison:
 *
 *   B code:    if (x > 10) { ... }  (x is local variable)
 *
 *   x86-64:    cmp eax, 10
 *              jg  .L1
 *
 *   ARM64:     cmp w9, #10          ; x in register x9
 *              b.gt .L1
 *
 ****************************************************************************/

namespace credence::target::common::assembly {

namespace m = matchit;

/**
 * @brief Compute the result from trivial relational expression
 */
Immediate get_result_from_trivial_relational_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    int result{ 0 };
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    auto lhs_type = type::get_type_from_rvalue_data_type(lhs);
    auto rhs_type = type::get_type_from_rvalue_data_type(lhs);

    m::match(op)(make_trivial_immediate_binary_result(==),
        make_trivial_immediate_binary_result(!=),
        make_trivial_immediate_binary_result(<),
        make_trivial_immediate_binary_result(>),
        make_trivial_immediate_binary_result(&&),
        make_trivial_immediate_binary_result(||),
        make_trivial_immediate_binary_result(<=),
        make_trivial_immediate_binary_result(>=));

    return make_numeric_immediate(result, "byte");
}

Immediate get_result_from_trivial_integral_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result = trivial_arithmetic_from_numeric_table_type<int>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result = trivial_arithmetic_from_numeric_table_type<long>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<long>(result, "long");
    } else if (type == "float") {
        auto result = trivial_arithmetic_from_numeric_table_type<float>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<float>(result, "float");

    } else if (type == "double") {
        auto result = trivial_arithmetic_from_numeric_table_type<double>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<double>(result, "double");
    }
    credence_error("unreachable");
    return make_numeric_immediate(0, "int");
}

/**
 * @brief Compute the result from trivial bitwise expression
 */
Immediate get_result_from_trivial_bitwise_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result =
            trivial_bitwise_from_numeric_table_type<int>(lhs_imm, op, rhs_imm);
        return make_numeric_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result =
            trivial_bitwise_from_numeric_table_type<long>(lhs_imm, op, rhs_imm);
        return make_numeric_immediate<long>(result, "long");
    }
    credence_error("unreachable");
    return make_numeric_immediate(0, "int");
}

}