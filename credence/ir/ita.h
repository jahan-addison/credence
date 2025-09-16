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

#include <algorithm>         // for __find, find
#include <array>             // for array
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>             // for deque
#include <iomanip>           // for operator<<, setw
#include <matchit.h>         // for pattern, Or, Wildcard, PatternH...
#include <optional>          // for optional
#include <ostream>           // for basic_ostream, operator<<, endl
#include <ranges>            // for ranges::find
#include <simplejson.h>      // for JSON
#include <sstream>           // for basic_ostringstream, ostream
#include <string>            // for basic_string, char_traits, allo...
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple, make_tuple
#include <utility>           // for pair
#include <vector>            // for vector

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Instruction Tuple Abstraction or ITA of program flow,
 * control statements, and application scope in sets of 4-tuples
 *
 * See README.md for details.
 */
class ITA
{
  public:
    ITA(ITA const&) = delete;
    ITA& operator=(ITA const&) = delete;

    explicit ITA(
        json::JSON const& internal_symbols,
        Symbol_Table<> const& symbols,
        Symbol_Table<> const& globals)
        : internal_symbols_(internal_symbols)
        , symbols_(symbols)
        , globals_(globals)
    {
    }

  public:
    enum class Instruction
    {
        FUNC_START,
        FUNC_END,
        LABEL,
        GOTO,
        IF,
        IF_E,
        PUSH,
        POP,
        CALL,
        CMP,
        VARIABLE,
        RETURN,
        LEAVE,
        EOL,
        NOOP
    };

  public:
    using Quadruple =
        std::tuple<Instruction, std::string, std::string, std::string>;
    using Instructions = std::deque<Quadruple>;
    using Tail_Branch = std::optional<Quadruple>;
    using Branch_Comparator = std::pair<std::string, Instructions>;
    using Branch_Instructions = std::pair<Instructions, Instructions>;

    constexpr friend std::ostream& operator<<(
        std::ostream& os,
        ITA::Instruction const& op)
    {
        switch (op) {
            case ITA::Instruction::FUNC_START:
                os << "BeginFunc";
                break;
            case ITA::Instruction::FUNC_END:
                os << "EndFunc";
                break;
            case ITA::Instruction::LABEL:
                break;
            case ITA::Instruction::VARIABLE:
                os << "=";
                break;
            case ITA::Instruction::NOOP:
                os << "";
                break;
            case ITA::Instruction::CMP:
                os << "CMP";
                break;
            case ITA::Instruction::RETURN:
                os << "RET";
                break;
            case ITA::Instruction::LEAVE:
                os << "LEAVE";
                break;
            case ITA::Instruction::IF_E:
                os << "IF_E";
                break;
            case ITA::Instruction::IF:
                os << "IF";
                break;
            case ITA::Instruction::PUSH:
                os << "PUSH";
                break;
            case ITA::Instruction::POP:
                os << "POP";
                break;
            case ITA::Instruction::CALL:
                os << "CALL";
                break;
            case ITA::Instruction::GOTO:
                os << "GOTO";
                break;
            case ITA::Instruction::EOL:
                os << ";";
                break;
            default:
                os << "null";
                break;
        }
        return os;
    }

    /**
     * @brief Emit a qaudruple tuple to a std::ostream
     */
    static void emit_to(std::ostream& os, ITA::Quadruple const& ita)
    {
        ITA::Instruction op = std::get<ITA::Instruction>(ita);
        std::array<ITA::Instruction, 5> lhs_instruction = {
            ITA::Instruction::GOTO,
            ITA::Instruction::PUSH,
            ITA::Instruction::LABEL,
            ITA::Instruction::POP,
            ITA::Instruction::CALL
        };
        if (std::ranges::find(lhs_instruction, op) != lhs_instruction.end()) {
            if (op == ITA::Instruction::LABEL)
                os << std::get<1>(ita) << ":" << std::endl;
            else
                os << op << " " << std::get<1>(ita) << ";" << std::endl;
        } else {
            m::match(op)(
                m::pattern | ITA::Instruction::RETURN =
                    [&] {
                        os << op << " " << std::get<1>(ita) << ";" << std::endl;
                    },
                m::pattern | ITA::Instruction::LEAVE =
                    [&] { os << op << ";" << std::endl; },
                m::pattern |
                    m::or_(ITA::Instruction::IF, ITA::Instruction::IF_E) =
                    [&] {
                        os << op << " " << std::get<1>(ita) << " "
                           << std::get<2>(ita) << " " << std::get<3>(ita) << ";"
                           << std::endl;
                    },
                m::pattern | m::_ =
                    [&] {
                        os << std::get<1>(ita) << " " << op << " "
                           << std::get<2>(ita) << std::get<3>(ita) << ";"
                           << std::endl;
                    }

            );
        }
    }

