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

#include <credence/ir/ita.h> // for ITA
#include <credence/symbol.h> // for Symbol_Table
#include <deque>             // for deque
#include <map>               // for map
#include <memory>            // for unique_ptr
#include <optional>          // for optional
#include <set>               // for set
#include <string>            // for basic_string, string
#include <utility>           // for pair
#include <variant>           // for variant
#include <vector>            // for vector

namespace credence {

namespace ir {

class Normalization
{
  public:
    Normalization() = delete;
    Normalization(Normalization&) = delete;

  public:
    explicit Normalization(
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
    using Parameters = std::set<std::string>;
    using Labels = std::set<Label>;
    using Locals = std::set<std::string>;

    constexpr static auto UNARY_TYPES = { "++", "--", "*", "&", "-",
                                          "+",  "~",  "!", "~" };

    constexpr bool is_unary(std::string_view rvalue)
    {
        return std::ranges::any_of(UNARY_TYPES, [&](std::string_view x) {
            return rvalue.find(x) != std::string_view::npos;
        });
    }

  public:
    ITA::Instructions from_ita_instructions();

  public:
    static ITA::Instructions to_normal_form_ita(
        ITA::Node const& symbols,
        util::AST_Node ast);

  private:
    struct Function_Definition
    {
        Labels labels;
        Locals locals;
        Parameters parameters;
        static constexpr int max_depth{ 50 };
        unsigned int allocation{ 0 };

        std::deque<std::variant<LValue, RValue>> stack;
        ITA::Instructions instructions{};
    };
    using Function_PTR = std::unique_ptr<Function_Definition>;
    using Functions = std::map<std::string, Function_PTR>;
    using Stack_Frame = std::optional<Function_PTR*>;

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
    using RValue_Data_Type = std::
        tuple<Normalization::RValue, Normalization::Type, Normalization::Size>;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction();
    void from_func_end_ita_instruction(ITA::Quadruple const& instruction);
    void from_label_ita_instruction(ITA::Quadruple const& instruction);
    void from_variable_ita_instruction(ITA::Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    RValue_Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        Normalization::RValue const& rvalue,
        std::string_view unary_operator);
    void from_symbol_reassignment(
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