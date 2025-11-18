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

#include <compare>           // for operator<, _CmpUnspecifiedParam
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLE...
#include <credence/value.h>  // for Expression, Literal
#include <deque>             // for deque, operator<=>, operator==
#include <matchit.h>         // for matchit
#include <optional>          // for nullopt, nullopt_t, optional
#include <ostream>           // for operator<<, basic_ostream, endl
#include <simplejson.h>      // for JSON
#include <sstream>           // for ostream
#include <stack>             // for stack
#include <string>            // for basic_string, string, operator+
#include <string_view>       // for string_view
#include <tuple>             // for tuple, make_tuple
#include <utility>           // for pair, make_pair
#include <vector>            // for vector

namespace credence {

namespace ir {

/**
 * @brief
 * Instruction Tuple Abstraction or ITA of program flow,
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
    ~ITA() = default;
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
        LOCL,
        GLOBL,
        IF,
        JMP_E,
        PUSH,
        POP,
        CALL,
        CMP,
        MOV,
        RETURN,
        LEAVE,
        NOOP
    };

  public:
    using Quadruple =
        std::tuple<Instruction, std::string, std::string, std::string>;
    using Instructions = std::deque<Quadruple>;
    using Node = util::AST_Node;
    using Vector_Decay_Ref = std::vector<internal::value::Literal>;
    using Parameters = std::vector<std::string>;
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
            case ITA::Instruction::MOV:
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
            case ITA::Instruction::GLOBL:
                os << "GLOBL";
                break;
            case ITA::Instruction::LOCL:
                os << "LOCL";
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
            default:
                os << "null";
                break;
        }
        return os;
    }

  public:
    /**
     * @brief Insert instructions from one std::deque into another
     */
    static inline void insert_instructions(
        Instructions& to,
        Instructions const& from)
    {
        to.insert(to.end(), from.begin(), from.end());
    }

  public:
    static void emit_to(
        std::ostream& os,
        ITA::Quadruple const& ita,
        bool indent = false);
    /**
     * @brief Emit a std::deque of instructions to a std::ostream
     *   If indent is true indent with a tab for formatting
     */
    static inline void emit(
        std::ostream& os,
        Instructions const& instructions,
        bool indent = true) // not constexpr until C++23
    {
        for (auto i = instructions.begin(); i < instructions.end(); i++) {
            if (i == instructions.end() - 1 and indent) {
                emit_to(os, *i, false);
                os << std::endl;
            } else
                emit_to(os, *i, indent);
        }
    }
    /**
     * @brief Emit ITA::instructions_ field to an std::ostream
     */
    inline void emit(std::ostream& os)
    { // not constexpr until C++23
        for (auto const& i : instructions_)
            emit_to(os, i);
    }
    /**
     * @brief Emit ITA::instructions_ field to an std::ostream
     *   If indent is true indent with a tab for formatting
     */
    inline void emit(std::ostream& os, bool indent)
    { // not constexpr until C++23
        for (auto const& i : instructions_)
            ITA::emit_to(os, i, indent);
    }

  public:
    /**
     * @brief Create a temporary (e.g. _t5) lvalue from the current temporary
     * size Set as a Instruction::MOV instruction with the right-hamd-side
     */
    static constexpr inline Quadruple make_temporary(
        int* temporary_size,
        std::string const& temp)
    {
        return make_quadruple(
            Instruction::MOV,
            std::string{ "_t" } +
                util::to_constexpr_string<int>(++(*temporary_size)),
            temp);
    }
    /**
     * @brief Create a temporary (e.g. _L5) label from the
     * current temporary size Set as a Instruction::LABEL
     */
    constexpr inline Quadruple make_temporary()
    {
        return make_quadruple(
            Instruction::LABEL,
            std::string{ "_L" } + util::to_constexpr_string<int>(++temporary),
            "");
    }

    /**
     * @brief Create a temporary (e.g. _t5) lvalue from the current temporary
     * size Set as a standlone Instruction::LABEL instruction
     */
    static constexpr inline Quadruple make_temporary(int* temporary_size)
    {
        return make_quadruple(
            Instruction::LABEL,
            std::string{ "_L" } +
                util::to_constexpr_string<int>(++(*temporary_size)),
            "");
    }

    /**
     * @brief Create a qaudruple-tuple from 4 operands, 3 optional
     */
    static constexpr inline Quadruple make_quadruple(
        Instruction op,
        std::string const& s1 = "",
        std::string const& s2 = "",
        std::string const& s3 = "")
    {
        return std::make_tuple(op, s1, s2, s3);
    }

  public:
    static util::AST_Node make_block_statement(
        std::deque<util::AST_Node> const& blocks);
    static util::AST_Node make_block_statement(util::AST_Node block);
    static std::string instruction_to_string(Instruction op);
    static std::string quadruple_to_string(Quadruple const& ita);

  public:
    /**
     * @brief ITA::Instructions factory method
     */
    static inline ITA::Instructions make_ITA_instructions(
        ITA::Node const& internal_symbols,
        ITA::Node const& node)
    {
        auto ita = ITA{ internal_symbols };
        return ita.build_from_definitions(node);
    }

  public:
    Instructions build_from_definitions(Node const& node);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_function_definition(Node const& node);
    void build_from_vector_definition(Node const& node);
    std::string build_function_label_from_parameters(
        std::string_view name,
        Parameters const& parameters);

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
    void build_from_auto_statement(
        Node const& node,
        Instructions& instructions);
    void build_from_extrn_statement(
        Node const& node,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_rvalue_statement(Node const& node);
    std::vector<std::string> build_from_expression_node(
        internal::value::Expression::Type & rvalue);

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
     * including a return label stack and passing of instructions
     */
    class Branch
    {
      public:
        Branch() = delete;
        ~Branch() = default;
        explicit Branch(int* temporary)
            : temporary(temporary)
        {
        }

      public:
        static constexpr auto BRANCH_STATEMENTS = { "if", "while", "case" };

      public:
        constexpr static bool is_branching_statement(std::string_view s);
        constexpr static bool last_instruction_is_jump(Quadruple const& inst);

      public:
        /**
         * @brief Construct root branch label and set to root block
         */
        constexpr inline void set_root_branch(ITA& ita)
        {
            if (level == 1) {
                // _L1 label is reserved for function scope resume
                root_branch = ita.make_temporary();
                set_block_to_root();
            }
        }
        void increment_branch_level();
        void decrement_branch_level(bool not_branching = false);
        void teardown();
        Tail_Branch get_parent_branch(bool last = false);
        constexpr inline bool is_root_level() { return level == 1; }
        constexpr inline bool is_branch_level() { return level > 1; }
        constexpr inline void set_block_to_root() { block_level = root_branch; }
        constexpr inline Tail_Branch get_root_branch() { return root_branch; }

      public:
        std::stack<Tail_Branch> stack{};

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
    inline void make_root_branch()
    {
        temporary = 0;
        branch.set_root_branch(*this);
    }
    util::AST_Node internal_symbols_;
    Symbol_Table<> symbols_{};
  public:
    Symbol_Table<> globals_{};
};

// clang-format on

inline ITA::Instructions make_ITA_instructions(
    util::AST_Node const& internals_symbols,
    util::AST_Node const& definitions)
{
    return ITA::make_ITA_instructions(internals_symbols, definitions);
}

std::pair<std::string, std::string> get_rvalue_from_mov_qaudruple(
    ITA::Quadruple const& instruction);

} // namespace ir

} // namespace credence