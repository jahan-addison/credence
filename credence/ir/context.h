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

#include <algorithm>         // for remove_if, __any_of, __find_if, any_of
#include <cctype>            // for isspace
#include <credence/assert.h> // for CREDENCE_ASSERT
#include <credence/ir/ita.h> // for ITA
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <initializer_list>  // for initializer_list
#include <map>               // for map
#include <memory>            // for shared_ptr, unique_ptr
#include <optional>          // for nullopt, nullopt_t, optional
#include <set>               // for set
#include <string>            // for basic_string, string
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for tuple
#include <utility>           // for pair
#include <variant>           // for variant

namespace credence {

namespace ir {

class Context
{
  public:
    Context() = delete;
    Context(Context&) = delete;

  public:
    explicit Context(
        ITA::Node const& hoisted_symbols,
        ITA::Instructions const& instructions)
        : instructions_(instructions)
        , hoisted_symbols_(hoisted_symbols)
    {
    }

  public:
    using Label = std::string;
    using Type = std::string;
    using Address = std::size_t;
    using Size = std::size_t;
    using LValue = std::string;
    using RValue = std::string;
    using RValue_Reference = std::variant<LValue, RValue>;
    using Temporary = std::pair<LValue, RValue>;
    using Parameters = std::set<std::string>;
    using Labels = std::set<Label>;
    using Locals = std::set<std::string>;

    constexpr static auto UNARY_TYPES = { "++", "--", "*", "&", "-",
                                          "+",  "~",  "!", "~" };

    inline bool is_unary(std::string_view rvalue)
    {
        return std::ranges::any_of(UNARY_TYPES, [&](std::string_view x) {
            return rvalue.starts_with(x) or rvalue.ends_with(x);
        });
    }
    constexpr inline std::string_view get_unary(std::string_view rvalue)
    {
        auto it = std::ranges::find_if(UNARY_TYPES, [&](std::string_view op) {
            return rvalue.find(op) != std::string_view::npos;
        });
        if (it == UNARY_TYPES.end())
            return "";
        return *it;
    }
    inline std::string get_unary_lvalue(std::string& lvalue)
    {
        std::string_view unary_chracters = "+-*&+~!";
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

  public:
    ITA::Instructions from_ita_instructions();

  public:
    static ITA::Instructions to_ita_from_ast(
        ITA::Node const& symbols,
        ITA::Node const& ast);

  private:
    struct Function_Definition
    {
        Label symbol{};
        Labels labels;
        Locals locals;
        Parameters parameters;
        static constexpr int max_depth{ 50 };
        unsigned int allocation{ 0 };

        std::map<LValue, RValue> temporary;
        std::deque<RValue_Reference> stack;
        ITA::Instructions instructions{};
    };
    using Function_PTR = std::shared_ptr<Function_Definition>;
    using Functions = std::map<std::string, Function_PTR>;
    using Stack_Frame = std::optional<Function_PTR>;

    constexpr inline bool is_stack_frame() { return stack_frame.has_value(); }
    inline Function_PTR get_stack_frame()
    {
        CREDENCE_ASSERT(is_stack_frame());
        return stack_frame.value();
    }

  private:
    struct Vector_Definition
    {
        explicit Vector_Definition(int size_of)
            : size(size_of)
        {
        }
        std::vector<RValue> data{};
        int decay_index{ 0 };
        unsigned long size{ 0 };
        static constexpr int max_size{ 1000 };
    };
    using Vectors = std::map<std::string, std::unique_ptr<Vector_Definition>>;

  private:
    void context_frame_error(std::string_view message, std::string_view symbol);
    using RValue_Data_Type =
        std::tuple<Context::RValue, Context::Type, Context::Size>;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction();
    void from_func_end_ita_instruction(ITA::Quadruple const& instruction);
    void from_label_ita_instruction(ITA::Quadruple const& instruction);
    void from_variable_ita_instruction(ITA::Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    RValue_Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue& rvalue,
        std::string_view unary_operator);
    void from_symbol_reassignment(
      LValue const& lhs,
      LValue const& rhs);
    void from_temporary_assignment(
      LValue const& lhs,
      LValue const& rhs);
    RValue_Data_Type from_integral_unary_expression(RValue const& lvalue);
    RValue_Data_Type static get_rvalue_symbol_type_size(std::string const& datatype);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Symbol_Table<RValue_Data_Type, LValue> symbols_{};
    Symbol_Table<LValue, Address> address_table_{};
    // clang-format on

  private:
    Stack_Frame stack_frame{ std::nullopt };
    size_t instruction_index{ 0 };
    ITA::Instructions instructions_;
    ITA::Node hoisted_symbols_;

  public:
    Functions functions{};
    Labels labels{};
};

} // namespace ir

} // namespace credence