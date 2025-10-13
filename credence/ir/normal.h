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

class ITA_Normalize
{
  public:
    ITA_Normalize() = delete;
    ITA_Normalize(ITA_Normalize&) = delete;

  public:
    explicit ITA_Normalize(
        ITA::Node const& symbols,
        ITA::Instructions const& instructions)
        : instructions_(instructions)
        , hoisted_symbols_(symbols)
    {
    }

  public:
    using Label = std::string;
    using Type = std::string;
    using Address = std::size_t;
    using LValue = std::pair<std::string, Type>;
    using RValue = std::string;
    using Parameters = std::set<std::string>;
    using Labels = std::set<Label>;
    using Locals = std::set<std::string>;

  public:
    ITA::Instructions from_ita_instructions();

  public:
    static ITA::Instructions to_normal_form(
        ITA::Node const& symbols,
        ITA::Instructions const& instructions);

  private:
    void from_ita_vector();
    void from_ita_function();

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

  public:
    Symbol_Table<LValue, Address> symbol_table{};

  private:
    size_t instruction_index{ 0 };
    ITA::Instructions instructions_;
    ITA::Node hoisted_symbols_;

  public:
    Functions functions{};
    Labels labels{};
};

} // namespace ir

} // namespace credence