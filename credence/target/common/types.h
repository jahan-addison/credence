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

#include <credence/ir/object.h>
#include <credence/types.h>

#include <concepts>
#include <deque>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace credence::target::common {

/**
 * @brief Common semantic types used by both architectures
 */
using Label = type::semantic::Label;
using Type = type::semantic::Type;
using Size = type::semantic::Size;
using LValue = type::semantic::LValue;
using RValue = type::semantic::RValue;
using Immediate = type::Data_Type;
using Stack_Offset = std::size_t;

using Operand_Lambda = std::function<bool(RValue)>;

namespace detail {

// Helper trait to detect std::pair
template<typename T>
struct is_pair : std::false_type
{};

template<typename T, typename U>
struct is_pair<std::pair<T, U>> : std::true_type
{};

template<typename T>
struct is_deque : std::false_type
{};

template<typename T>
struct is_variant : std::false_type
{};

template<typename T, typename Alloc>
struct is_deque<std::deque<T, Alloc>> : std::true_type
{};

template<typename... Args>
struct is_variant<std::variant<Args...>> : std::true_type
{};

struct base_stack_pointer
{
    base_stack_pointer();
    ~base_stack_pointer();
    base_stack_pointer(base_stack_pointer const&) = delete;
    base_stack_pointer& operator=(base_stack_pointer const&) = delete;
    base_stack_pointer(base_stack_pointer&&) noexcept;
    base_stack_pointer& operator=(base_stack_pointer&&) noexcept;

    using Offset = Stack_Offset;

  private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace detail

template<typename T>
concept Pair_T = detail::is_pair<T>::value;

template<typename T>
concept Stack_T = std::derived_from<T, detail::base_stack_pointer>;
template<Stack_T Stack>
using Stack_Pointer = std::shared_ptr<Stack>;

template<typename T>
concept Deque_T = detail::is_deque<T>::value;
template<typename T>
concept Variant_T = detail::is_variant<T>::value;
template<Variant_T T, Deque_T U>
using Instruction_Pair = std::pair<T, U>;

template<typename T>
concept Enum_T = std::is_enum_v<T> && !std::is_convertible_v<T, int>;
template<Enum_T Registers>
using Storage_T =
    std::variant<std::monostate, Stack_Offset, Registers, Immediate>;

template<typename T1, typename T2, typename... Rest>
concept Operands_T = (std::same_as<T2, Rest> && ...);

template<Enum_T Registers>
using Binary_Operands_T = std::pair<Storage_T<Registers>, Storage_T<Registers>>;

template<Enum_T Registers>
using Ternary_Operands_T = std::
    tuple<Storage_T<Registers>, Storage_T<Registers>, Storage_T<Registers>>;

template<Enum_T Directives>
using Directive_Pair = std::pair<std::string, Directives>;

using Table_Pointer = std::shared_ptr<ir::object::Object>;
template<typename T>
using Pointer_ = std::shared_ptr<T>;

template<Enum_T M, Enum_T R>
using Mnemonic_2ARY = std::tuple<M, Storage_T<R>, Storage_T<R>>;

template<Enum_T M, Enum_T R>
using Mnemonic_3ARY = std::tuple<M, Storage_T<R>, Storage_T<R>, Storage_T<R>>;

template<Enum_T M, Enum_T R>
using Mnemonic_4ARY =
    std::tuple<M, Storage_T<R>, Storage_T<R>, Storage_T<R>, Storage_T<R>>;

template<Enum_T M, Enum_T R>
using Instruction_2ARY = std::variant<Label, Mnemonic_2ARY<M, R>>;

template<Enum_T M, Enum_T R>
using Instruction_3ARY = std::variant<Label, Mnemonic_3ARY<M, R>>;

template<Enum_T M, Enum_T R>
using Instruction_4ARY = std::variant<Label, Mnemonic_4ARY<M, R>>;

template<Enum_T M, Enum_T R>
using Instruction_T = std::variant<Label,
    Mnemonic_2ARY<M, R>, // x86-64: mov rax, 5
    Mnemonic_3ARY<M, R>  // ARM64: mul x0, x1, x2
    >;

template<typename T, typename M, typename R>
concept Instruction_Input = std::same_as<T, Instruction_T<M, R>>;

template<Enum_T M, Enum_T R>
using Instructions = std::deque<Instruction_T<M, R>>;

template<Enum_T T>
constexpr T get_first_of_enum_t(int first = 0)
{
    return static_cast<T>(first);
}

} // namespace credence::target::common
