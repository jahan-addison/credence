/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include <compare> // for _CmpUnspecifiedParam, operator<, strong...
#include <credence/frontend/hir/hir.h> // for Unit, Node_Index
#include <credence/ir/operand.h>       // for Literal
#include <credence/ir/symbols.h>       // for hoisted_symbols
#include <credence/symbol.h>           // for Symbol_Table
#include <credence/util.h> // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>           // for deque, operator<=>, operator==
#include <easyjson.h>      // for JSON, object
#include <iomanip>         // for operator<<, setw
#include <optional>        // for nullopt, nullopt_t, optional
#include <source_location> // for source_location
#include <sstream>         // for basic_ostream, basic_ostringstream, ope...
#include <stack>           // for stack
#include <string>          // for basic_string, char_traits, allocator
#include <string_view>     // for string_view
#include <tuple>           // for tuple, get, make_tuple
#include <utility>         // for pair, make_pair
#include <vector>          // for vector

/****************************************************************************
 * Instruction Tuple Abstraction
 *
 * The intermediate representation (IR) is formalized as a linear four-tuple,
 * named the Instruction Tuple Abstraction (ITA). The ITA comprises a collection
 * of platform-independent instructions that approximate the structure and
 * semantics of a target machine language.
 *
 * See ir/readme.md for details.
 *
 *  Example transformation:
 *
 *  main() {
 *    auto x;
 *    x = 5 + 10;
 *  }
 *
 *  Becomes:
 *
 *  __main():
 *   BeginFunc ;
 *    LOCL x;
 *    _t1 = (5:int:4) + (10:int:4);
 *    MOV x _t1;
 *   EndFunc ;
 *
 ****************************************************************************/

