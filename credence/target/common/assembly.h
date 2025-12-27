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

#include "matchit.h"        // for pattern, PatternHelper, Pattern...
#include "types.h"          // for Immediate, Storage_T, Enum_T
#include <credence/types.h> // for integral_from_type, get_value_f...
#include <credence/util.h>  // for STRINGIFY, sv, Numeric, to_cons...
#include <fmt/format.h>     // for format
#include <ostream>          // for operator<<, basic_ostream
#include <sstream>          // for basic_ostringstream, basic_ios
#include <string>           // for basic_string, string, char_traits
#include <string_view>      // for basic_string_view, string_view
#include <variant>          // for monostate, visit

// -----------------------------
// Immediate relational utilities
// -----------------------------
// These macros rely on matchit (namespace m::) being available in the includer.

#define make_integral_relational_entry(T, op)                   \
    m::pattern | std::string{ STRINGIFY(T) } = [&] {            \
        result = credence::type::integral_from_type<T>(lhs_imm) \
            op credence::type::integral_from_type<T>(rhs_imm);  \
    }

#define make_string_relational_entry(op)                                      \
    m::pattern | std::string{ "string" } = [&] {                              \
        result =                                                              \
            std::string_view{ lhs_imm }.compare(std::string_view{ rhs_imm }); \
    }

#define make_char_relational_entry(op)                                    \
    m::pattern | std::string{ "char" } = [&] {                            \
        result = static_cast<int>(static_cast<unsigned char>(lhs_imm[1])) \
            op static_cast<int>(static_cast<unsigned char>(rhs_imm[1]));  \
    }

#define make_trivial_immediate_binary_result(op)                    \
    m::pattern | std::string{ STRINGIFY(op) } = [&] {               \
        m::match(lhs_type)(make_integral_relational_entry(int, op), \
            make_integral_relational_entry(long, op),               \
            make_integral_relational_entry(float, op),              \
            make_integral_relational_entry(double, op),             \
            make_string_relational_entry(op),                       \
            make_char_relational_entry(op));                        \
    }

// ---------------------------------
// Register and directive print macros
// ---------------------------------
// These macros expect rr/dd helpers to be provided by the includer, e.g.
// x64_rr/arm_rr.

#define COMMON_REGISTER_OSTREAM(rr, reg) \
    case rr(reg):                        \
        os << STRINGIFY(reg);            \
        break

#define COMMON_REGISTER_STRING(rr, reg) \
    case rr(reg):                       \
        return STRINGIFY(reg);          \
        break

#define COMMON_DIRECTIVE_OSTREAM(dd, d)        \
    case dd(d): {                              \
        auto di = sv(STRINGIFY(d));            \
        if (credence::util::contains(di, "_")) \
            di.remove_suffix(1);               \
        os << "." << di;                       \
    }; break

#define COMMON_DIRECTIVE_OSTREAM_2ARY(dd, d, g)                            \
    case dd(d): {                                                          \
        auto di =                                                          \
            sv(STRINGIFY(d)) == sv("start") ? "global" : sv(STRINGIFY(d)); \
        if (credence::util::contains(di, "_"))                             \
            di.remove_suffix(1);                                           \
        os << fmt::format(".{} {}", di, STRINGIFY(g));                     \
    }; break

// ---------------------------------
// Generic instruction insertion helpers
// ---------------------------------
// Two-operand architectures (e.g., x86_64)

#define COMMON_ASM_ZERO_O_2(InstructionType, MnemonicType, ONUL, inst, op) \
    inst.emplace_back(InstructionType{ MnemonicType::op, ONUL, ONUL })

#define COMMON_ASM_DEST_2(InstructionType, MnemonicType, ONUL, inst, op, dest) \
    inst.emplace_back(InstructionType{ MnemonicType::op, dest, ONUL })

#define COMMON_ASM_DEST_S_2(                                           \
    RegisterType, InstructionType, MnemonicType, ONUL, inst, op, dest) \
    inst.emplace_back(                                                 \
        InstructionType{ MnemonicType::op, RegisterType::dest, ONUL })

#define COMMON_ASM_SRC_2(InstructionType, MnemonicType, ONUL, inst, op, src) \
    inst.emplace_back(InstructionType{ MnemonicType::op, src, ONUL })

