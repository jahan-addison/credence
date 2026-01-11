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
 * Compiler error handling and diagnostics
 *
 * Provides compile-time error detection and reporting with clear messages.
 * Catches semantic errors, type mismatches, out-of-bounds array access,
 * undefined symbols, and other programming errors before code generation.
 *
 * Example - compile-time boundary checking:
 *
 *   main() {
 *     extrn values;
 *     print(values[10]);  // Error: array 'values' has size 3
 *   }
 *   values [3] 1, 2, 3;
 *
 *   Output: "Out of range: index 10 exceeds array size 3"
 *
 * Example - type mismatch:
 *
 *   add(x, y) { return(x + y); }
 *
 *   main() {
 *     add(5);  // Error: function 'add' expects 2 arguments, got 1
 *   }
 *
 * Example - undefined symbol:
 *
 *   main() {
 *     auto x;
 *     x = unknown_var;  // Error: undefined symbol 'unknown_var'
 *   }
 *
 * All errors include source location (file, line, column) for easy debugging.
 *
 *****************************************************************************/

#pragma once

#include <credence/util.h>
#include <easyjson.h>
#include <exception>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <source_location>
#include <string_view>
#include <type_traits>

#define credence_error(message) \
    (credence::detail::assert_fail(std::source_location::current(), message))

#define credence_compile_error(location, message, symbol, symbols) \
    (credence::detail::compile_error_impl(location, message, symbol, symbols))

#define credence_compile_error_with_type(     \
    location, message, symbol, symbols, type) \
    (credence::detail::compile_error_impl(    \
        location, message, symbol, symbols, type))

#define credence_assert(condition) \
    (credence::detail::assert_impl(std::source_location::current(), condition))

#define credence_assert_message(condition, message) \
    (credence::detail::assert_impl(                 \
        std::source_location::current(), condition, message))

#define credence_assert_message_trace(condition, message, location) \
    (credence::detail::assert_impl(location, condition, message))

#define credence_assert_equal(actual, expected) \
    (credence::detail::assert_equal_impl(       \
        std::source_location::current(), actual, expected))

#define credence_assert_nequal(actual, expected) \
    (credence::detail::assert_nequal_impl(       \
        std::source_location::current(), actual, expected))

namespace credence {

namespace detail {

class Credence_Exception : public std::exception
{
    std::string message_;

  public:
    template<typename... Args>
    explicit constexpr Credence_Exception(
        fmt::format_string<Args...> fmt_string,
        Args&&... args)
        : message_{ fmt::format(fmt_string, std::forward<Args>(args)...) }
    {
    }

    [[nodiscard]] const char* what() const noexcept override
    {
        return message_.c_str();
    }
};

inline void compile_error_impl(
    [[maybe_unused]] std::source_location const& location,
    std::string_view message,
    std::string_view symbol_name,
    easyjson::JSON const& symbols,
    std::string_view type = "symbol")
{
    auto symbol = symbol_name.data();
#ifndef DEBUG
    if (symbols.has_key(symbol) and type == "symbol")
        throw Credence_Exception("\n  Credence could not compile "
                                 "source:\n    on {} '{}'\n    "
                                 "with: "
                                 "\"{}\"\n  > from line {} column {}:{}",
            type,
            symbol,
            message,
            symbols[symbol]["line"].to_int(),
            symbols[symbol]["column"].to_int(),
            symbols[symbol]["end_column"].to_int());
    else
        throw Credence_Exception("\n  Credence could not compile "
                                 "source:\n    on {} '{}'\n    "
                                 "with: {}",
            type,
            symbol,
            message);
#else
    if (symbols.has_key(symbol) and type == "symbol")
        throw Credence_Exception(
            "\n  Credence could not compile source:\n    on {} '{}'\n "
            "   "
            "with: "
            "\"{}\"\n  > from line {} column {}:{}\n\n\n >>> In file "
            "'{}'\n line {}\n   ::: '{}'\n",
            type,
            symbol,
            message,
            symbols[symbol]["line"].to_int(),
            symbols[symbol]["column"].to_int(),
            symbols[symbol]["end_column"].to_int(),
            location.file_name(),
            location.line(),
            location.function_name());
    else
        throw Credence_Exception("\n  Credence could not compile "
                                 "source:\n    on {} '{}'\n    "
                                 "with: \"{}\""
                                 "\n\n\n >>> In file "
                                 "'{}'\n line {}\n   ::: '{}'\n",
            type,
            symbol,
            message,
            location.file_name(),
            location.line(),
            location.function_name());
#endif
}

inline void assert_fail(std::source_location const& location,
    std::string_view message = "")
{
    if (message.empty()) {
        throw Credence_Exception(
            "\n    Assertion failed in '{}'\n    at line {}\n  ::: "
            "'{}'\n",
            location.file_name(),
            location.line(),
            location.function_name());
    } else {
        throw Credence_Exception(
            "\n    Assertion failed in '{}'\n    at line {}\n  ::: "
            "'{}'\n    "
            "with "
            "'{}'\n",
            location.file_name(),
            location.line(),
            location.function_name(),
            message);
    }
}

inline void assert_impl(std::source_location const& location,
    bool condition,
    std::string_view message = "")
{
    if (!condition) {
        if (message.empty()) {
            throw Credence_Exception(
                "\n    Assertion Failed in '{}'\n    at line {} "
                "\n  ::: '{}'\n",
                location.file_name(),
                location.line(),
                location.function_name());
        } else {
            throw Credence_Exception(
                "\n    Assertion failed in '{}'\n    at line {} "
                "\n  ::: '{}': \n   "
                " "
                "with "
                "'{}'\n",
                location.file_name(),
                location.line(),
                location.function_name(),
                message);
        }
    }
}

template<typename T1, typename T2>
constexpr inline void assert_equal_impl(std::source_location const& location,
    const T1& actual,
    const T2& expected)
{
    assert_impl(location,
        actual == expected,
        fmt::format("expected '{}' to equal '{}'",
            util::to_constexpr_string(actual),
            util::to_constexpr_string(expected)));
}

template<typename T1, typename T2>
constexpr inline void assert_nequal_impl(std::source_location const& location,
    const T1& actual,
    const T2& expected)
{
    assert_impl(location,
        actual != expected,
        fmt::format("expected '{}' to not equal '{}'",
            util::to_constexpr_string(actual),
            util::to_constexpr_string(expected)));
}

} // namespace detail

inline void throw_compiletime_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location = std::source_location::current(),
    std::string_view type_ = "symbol",
    std::string_view scope = "main",
    util::AST_Node const& symbols = util::AST::object())
{
    detail::compile_error_impl(location,
        fmt::format("{} in function '{}'", message, scope),
        symbol,
        symbols,
        type_);
}

} // namespace credence