  public:
    inline void emit(std::ostream& os)
    {
        for (auto const& i : instructions_) {
            ITA::emit_to(os, i);
        }
    }

  public:
#ifdef __clang__
    static constexpr inline Quadruple make_temporary(
#else
    static inline Quadruple make_temporary(
#endif
        int* temporary_size,
        std::string const& temp)
    {
        return make_quadruple(
            Instruction::VARIABLE,
            std::string{ "_t" } + std::to_string(++(*temporary_size)),
            temp);
    }

    static inline void insert_instructions(Instructions& to, Instructions& from)
    {
        to.insert(to.end(), from.begin(), from.end());
    }

#ifdef __clang__
    static constexpr inline Quadruple make_temporary(int* temporary_size)
#else
    static inline Quadruple make_temporary(int* temporary_size)
#endif
    {
        return make_quadruple(
            Instruction::LABEL,
            std::string{ "_L" } + std::to_string(++(*temporary_size)),
            "");
    }
#ifdef __clang__
    static constexpr inline Quadruple make_quadruple(
#else
    static inline Quadruple make_quadruple(
#endif
        Instruction op,
        std::string const& s1,
        std::string const& s2,
        std::string const& s3 = "")
    {
        return std::make_tuple(op, s1, s2, s3);
    }

  public:
    inline std::string instruction_to_string(
        Instruction op) // not constexpr until C++23
    {
        std::ostringstream os;
        os << op;
        return os.str();
    }

    inline std::string quadruple_to_string(
        Quadruple const& ita) // not constexpr until C++23
    {
        std::ostringstream os;
        os << std::setw(2) << std::get<1>(ita) << std::get<0>(ita)
           << std::get<2>(ita) << std::get<3>(ita);

        return os.str();
    }

  public:
    using Node = util::AST_Node;
    Instructions build_from_definitions(Node& node);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_function_definition(Node& node);
    void build_from_vector_definition(Node& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_block_statement(Node& node, bool ret = false);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Branch_Instructions build_from_switch_statement(Tail_Branch& block_level, Node& node);
    Branch_Instructions build_from_while_statement(Tail_Branch& block_level, Node& node);
    Branch_Instructions build_from_if_statement(Tail_Branch& block_level, Node& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_label_statement(Node& node);
    Instructions build_from_goto_statement(Node& node);
    Instructions build_from_return_statement(Node& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_auto_statement(Node& node);
    void build_from_extrn_statement(Node& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_rvalue_statement(Node& node);
    std::vector<std::string> build_from_rvalue_expression(
        type::RValue::Type& rvalue);

  private:
    void insert_rvalue_or_block_branch_instructions(
        Node& block,
        Instructions& branch_instructions);

    std::string build_from_branch_comparator_rvalue(
        Node& block,
        Instructions& instructions);

    // clang-format on
  private:
    int temporary{ 0 };

  private:
    Instructions instructions_;

  private:
    void return_and_resume_instructions(
        Instructions& instructions,
        std::string_view from);
    void return_and_resume_instructions(Instructions& instructions);
    inline void insert_tail_branch_jump_instruction(
        Instructions& instructions,
        Tail_Branch& block_tail)
    {
        instructions.emplace_back(block_tail.value());
    }
    // clang-format off
    std::array<std::string_view, 3> BRANCH_STATEMENTS =
        { "if", "while", "switch" };
    // clang-format on

  private:
    json::JSON internal_symbols_;
    Tail_Branch tail_branch;
    bool is_branching = false;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

} // namespace ir

} // namespace credence