namespace credence {

namespace ir {

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

using Quadruple =
    std::tuple<Instruction, std::string, std::string, std::string>;
using Instructions = std::deque<Quadruple>;

/**
 * @brief Create a qaudruple-tuple from 4 operands, 3 optional
 */
constexpr Quadruple make_quadruple(Instruction op,
    std::string const& s1 = "",
    std::string const& s2 = "",
    std::string const& s3 = "")
{
    return std::make_tuple(op, s1, s2, s3);
}

/**
 * @brief Insert instructions from one std::deque into another
 */
inline void insert(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

/**
 * @brief Create a temporary (e.g. _t5) lvalue from the current temporary
 * size Set as a Instruction::MOV instruction with the right-hamd-side
 */
constexpr inline Quadruple make_temporary(int* temporary_size,
    std::string const& temp)
{
    return make_quadruple(Instruction::MOV,
        std::string{ "_t" } +
            util::to_constexpr_string<int>(++(*temporary_size)),
        temp);
}

/**
 * @brief Create a temporary (e.g. _t5) lvalue from the current temporary
 * size Set as a standlone Instruction::LABEL instruction
 */
constexpr inline Quadruple make_temporary(int* temporary_size)
{
    return make_quadruple(Instruction::LABEL,
        std::string{ "_L" } +
            util::to_constexpr_string<int>(++(*temporary_size)),
        "");
}

namespace detail {

constexpr std::ostream& operator<<(std::ostream& os, Instruction const& op)
{
    switch (op) {
        case Instruction::FUNC_START:
            os << "BeginFunc";
            break;
        case Instruction::FUNC_END:
            os << "EndFunc";
            break;
        case Instruction::LABEL:
            break;
        case Instruction::MOV:
            os << "=";
            break;
        case Instruction::NOOP:
            os << "";
            break;
        case Instruction::CMP:
            os << "CMP";
            break;
        case Instruction::RETURN:
            os << "RET";
            break;
        case Instruction::GLOBL:
            os << "GLOBL";
            break;
        case Instruction::LOCL:
            os << "LOCL";
            break;
        case Instruction::LEAVE:
            os << "LEAVE";
            break;
        case Instruction::JMP_E:
            os << "JMP_E";
            break;
        case Instruction::IF:
            os << "IF";
            break;
        case Instruction::PUSH:
            os << "PUSH";
            break;
        case Instruction::POP:
            os << "POP";
            break;
        case Instruction::CALL:
            os << "CALL";
            break;
        case Instruction::GOTO:
            os << "GOTO";
            break;
        default:
            os << "null";
            break;
    }
    return os;
}

/**
 * @brief Use operator<< to implement instruction symbol to string
 */
inline std::string instruction_to_string(
    Instruction op) // not constexpr until C++23
{
    std::ostringstream os;
    os << op;
    return os.str();
}

/**
 * @brief Use std::ostringstream to implement qaudruple-tuple to string
 */
inline std::string quadruple_to_string(
    Quadruple const& ita) // not constexpr until C++23
{
    std::ostringstream os;
    os << std::setw(2) << std::get<1>(ita) << std::get<0>(ita)
       << std::get<2>(ita) << std::get<3>(ita);

    return os.str();
}

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
    using Last_Branch = std::optional<Quadruple>;
    using Branch_Comparator = std::pair<std::string, Instructions>;
    using Branch_Instructions = std::pair<Instructions, Instructions>;

  public:
    static constexpr auto BRANCH_STATEMENTS = { "if", "while", "case" };

  public:
    constexpr static bool is_branching_statement(std::string_view s);
    constexpr static bool last_instruction_is_jump(Quadruple const& inst);

  public:
    /**
     * @brief Construct root branch label and set to root block
     */
    constexpr inline void set_root_branch(int* temporary_index)
    {
        if (level == 1) {
            // _L1 label is reserved for function scope resume
            root_branch = make_quadruple(Instruction::LABEL,
                std::string{ "_L" } +
                    util::to_constexpr_string<int>(++(*temporary_index)),
                "");
            set_block_to_root();
        }
    }
    void increment_branch_level();
    void decrement_branch_level(bool not_branching = false);
    constexpr void teardown();
    Last_Branch get_parent_branch(bool last = false);
    constexpr inline bool is_root_level() { return level == 1; }
    constexpr inline bool is_branch_level() { return level > 1; }
    constexpr inline void set_block_to_root() { block_level = root_branch; }
    constexpr inline Last_Branch get_root_branch() { return root_branch; }

  public:
    std::stack<Last_Branch> stack{};

  private:
    Last_Branch root_branch;
    Last_Branch block_level;
    bool is_branching = false;
    int level = 1;
    int* temporary;
};

void emit_to(std::ostream& os, Quadruple const& ita, bool indent = false);

/**
 * @brief Emit a std::deque of instructions to a std::ostream
 *   If indent is true indent with a tab for formatting
 */
inline void emit(std::ostream& os,
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

} // namespace detail

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
    explicit ITA(frontend::hir::Unit const& unit)
        : unit_(&unit)
        , details_(hoisted_symbols(unit))
    {
    }

    /**
     * @brief Build against a symbol table the caller has already extended
     *
     * The standard library adds its own names after the frontend has run,
     * so a call resolves against what the caller knows and not against
     * the unit alone.
     */
    explicit ITA(frontend::hir::Unit const& unit, util::AST_Node const& details)
        : unit_(&unit)
        , details_(details)
    {
    }

  public:
    // a statement or an expression of the lowered unit
    using Node = frontend::hir::Node_Index;

  public:
    friend constexpr std::ostream& detail::operator<<(std::ostream& os,
        Instruction const& op);

  public:
    /**
     * @brief Create a temporary (e.g. _L5) label from the
     * current temporary size Set as a Instruction::LABEL
     */
    constexpr inline Quadruple make_temporary()
    {
        return make_quadruple(Instruction::LABEL,
            std::string{ "_L" } + util::to_constexpr_string<int>(++temporary),
            "");
    }

  public:
    using Instruction_Pair = std::pair<Symbol_Table<>, Instructions>;

  private:
    using Branch_Instructions = detail::Branch::Branch_Instructions;
    using Parameters = std::vector<std::string>;

  public:
    /**
     * @brief Emit Instructions_ field to an std::ostream
     */
    inline void emit(std::ostream& os)
    { // not constexpr until C++23
        for (auto const& i : instructions_)
            detail::emit_to(os, i);
    }
    /**
     * @brief Emit Instructions_ field to an std::ostream
     *   If indent is true indent with a tab for formatting
     */
    inline void emit(std::ostream& os, bool indent)
    { // not constexpr until C++23
        for (auto const& i : instructions_)
            detail::emit_to(os, i, indent);
    }

  public:
    /**
     * @brief Instructions factory methods
     */
    static inline Instructions make_ita_instructions(
        frontend::hir::Unit const& unit)
    {
        auto ita = ITA{ unit };
        return ita.build_from_definitions();
    }
    static inline Instruction_Pair make_ita_instructions_with_globals(
        frontend::hir::Unit const& unit,
        util::AST_Node const& details)
    {
        auto ita = ITA{ unit, details };
        return std::pair<Symbol_Table<>&, Instructions>(
            ita.globals_, ita.build_from_definitions());
    }

  public:
    Instructions build_from_definitions();

  private:
    std::string_view symbol_name_of(Node node) const;
    void check_call_is_resolvable(Node node);
    operand::Literal literal_of(Node node) const;

  public:
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_function_definition(Node node);
    void build_from_vector_definition(Node node);
    void build_from_union_definition(Node node);
    constexpr std::string build_function_label_from_parameters(
        std::string_view name,
        Parameters const& parameters);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_block_statement(
    Node node,
    bool root_function_scope = false);
  private:
    void build_statement_setup_branches(
        std::string_view type,
        Instructions& instructions);
    void build_statement_teardown_branches(
        std::string_view type,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Branch_Instructions build_from_switch_statement(
        Node node);
    Branch_Instructions build_from_case_statement(
        Node node,
        std::string const& switch_label,
        detail::Branch::Last_Branch const& tail);
    Branch_Instructions build_from_while_statement(
        Node node);
    Branch_Instructions build_from_if_statement(
        Node node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_label_statement(Node node);
    Instructions build_from_goto_statement(Node node);
    Instructions build_from_return_statement(Node node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_auto_statement(
        Node node,
        Instructions& instructions);
    void build_from_extrn_statement(
        Node node,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Instructions build_from_rvalue_statement(Node node);

  private:
    void insert_branch_block_instructions(
        Node block,
        Instructions& branch_instructions);
    void insert_branch_jump_and_resume_instructions(
        Node block,
        Instructions& predicate_instructions,
        Instructions& branch_instructions,
        Quadruple const& label,
        detail::Branch::Last_Branch const& tail = std::nullopt);

    std::string build_from_branch_comparator_rvalue(
        Node block,
        Instructions& instructions);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    int temporary{ 0 };
    int identifier{ 0 };
    // clang-format on

  private:
    void ita_error(std::string_view message,
        std::string_view symbol,
        std::source_location const& location = std::source_location::current());

  private:
    Instructions instructions_;

  private:
    detail::Branch branch{ &temporary };
    static detail::Branch::Branch_Instructions make_statement_instructions()
    {
        return std::make_pair(Instructions{}, Instructions{});
    }

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    inline void make_root_branch()
    {
        temporary = 0;
        branch.set_root_branch(&temporary);
    }
    frontend::hir::Unit const* unit_{ nullptr };

    // the shape of every declared name, which a call reads to know
    // whether it leaves a value behind
    util::AST_Node details_{};
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
};

// clang-format on

inline ITA::Instruction_Pair make_ita_instructions(
    frontend::hir::Unit const& unit,
    util::AST_Node const& details)
{
    return ITA::make_ita_instructions_with_globals(unit, details);
}

std::pair<std::string, std::string> get_rvalue_from_mov_qaudruple(
    Quadruple const& instruction);

/**
 * @brief Get the lvalue from an ita MOV instruction
 */
constexpr std::string get_lvalue_from_mov_qaudruple(
    Quadruple const& instruction)
{
    return std::get<1>(instruction);
}

} // namespace ir

} // namespace credence