// Three-operand architectures (e.g., ARM64)

#define COMMON_ASM_ZERO_O_3(InstructionType, MnemonicType, ONUL, inst, op) \
    inst.emplace_back(InstructionType{ MnemonicType::op, ONUL, ONUL, ONUL })

#define COMMON_ASM_DEST_3(InstructionType, MnemonicType, ONUL, inst, op, dest) \
    inst.emplace_back(InstructionType{ MnemonicType::op, dest, ONUL, ONUL })

#define COMMON_ASM_DEST_S_3(                                           \
    RegisterType, InstructionType, MnemonicType, ONUL, inst, op, dest) \
    inst.emplace_back(                                                 \
        InstructionType{ MnemonicType::op, RegisterType::dest, ONUL, ONUL })

#define COMMON_ASM_SRC_3(InstructionType, MnemonicType, ONUL, inst, op, src) \
    inst.emplace_back(InstructionType{ MnemonicType::op, src, ONUL, ONUL })

namespace credence::target::common::assembly {

enum class OS_Type
{
    Linux,
    BSD
};

enum class Arch_Type
{
    ARM64,
    X8664
};

constexpr OS_Type get_os_type()
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    return OS_Type::Linux;

#elif defined(__APPLE__) || defined(__bsdi__)
    return OS_Type::BSD;
#endif
}

/**
 * @brief directive insertion helper for arrays
 */
inline Immediate make_array_immediate(std::string_view address)
{
    return Immediate{ address, "string", 8UL };
}

/**
 * @brief directive insertion for arbitrary immediate devices
 */
inline Immediate make_direct_immediate(std::string_view str)
{
    return Immediate{ str, "string", 8UL };
}

/**
 * @brief Get a storage device as string for emission
 */
template<Enum_T Registers>
std::string get_storage_as_string(Storage_T<Registers> const& storage)
{
    std::ostringstream result{};
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](common::Stack_Offset const& s) {
                       result << fmt::format("stack offset: {}", s);
                   },
                   [&](Registers const& s) { result << s; },
                   [&](Immediate const& s) {
                       result << type::get_value_from_rvalue_data_type(s);
                   },
               },
        storage);
    return result.str();
}

/**
 * @brief
 *  Template function to compute type-safe trivial arithmetic binary
 * expression
 */

template<util::Numeric T>
T trivial_arithmetic_from_numeric_table_type(std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T result{ 0 };
    T imm_l = type::integral_from_type<T>(lhs);
    T imm_r = type::integral_from_type<T>(rhs);
    switch (op[0]) {
        case '+':
            return imm_l + imm_r;
        case '-':
            return imm_l - imm_r;
        case '*':
            return imm_l * imm_r;
        case '/':
            return imm_l / imm_r;
    }
    return result;
}

/**
 * @brief
 *  Template function to compute type-safe trivial bitwise binary
 * expression
 */
template<util::Numeric T>
T trivial_bitwise_from_numeric_table_type(std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T imm_l = type::integral_from_type<T>(lhs);
    T imm_r = type::integral_from_type<T>(rhs);
    if (op == ">>")
        return imm_l >> imm_r;
    else if (op == "<<")
        return imm_l << imm_r;
    else
        switch (op[0]) {
            case '^':
                return imm_l ^ imm_r;
            case '&':
                return imm_l & imm_r;
            case '|':
                return imm_l | imm_r;
        }
    return 0;
}

/**
 * @brief Type-safe numeric type::Data_Type immediate constructor
 */
template<util::Numeric T>
constexpr Immediate make_numeric_immediate(T imm,
    std::string const& type = "int")
{
    return Immediate{ util::to_constexpr_string(imm), type, 4UL };
}

/**
 * @brief Type-safe numeric u32 type::Data_Type immediate constructor
 */
constexpr Immediate make_u32_int_immediate(unsigned int imm)
{
    return Immediate{ util::to_constexpr_string(imm), "int", 4UL };
}

Immediate get_result_from_trivial_integral_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_relational_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_bitwise_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);

/**
 * @brief Compute the result from trivial integral expression
 */
Immediate get_result_from_trivial_integral_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);

} // credence::target::common::assembly