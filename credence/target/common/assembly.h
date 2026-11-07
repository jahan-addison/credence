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
#include <cmath>            // for fmod
#include <credence/types.h> // for integral_from_type, get_value_f...
#include <credence/util.h>  // for STRINGIFY, sv, Numeric, to_cons...
#include <fmt/format.h>     // for format
#include <ostream>          // for operator<<, basic_ostream
#include <sstream>          // for basic_ostringstream, basic_ios
#include <string>           // for basic_string, string, char_traits
#include <string_view>      // for basic_string_view, string_view
#include <type_traits>      // for is_integral_v
#include <variant>          // for monostate, visit

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

/**
 * The entries of a match on the type of the left-hand-side immediate of a
 * relational expression, so a comparison is made on the C++ type of the
 * immediates. A string is compared by its contents, and a char by its code.
 */

#define make_integral_relational_entry(T, op)                   \
    m::pattern | std::string{ STRINGIFY(T) } = [&] {            \
        result = credence::type::integral_from_type<T>(lhs_imm) \
            op credence::type::integral_from_type<T>(rhs_imm);  \
    }

#define make_string_relational_entry(op)              \
    m::pattern | std::string{ "string" } = [&] {      \
        result = std::string_view{ lhs_imm }.compare( \
            std::string_view{ rhs_imm }) op 0;        \
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

/**
 * Short-form immediates of the instruction inserters. A direct immediate is
 * emitted as is, such as a label or an address operand, `[sp, #16]`.
 */
#define direct_immediate(imm) \
    target::common::assembly::make_direct_immediate(imm)

#define u32_int_immediate(i) target::common::assembly::make_u32_int_immediate(i)

#define alignment__integer() target::common::assembly::make_u32_int_immediate(0)

#define alignment__sp_integer(i) direct_immediate(fmt::format("[sp, #{}]", i))

#define alignment__16_integer() \
    target::common::assembly::make_u32_int_immediate(16)

/**
 * The cases of a switch that prints a register or directive by its
 * enumerator name. A trailing underscore avoids a C++ keyword, `float_`, and
 * is removed, and the `start` directive is printed as `.global`.
 */
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

namespace credence::target::common::assembly {

namespace detail {

/**
 * @brief The mnemonic of an instruction, implicit from a mnemonic enumerator
 */
template<Enum_T Mnemonic>
struct Mnemonic_Resolver
{
    Mnemonic value;
    // cppcheck-suppress noExplicitConstructor
    constexpr Mnemonic_Resolver(Mnemonic m)
        : value(m)
    {
    }
};

/**
 * @brief An operand of an instruction, implicit from any storage device
 *
 * A register, stack offset, immediate, or empty operand converts to the
 * storage variant, so an inserter passes each operand as it is
 */
template<Enum_T Register>
struct Operand_Resolver
{
    using Storage = Storage_T<Register>;
    Storage value;

    // cppcheck-suppress noExplicitConstructor
    constexpr Operand_Resolver(Register r)
        : value(r)
    {
    }

    // cppcheck-suppress noExplicitConstructor
    constexpr Operand_Resolver(common::Stack_Offset o)
        : value(o)
    {
    }
    // cppcheck-suppress noExplicitConstructor
    constexpr Operand_Resolver(Immediate const& imm)
        : value(imm)
    {
    }
    // cppcheck-suppress noExplicitConstructor
    constexpr Operand_Resolver(std::monostate)
        : value(std::monostate{})
    {
    }
    // cppcheck-suppress noExplicitConstructor
    constexpr Operand_Resolver(Storage const& storage)
        : value(storage)
    {
        std::visit(util::overload{
                       [&](std::monostate) {},
                       [&](common::Stack_Offset const& s) { value = s; },
                       [&](Register const& s) { value = s; },
                       [&](Immediate const& s) { value = s; },
                   },
            storage);
    }
};

} // namespace detail

/**
 * @brief Append an instruction of two, three, or four operands
 *
 * Each target wraps it in a short-form macro, and the instruction tuple
 * type `T` is chosen by the number of operands
 *
 *  Example:
 *
 *    arm64_add__asm(instructions, add, w8, w8, w7);
 *
 *  Becomes:
 *
 *    add w8, w8, w7
 */
template<Enum_T Mnemonic, Enum_T Register, Deque_T Instructions>
struct Assembly_Inserter
{
    Assembly_Inserter() = delete;

    enum class nary
    {
        ary_2,
        ary_3,
        ary_4
    };

    using Op = detail::Mnemonic_Resolver<Mnemonic>;

    template<typename T, nary Size>
    static void insert(Instructions& inst,
        Op op,
        detail::Operand_Resolver<Register> s0 = std::monostate{},
        detail::Operand_Resolver<Register> s1 = std::monostate{},
        detail::Operand_Resolver<Register> s2 = std::monostate{},
        detail::Operand_Resolver<Register> s3 = std::monostate{})
    {
        if constexpr (Size == nary::ary_2) {
            inst.emplace_back(T{ op.value, s0.value, s1.value });
        } else if constexpr (Size == nary::ary_3) {
            inst.emplace_back(T{ op.value, s0.value, s1.value, s2.value });
        } else if constexpr (Size == nary::ary_4) {
            inst.emplace_back(
                T{ op.value, s0.value, s1.value, s2.value, s3.value });
        }
    }
};

/**
 * @brief The operating system of the target, BSD is Darwin
 */
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

/**
 * @brief The operating system of the host, which is the default target
 *
 * Windows is not supported, and its syscall table is Linux
 */
constexpr OS_Type get_os_type()
{
#if defined(__linux__)
    return OS_Type::Linux;
#elif defined(__APPLE__) || defined(__bsdi__)
    return OS_Type::BSD;
#elif defined(_WIN32) || defined(_WIN64)
    return OS_Type::Linux;
#else
    return OS_Type::Linux;
#endif
}

/**
 * @brief An immediate operand of a directive of an array in the data section
 */
inline Immediate make_array_immediate(std::string_view address)
{
    return Immediate{ address, "string", 8UL };
}

/**
 * @brief An immediate that is emitted as is, such as a label or address
 *
 *  Example:
 *
 *    make_direct_immediate("[sp, #16]")   // [sp, #16]
 */
inline Immediate make_direct_immediate(std::string_view str)
{
    return Immediate{ str, "string", 8UL };
}

/**
 * @brief Get a storage device as a string, for errors and debugging
 */
template<Enum_T Registers>
std::string get_storage_as_string(Storage_T<Registers> const& storage)
{
    std::ostringstream result{};
    std::visit(util::overload{
                   [&](std::monostate) { result << "(empty)"; },
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
 * @brief Compute an arithmetic expression of two immediates of type T
 *
 * The remainder of a float or double is fmod
 *
 *  Example:
 *
 *    trivial_arithmetic_from_numeric_table_type<int>("10", "%", "3")   // 1
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
        case '%':
            if constexpr (std::is_integral_v<T>)
                return imm_l % imm_r;
            else
                return std::fmod(imm_l, imm_r);
    }
    return result;
}

/**
 * @brief Compute a bitwise expression of two integer immediates of type T
 *
 *  Example:
 *
 *    trivial_bitwise_from_numeric_table_type<int>("5", "<<", "3")   // 40
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
 * @brief Construct a numeric immediate of a type, e.g. `(5:int:4)`
 */
template<util::Numeric T>
constexpr Immediate make_numeric_immediate(T imm,
    std::string const& type = "int")
{
    return Immediate{ util::to_constexpr_string(imm), type, 4UL };
}

/**
 * @brief Construct an unsigned immediate, such as a stack offset or size
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

} // credence::target::common::assembly