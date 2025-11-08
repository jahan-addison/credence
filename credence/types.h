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

#include <credence/operators.h> // for Operator
#include <cstddef>              // for size_t
#include <mapbox/eternal.hpp>   // for map
#include <memory>               // for shared_ptr, make_shared
#include <string>               // for basic_string, string
#include <string_view>          // for basic_string_view, string_view
#include <type_traits>          // for is_constructible_v, underlying_type_t
#include <utility>              // for pair, make_pair
#include <variant>              // for get, monostate, variant
#include <vector>               // for vector

namespace credence {
namespace type {
struct RValue;
}
}

namespace credence {

namespace type {

enum class RValue_Type_Variant : size_t
{
    RValue_Pointer = 1,
    Value_Pointer,
    Symbol,
    Unary,
    Relation,
    Function,
    LValue,
    Value
};

namespace detail {

using RValue_Pointer_PTR = std::shared_ptr<type::RValue>;

constexpr unsigned long type_variant(RValue_Type_Variant const& type)
{
    return static_cast<std::underlying_type_t<RValue_Type_Variant>>(type);
}

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

inline float integral_from_type_double(std::string const& t)
{
    return std::stod(t);
}

} // namespace detail

template<typename T>
T integral_from_type(std::string const& t)
{
    if constexpr (std::is_same_v<T, int>) {
        return detail::integral_from_type_int(t);
    } else if constexpr (std::is_same_v<T, long>) {
        return detail::integral_from_type_long(t);
    } else if constexpr (std::is_same_v<T, float>) {
        return detail::integral_from_type_float(t);
    } else {
        return detail::integral_from_type_double(t);
    }
}

using Byte = unsigned char;
using Type_Size = std::pair<std::string, std::size_t>;
using Value = std::variant<
    std::monostate,
    int,
    long,
    Byte,
    float,
    double,
    bool,
    std::string,
    char>;

#ifdef __clang__
constexpr auto LITERAL_TYPE =
    mapbox::eternal::map<std::string_view, std::pair<std::string, std::size_t>>(
        { { "word", { "word", sizeof(void*) } },
          { "byte", { "byte", sizeof(Byte) } },
          { "int", { "int", sizeof(int) } },
          { "long", { "long", sizeof(long) } },
          { "float", { "float", sizeof(float) } },
          { "double", { "double", sizeof(double) } },
          { "bool", { "bool", sizeof(bool) } },
          { "null", { "null", 0 } },
          { "char", { "char", sizeof(char) } } });
#else
static auto LITERAL_TYPE =
    mapbox::eternal::map<std::string_view, std::pair<std::string, std::size_t>>(
        { { "word", { "word", sizeof(void*) } },
          { "byte", { "byte", sizeof(Byte) } },
          { "int", { "int", sizeof(int) } },
          { "long", { "long", sizeof(long) } },
          { "float", { "float", sizeof(float) } },
          { "double", { "double", sizeof(double) } },
          { "bool", { "bool", sizeof(bool) } },
          { "null", { "null", 0 } },
          { "char", { "char", sizeof(char) } } });
#endif

constexpr auto NULL_LITERAL =
    std::pair<std::monostate, std::pair<std::string, std::size_t>>{
        std::monostate{},
        std::pair<std::string, std::size_t>{ "null", 0 }
    };

constexpr auto WORD_LITERAL =
    std::pair<std::string, std::pair<std::string, std::size_t>>{
        "__WORD__",
        std::pair<std::string, std::size_t>{ "word", sizeof(void*) }
    };

using Value_Type = std::pair<Value, Type_Size>;
using Value_Pointer = std::vector<Value_Type>;

struct RValue
{
    using RValue_Pointer = detail::RValue_Pointer_PTR;
    using Value_Pointer = type::Value_Pointer;

    using Value = Value_Type;
    using LValue = std::pair<std::string, Value>;
    using Symbol = std::pair<LValue, RValue_Pointer>;
    using Unary = std::pair<type::Operator, RValue_Pointer>;
    using Relation = std::pair<type::Operator, std::vector<RValue_Pointer>>;

    constexpr inline static LValue make_lvalue(std::string const& name)
    {
        return std::make_pair(name, WORD_LITERAL);
    }

    constexpr inline static LValue make_lvalue(
        std::string const& name,
        Value_Type const& value)
    {
        return std::make_pair(name, value);
    }

    using Function = std::pair<LValue, std::vector<RValue_Pointer>>;
    using Type = std::variant<
        std::monostate,
        RValue_Pointer,
        Value_Pointer,
        Symbol,
        Unary,
        Relation,
        Function,
        LValue,
        Value>;

    using Type_Pointer = std::shared_ptr<RValue::Type>;

    Type value;
};

inline auto make_rvalue_value_type(
    Value const& value,
    Type_Size const& type_size)
{
    return std::make_pair(value, type_size);
}

inline RValue::Type_Pointer rvalue_type_pointer_from_rvalue(
    type::RValue::Type rvalue_type) // not constexpr until C++23
{
    return std::make_shared<RValue::Type>(rvalue_type);
}

inline size_t get_type_of_rvalue(RValue const& rvalue)
{
    if (rvalue.value.index() ==
        detail::type_variant(RValue_Type_Variant::RValue_Pointer)) {
        return std::get<RValue::RValue_Pointer>(rvalue.value)->value.index();
    } else {
        return rvalue.value.index();
    }
}
template<typename T>
constexpr inline T get_raw_value_from_rvalue_type(
    RValue::Type_Pointer const& type)
{
    static_assert(
        std::is_constructible_v<Value, T>,
        "Error: Type T is not a valid alternative in Value");
    return std::get<T>(std::get<RValue::Value>(*type).first);
}

inline RValue_Type_Variant get_rvalue_type_as_variant(RValue const& rvalue)
{
    return static_cast<RValue_Type_Variant>(get_type_of_rvalue(rvalue));
}

inline bool is_rvalue_type_pointer_variant(
    RValue::Type_Pointer const& rvalue,
    RValue_Type_Variant type)
{
    return rvalue->index() == detail::type_variant(type);
}

inline bool is_rvalue_variant(RValue const& rvalue, RValue_Type_Variant type)
{
    return get_type_of_rvalue(rvalue) == detail::type_variant(type);
}

} // namespace type

} // namespace credence