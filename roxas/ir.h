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
#include <utility>
#include <vector>

// https://github.com/shinh/elvm/blob/master/ir/ir.h
// https://github.com/arnlaugsson/project-3/blob/master/code.py

// https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf
// ^ slide 156

namespace roxas {

class Quintdruple
{
  public:
    Quintdruple(Quintdruple const&) = delete;
    Quintdruple& operator=(Quintdruple const&) = delete;

  public:
    enum class Operator
    {
        FUNC_START,
        FUNC_END,
        LABEL,
        GOTO,
        MINUS,
        PLUS,
        LT,
        GT,
        LE,
        GE,
        XOR,
        LSHIFT,
        RSHIFT,
        SUBTRACT,
        ADD,
        MOD,
        MUL,
        DIV,
        INDIRECT,
        ADDR_OF,
        UMINUS,
        UNOT,
        UONE,
        PUSH,
        POP,
        CALL,
        VARIABLE,
        RETURN,
        NOOP
    };

    constexpr std::string_view operator_to_string(Operator op) noexcept;

  public:
    using Table = std::pair<std::string_view, std::size_t>;
    using Entry = Symbol_Table<std::array<std::string_view, 4>>::Table_Entry;
    using Quint = std::pair<Operator, std::array<std::string_view, 4>>;
    explicit Quintdruple(Operator op,
                         Entry arg1,
                         Entry arg2,
                         Entry result, // result or label
                         Entry label = "")
        : quintdruple_(std::pair(
              op,
              std::array<std::string_view, 4>{ arg1, arg2, result, label }))
    {
    }
    ~Quintdruple() = default;

  public:
    Quint get() noexcept;

  private:
    Quint quintdruple_{};
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
    using Node = json::JSON;
    using Nodes = json::JSON;
    using Symbol = Symbol_Table<unsigned int>::Table_Entry;
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

  private:
    Symbol_Table<Symbol> symbols_{};
    std::vector<Quintdruple> quintdruple_list_{};
    std::vector<Quintdruple::Entry> labels_{};
};

} // namespace roxas