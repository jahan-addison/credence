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
#include <memory>            // for unique_ptr, make_unique
#include <string>            // for basic_string, string
#include <utility>           // for pair
#include <vector>            // for vector

namespace credence {

namespace ir {

class ITA_Parser
{
  public:
    ITA_Parser() = delete;
    ITA_Parser(ITA_Parser&) = delete;

  public:
    explicit ITA_Parser(
        ITA::Instructions&& instructions,
        ITA::Node const& hoisted_symbols)
        : instructions_(std::make_unique<ITA::Instructions>(instructions))
        , hoisted_symbols_(hoisted_symbols)
    {
        instruction_location = 0;
    }

  public:
    using Label = std::string;
    using Address = std::size_t;
    using LValue = std::pair<std::string, std::string>;
    using RValue = std::string;
    using Parameters = std::vector<std::string>;
    using Labels = std::vector<Label>;

  public:
    static ITA::Instructions instructions_pruning_in_place(
        ITA::Instructions const& instructions);
    void from_ita_instructions();

  private:
    void from_ita_vector();
    void from_ita_function();

  private:
    struct Function_Definition
    {
        Labels labels;
        Parameters parameters;
        int max_depth = 50;
        unsigned long size;

        std::deque<LValue> stack;
        ITA::Instructions instructions{};
    };
    using Function_PTR = std::unique_ptr<Function_Definition>;
    using Functions = std::vector<Function_PTR>;

  private:
    struct Vector_Definition
    {
        explicit Vector_Definition(int size_of)
            : size(size_of)
        {
        }
        std::vector<RValue> data{};
        int decay_index = 0;
        unsigned long size;
    };
    using Vector_PTR = std::unique_ptr<Vector_Definition>;
    using Vectors = std::vector<Vector_PTR>;

  public:
    Symbol_Table<LValue, Address> symbols_table{};

  private:
    unsigned int instruction_location;
    std::unique_ptr<ITA::Instructions> instructions_;
    ITA::Node hoisted_symbols_;

  private:
    Functions functions_{};
    Labels labels_{};
};

} // namespace ir

} // namespace credence