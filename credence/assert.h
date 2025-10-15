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
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/formatting.hpp>
#include <format>
#include <iostream>
#include <simplejson.h>
#include <stdexcept>
#include <string_view>

#ifndef CREDENCE_TEST
#define credence_error(message)  do {           \
  credence::credence_cpptrace_stack_trace(1,2); \
  throw std::runtime_error(message);            \
} while(0)
#else
#define credence_error(message)  do {    \
  throw std::runtime_error(message);     \
} while(0)
#endif

#ifndef CREDENCE_TEST
#define credence_runtime_error(message, symbol, symbols)     \
    ::credence_runtime_error_impl(message, symbol, symbols)
#else
#define credence_runtime_error(message, symbol, symbols)     \
    throw std::runtime_error(message)
#endif

#define CREDENCE_TRY_CATCH_BLOCK cpptrace::try_catch
#define CREDENCE_TRY CPPTRACE_TRY
#define CREDENCE_CATCH(param) CPPTRACE_CATCH(param)

#define CREDENCE_ASSERT(condition)                      \
        ::assert_impl(condition, "")
#define CREDENCE_ASSERT_MESSAGE(condition, message)     \
        ::assert_impl(condition, message)
#define CREDENCE_ASSERT_EQUAL(actual, expected)         \
        ::assert_equal_impl(actual, expected)
#define CREDENCE_ASSERT_NODE(actual, expected)          \
        ::assert_equal_impl(actual, expected)

namespace credence {

inline void credence_cpptrace_stack_trace(int skip = 2, int depth = 3)
{
    cpptrace::formatter{}
        .header("Credence Stack trace: ")
        .addresses(cpptrace::formatter::address_mode::object)
        .snippets(true)
        .print(cpptrace::generate_trace(skip, depth));
}

} // namespace credence

namespace {

inline void credence_runtime_error_impl(
    std::string_view message,
    std::string_view symbol_name,
    json::JSON symbols)
{
    auto symbol = symbol_name.data();
    if (symbols.has_key(symbol)) {
        credence_error(
            std::format(
                ">>> Runtime error :: on \"{}\" {}\n"
                ">>>    from line {} column {}:{}",
                symbol,
                message,
                symbols[symbol]["line"].to_int(),
                symbols[symbol]["column"].to_int(),
                symbols[symbol]["end_column"].to_int()));
    } else {
        credence_error(
            std::format(">>> Runtime error :: \"{}\" {}", symbol, message));
    }
}

[[maybe_unused]] void assert_impl(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "Credence Assertion :: " << message << std::endl;
        credence::credence_cpptrace_stack_trace();
        std::abort();
    }
}

template<typename T1, typename T2>
[[maybe_unused]] void assert_equal_impl(
    [[maybe_unused]] const T1& actual,
    [[maybe_unused]] const T2& expected)
{
    if (actual != expected) {
        std::cerr << std::format(
            "Credence Assertion: {} == {}\n", actual, expected);
        credence::credence_cpptrace_stack_trace();
        std::abort();
    }
}

} // namespace
