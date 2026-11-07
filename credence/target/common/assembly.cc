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
 * Common Assembly
 *
 * The instruction representation shared by the x86-64 and ARM64 targets:
 * immediates, operands, the instruction inserter, the OS and ISA types, and
 * compile-time evaluation of expressions of two immediates.
 *
 *  Example:
 *
 *    _t5 = x > (10:int:4);
 *    IF _t5 GOTO _L4;
 *
 *  Becomes, on x86-64:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    cmp eax, 10
 *    jg ._L4__main
 *
 *  And on arm64, where x is in w9:
 *
 *    mov w8, w9
 *    cmp w8, #10
 *    b.gt ._L4__main
 *
 ****************************************************************************/

namespace credence::target::common::assembly {

namespace m = matchit;

/**
 * @brief Compute a relational or logical expression of two immediates
 *
 * The result is 1 or 0, so a comparison of two literals is folded into a
 * `mov` of its result
 *
 *  Examples:
 *
 *    (1:int:4) < (2:int:4)                    // (1:byte:4)
 *    ("ab":string:2) == ("ab":string:2)       // (1:byte:4)
 *    (0:int:4) || (1:int:4)                   // (1:byte:4)
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

/**
 * @brief Compute an arithmetic expression of two immediates
 *
 * The result has the type of the left-hand-side, so an `int` expression is
 * computed with `int` arithmetic
 *
 *  Examples:
 *
 *    (10:int:4) % (3:int:4)     // (1:int:4)
 *    (10:int:4) - (4:int:4)     // (6:int:4)
 */
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
 * @brief Compute a bitwise expression of two integer immediates
 *
 *  Example:
 *
 *    (5:int:4) & (3:int:4)   // (1:int:4)
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