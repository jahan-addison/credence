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

#include <array>
#include <fstream>
#include <roxas/json.h>
#include <roxas/symbol.h>
#include <string_view>
#include <vector>

// https://github.com/shinh/elvm/blob/master/ir/ir.h
// https://github.com/arnlaugsson/project-3/blob/master/code.py

// https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf
// ^ slide 156

namespace roxas {

class Quintdruple
{
  public:
    Quintdruple(Quadruple const&) = delete;
    Quintdruple& operator=(Quintdruple const&) = delete;

  public:
    enum class Operator
    {
        LABEL,
        GOTO,
        PUSH,
        POP,
        CALL,
        FUNC_START,
        FUCN_END,
        VARIABLE,
        RETURN,
        NOOP
    };

  public:
    using Entry = Symbol_Table::Table_Entry;
    explicit Quintdruple(Entry op,
                         Entry arg1,
                         Entry arg2,
                         Entry result, // quadruple: result or label
                         Entry label = "")
        : quintdruple_({ op, arg1, arg2, result, label })
    {
    }
    ~Quintdruple() = default;

  public:
    std::string_view get();

  private:
    std::array<std::string, 5> quintdruple_{};
};

class Intermediate_Representation
{
  public:
    Intermediate_Representation(Intermediate_Representation const&) = delete;
    Intermediate_Representation& operator=(Intermediate_Representation const&) =
        delete;

  public:
    explicit Intermediate_Representation(json::JSON& ast)
        : ast_(std::move(ast))
    {
    }
    ~Intermediate_Representation() = default;

  public:
    using Node = json::Object;
    using Nodes = json::Array;
    void emit_to_stdout();
    void emit_to(std::fstream const& fstream);

  private:
    void from_assignment_expression(Node node);
    // inline lvalues
    void from_identifier(Node node);
    void from_indirect_identifier(Node node);
    void from_vector_idenfitier(Node node);
    // constants
    void from_number_literal(Node node);

  private:
    json::JSON ast_;
    Symbol_Table<std::array<Symbol_Table::Table_Entry, 2>> symbols{};
    std::vector<Quintdruple> quintdruple_list{};
    std::vector<Symbol_Table::Table_Entry> labels_{};
};

} // namespace roxas