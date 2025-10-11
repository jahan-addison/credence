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

#include <algorithm>         // for copy
#include <array>             // for array
#include <credence/assert.h> // for CREDENCE_ASSERT, assert_equal_impl, CRE...
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>             // for operator==, _Deque_iterator, deque
#include <functional>        // for identity
#include <initializer_list>  // for initializer_list
#include <iomanip>           // for operator<<, setw
#include <matchit.h>         // for pattern, Or, Wildcard, PatternHelper
#include <optional>          // for nullopt, optional
#include <ranges>            // for __find_fn, find
#include <simplejson.h>      // for JSON, object
#include <sstream>           // for basic_ostream, operator<<, ostream, bas...
#include <stack>             // for stack
#include <string>            // for allocator, char_traits, operator<<, string
#include <string_view>       // for operator==, string_view
#include <tuple>             // for tuple, get, make_tuple
#include <utility>           // for move, make_pair, pair
#include <vector>            // for vector

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Instruction Tuple Abstraction or ITA of program flow,
 * control statements, and application runtime in sets of 4-tuples
 *
 * See README.md for details.
 */
class ITA
{
  public:
#ifndef CREDENCE_TEST
    ITA(ITA const&) = delete;
    ITA& operator=(ITA const&) = delete;
#endif
    explicit ITA() = default;
    explicit ITA(util::AST_Node const& internal_symbols)
        : internal_symbols_(internal_symbols)
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
        JMP_E,
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
            case ITA::Instruction::JMP_E:
                os << "JMP_E";
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

