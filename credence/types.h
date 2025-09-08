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

#include <credence/eternal.h>
#include <credence/operators.h>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace credence {

namespace type {

struct RValue;

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

} // namespace detail

using Byte = unsigned char;
using Type_Size = std::pair<std::string, std::size_t>;
using Value = std::variant<std::monostate,
                           int,
                           long,
                           Byte,
                           float,
                           double,
                           bool,
                           std::string,
                           char>;

MAPBOX_ETERNAL_CONSTEXPR auto LITERAL_TYPE =
    mapbox::eternal::map<std::string_view, Type_Size>(
        { { "word", { "word", sizeof(void*) } },
          { "byte", { "byte", sizeof(Byte) } },
          { "int", { "int", sizeof(int) } },
          { "long", { "long", sizeof(long) } },
          { "float", { "float", sizeof(float) } },
          { "double", { "double", sizeof(double) } },
          { "bool", { "bool", sizeof(bool) } },
          { "null", { "null", 0 } },
          { "char", { "char", sizeof(char) } } });

constexpr std::pair<std::monostate, Type_Size> NULL_LITERAL = {
    std::monostate(),
    type::LITERAL_TYPE.find("null")->second
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

    using Function = std::pair<LValue, std::vector<RValue_Pointer>>;
    using Type = std::variant<std::monostate,
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

inline RValue::Type_Pointer rvalue_type_pointer_from_rvalue(
    type::RValue::Type rvalue_type) // not constexpr until C++23
{
    return std::make_shared<RValue::Type>(rvalue_type);
}

constexpr size_t get_type_of_rvalue(RValue const& rvalue)
{
    if (rvalue.value.index() ==
        detail::type_variant(RValue_Type_Variant::RValue_Pointer)) {
        return std::get<RValue::RValue_Pointer>(rvalue.value)->value.index();
    } else {
        return rvalue.value.index();
    }
}

constexpr RValue_Type_Variant get_rvalue_type_as_variant(RValue const& rvalue)
{
    return static_cast<RValue_Type_Variant>(get_type_of_rvalue(rvalue));
}

constexpr bool is_rvalue_variant(RValue const& rvalue, RValue_Type_Variant type)
{
    return get_type_of_rvalue(rvalue) == detail::type_variant(type);
}

} // namespace type

} // namespace credence