  public:
    using Node = util::AST_Node;
    /**
     * @brief Emit a qaudruple tuple to a std::ostream
     */
    static void emit_to(
        std::ostream& os,
        ITA::Quadruple const& ita,
        bool indent = false)
    {
        ITA::Instruction op = std::get<ITA::Instruction>(ita);
        constexpr std::array<ITA::Instruction, 5> lhs_instruction = {
            ITA::Instruction::GOTO,
            ITA::Instruction::PUSH,
            ITA::Instruction::LABEL,
            ITA::Instruction::POP,
            ITA::Instruction::CALL
        };
        if (std::ranges::find(lhs_instruction, op) != lhs_instruction.end()) {
            if (op == ITA::Instruction::LABEL) {
                os << std::get<1>(ita) << ":" << std::endl;
            } else {
                if (indent)
                    os << "    ";
                os << op << " " << std::get<1>(ita) << ";" << std::endl;
            }
        } else {
            if (indent) {
                if (op != ITA::Instruction::FUNC_START and
                    op != ITA::Instruction::FUNC_END)
                    os << "    ";
            }
            m::match(op)(
                m::pattern | ITA::Instruction::RETURN =
                    [&] {
                        os << op << " " << std::get<1>(ita) << ";" << std::endl;
                    },
                m::pattern | ITA::Instruction::LEAVE =
                    [&] { os << op << ";" << std::endl; },
                m::pattern |
                    m::or_(ITA::Instruction::IF, ITA::Instruction::JMP_E) =
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
    inline void emit(std::ostream& os, bool indent)
    {
        for (auto const& i : instructions_) {
            ITA::emit_to(os, i, indent);
        }
    }

  public:
    static constexpr inline Quadruple make_temporary(
        int* temporary_size,
        std::string const& temp)
    {
        return make_quadruple(
            Instruction::VARIABLE,
            std::string{ "_t" } +
                util::to_constexpr_string<int>(++(*temporary_size)),
            temp);
    }

    static inline void insert_instructions(Instructions& to, Instructions& from)
    {
        to.insert(to.end(), from.begin(), from.end());
    }

    static constexpr inline Quadruple make_temporary(int* temporary_size)
    {
        return make_quadruple(
            Instruction::LABEL,
            std::string{ "_L" } +
                util::to_constexpr_string<int>(++(*temporary_size)),
            "");
    }

    constexpr inline Quadruple make_temporary()
    {
        return make_quadruple(
            Instruction::LABEL,
            std::string{ "_L" } + util::to_constexpr_string<int>(++temporary),
            "");
    }
    static constexpr inline Quadruple make_quadruple(
        Instruction op,
        std::string const& s1,
        std::string const& s2,
        std::string const& s3 = "")
    {
        return std::make_tuple(op, s1, s2, s3);
    }

  public:
    static inline util::AST_Node make_block_statement(
        std::deque<util::AST_Node> const& blocks)
    {
        auto block_statement = util::AST::object();
        block_statement["node"] = util::AST_Node{ "statement" };
        block_statement["root"] = util::AST_Node{ "block" };
        block_statement["left"] = util::AST_Node{ blocks };

        return block_statement;
    }
    static inline util::AST_Node make_block_statement(util::AST_Node block)
    {
        auto block_statement = util::AST::object();
        block_statement["node"] = util::AST_Node{ "statement" };
        block_statement["root"] = util::AST_Node{ "block" };
        block_statement["left"].append(block);
        return block_statement;
    }
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
    Instructions build_from_definitions(Node const& node);
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_function_definition(Node const& node);
    void build_from_vector_definition(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_block_statement(
    Node const& node,
    bool root_function_scope = false);
  private:
    void build_statement_setup_branches(
        std::string_view type,
        Instructions& instructions);
    void build_statement_teardown_branches(
        std::string_view type,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Branch_Instructions build_from_switch_statement(Node const& node);
    Branch_Instructions build_from_case_statement(
        Node const& node,
        std::string const& switch_label,
        Tail_Branch const& tail);
    Branch_Instructions build_from_while_statement(Node const& node);
    Branch_Instructions build_from_if_statement(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_label_statement(Node const& node);
    Instructions build_from_goto_statement(Node const& node);
    Instructions build_from_return_statement(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_auto_statement(Node const& node);
    void build_from_extrn_statement(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_rvalue_statement(Node const& node);
    std::vector<std::string> build_from_rvalue_expression(
        type::RValue::Type& rvalue);

  private:
    void insert_branch_block_instructions(
        Node const& block,
        Instructions& branch_instructions);
    void insert_branch_jump_and_resume_instructions(
        Node const& block,
        Instructions& predicate_instructions,
        Instructions& branch_instructions,
        Quadruple const& label,
        Tail_Branch const& tail = std::nullopt);

    std::string build_from_branch_comparator_rvalue(
        Node const& block,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    int temporary{ 0 };
    // clang-format on

  private:
    Instructions instructions_;

  private:
    /**
     * @brief An object of branch state during ITA construction,
     * in order to enable continuation and passing of instructions
     *
     */
    class Branch
    {
      public:
        Branch() = delete;
        explicit Branch(int* temporary)
            : temporary(temporary)
        {
        }

      public:
        constexpr inline static bool is_branching_statement(std::string_view s)
        {
            return std::ranges::find(BRANCH_STATEMENTS, s) !=
                   BRANCH_STATEMENTS.end();
        }
        constexpr inline static bool last_instruction_is_jump(
            Quadruple const& inst)
        {
            return std::get<0>(inst) == Instruction::GOTO;
        }
        inline void increment_branch_level()
        {
            is_branching = true;
            level++;
            block_level = ITA::make_temporary(temporary);
            stack.emplace(block_level);
        }
        inline void decrement_branch_level(bool not_branching = false)
        {
            CREDENCE_ASSERT(level > 1);
            CREDENCE_ASSERT(!stack.empty());
            level--;
            if (not_branching)
                is_branching = false;
            stack.pop();
        }
        inline void teardown()
        {
            CREDENCE_ASSERT_EQUAL(level, 1);
            is_branching = false;
        }

      public:
        inline Tail_Branch get_parent_branch(bool last = false)
        {
            CREDENCE_ASSERT(root_branch.has_value());
            if (last and stack.size() > 1) {
                auto top = stack.top();
                stack.pop();
                auto previous = stack.top();
                stack.emplace(top);
                return previous;
            } else
                return stack.empty() ? root_branch : stack.top();
        }
        constexpr inline bool is_root_level() { return level == 1; }
        constexpr inline bool is_branch_level() { return level > 1; }
        constexpr inline void set_block_to_root() { block_level = root_branch; }
        constexpr inline Tail_Branch get_root_branch() { return root_branch; }
        constexpr inline void set_root_branch(ITA& ita)
        {
            if (level == 1) {
                // _L1 label is reserved for function scope continuation
                root_branch = ita.make_temporary();
                set_block_to_root();
            }
        }

      public:
        std::stack<Tail_Branch> stack{};
        // clang-format off
        static constexpr auto
        BRANCH_STATEMENTS = { "if", "while", "case" };
        // clang-format on

      private:
        Tail_Branch root_branch;
        Tail_Branch block_level;
        bool is_branching = false;
        int level = 1;
        int* temporary;
    };

  private:
    Branch branch{ &temporary };
    inline Branch_Instructions make_statement_instructions()
    {
        return std::make_pair(Instructions{}, Instructions{});
    }
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    inline void make_root_branch() {
        branch.set_root_branch(*this);
    }

  CREDENCE_PRIVATE_UNLESS_TESTED:
    util::AST_Node internal_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

} // namespace ir

} // namespace credence