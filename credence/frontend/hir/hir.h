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

#include <credence/frontend/ast.h>        // for AST, Operator, Meta
#include <credence/frontend/hir/symbol.h> // for Symbol_Table, Symbol_Index
#include <credence/frontend/hir/type.h>   // for Type_Table, Type_Index
#include <cstdint>                        // for uint32_t, int64_t
#include <string>                         // for string
#include <vector>                         // for vector

/****************************************************************************
 *
 * High level intermediate representation
 *
 * The AST with precedence resolved, names bound to symbols, and useless
 * syntax dropped. Keeps the shape of the AST, a flat array of trivially
 * copyable nodes with parallel arrays beside it, so a pass is a loop and
 * not a recursive walk.
 *
 *    nodes    the tree itself
 *    types    parallel to nodes, the type of nodes[i]
 *    metadata parallel to nodes, the source position of nodes[i]
 *    first    parallel to nodes, the first node of the subtree at i
 *    extra    flattened child lists that a Span indexes into
 *
 * Three things change on the way in from the AST:
 *
 *    - a chain such as "a * b + c" is reshaped to respect precedence,
 *      which the parser leaves alone
 *    - an identifier becomes a Symbol_Index, resolved once
 *    - parentheses and the blocks holding call arguments are dropped, as
 *      neither survives precedence resolution
 *
 * Nodes stay in post-order, children before parents, which gives the passes
 * two properties:
 *
 *    - a subtree is a contiguous range, first[i] through i, extracted in
 *      constant time
 *    - a bottom-up pass is a forward loop, as every operand of a node has
 *      already been visited when the node is reached
 *
 * The second lets the IR read an expression as a stack machine. Post-order
 * over an expression with correct precedence is the order an operand stack
 * consumes, so ir/hir_queue.cc walks first[i] through i and finds its
 * operands in the order it pops them.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

// Handle into Unit::nodes
using Node_Index = std::uint32_t;

// An absent child
inline constexpr Node_Index null_node_index = 0xFFFFFFFFu;

/**
 * @brief The node vocabulary, smaller than the AST it comes from
 */
enum class Type : std::uint32_t
{
    // definitions
    Function,
    Vector,
    Union,

    // statements
    Block,
    Expression,
    If,
    While,
    Switch,
    Case,
    Goto,
    Label,
    Return,
    Break,
    Auto,
    Extrn,
    Declaration,

    // expressions
    Binary,
    Unary,
    Assign,
    Ternary,
    Call,
    Subscript,
    Address_Of,
    Dereference,
    Pre_Inc_Dec,
    Post_Inc_Dec,

    // leaves
    Symbol_Ref,
    Integer,
    Double,
    Float,
    String,
    Char,
    Bool
};

/**
 * @brief A contiguous run of child indices inside Unit::extra
 */
struct Span
{
    std::uint32_t start;
    std::uint32_t count;
};

/**
 * @brief One HIR node, the same shape of payload as the AST node
 */
struct Node
{
    Type type;
    // cppcheck-suppress uninitMemberVarNoCtor
    ast::Operator op; // None unless the node carries an operator

    union Data
    {
        /**
         * Binary, Assign, Subscript, While, Case, Switch, Call
         */
        struct
        {
            Node_Index lhs;
            Node_Index rhs;
        } binary;

        /**
         * A child list living in Unit::extra.
         *
         * Function  [name, parameter..., body]
         * Vector    [name, value...]
         * If        [condition, then, else or null]
         * Ternary   [condition, then, else]
         * Block, Expression, Union
         */
        Span span;

        /**
         * Unary, Address_Of, Dereference, Pre_Inc_Dec, Post_Inc_Dec,
         * Return
         */
        Node_Index unary;

        // a resolved name, for Symbol_Ref, Label, Goto, Declaration
        Symbol_Index symbol;

        // interned text, for String
        ast::String_Index string;

        // inline literal payloads
        std::int64_t integer;
        double real;
        // cppcheck-suppress uninitMemberVarNoCtor
    } data;
};

static_assert(sizeof(Node) == 16, "Node must stay a compact POD");

/**
 * @brief Which member of Node::Data a node type keeps live
 */
enum class Payload : std::uint32_t
{
    None,
    Binary,
    Span,
    Unary,
    Symbol,
    String,
    Integer,
    Double
};

Payload payload_of(Type type);
std::string_view type_to_string(Type type);

// A subscript whose offset is not a compile time constant
inline constexpr std::uint32_t unknown_offset = 0xFFFFFFFFu;

/**
 * @brief One lowered translation unit
 */
struct Unit
{
    std::vector<Node> nodes;
    std::vector<Type_Index> types;   // parallel to nodes
    std::vector<ast::Meta> metadata; // parallel to nodes
    std::vector<Node_Index> first;   // parallel to nodes, subtree start
    std::vector<Node_Index> extra;   // flattened child lists

    /**
     * Parallel to nodes. The byte offset of a subscript whose index is a
     * constant, and unknown_offset everywhere else. Filled by the address
     * pass, so that a backend reads a number without working the offset
     * out again from the name of the symbol.
     */
    std::vector<std::uint32_t> offsets;

    Type_Table type_table;
    Symbol_Table symbol_table;

    // the interned text of the AST this was lowered from
    std::vector<ast::String_Entry> strings;
    std::vector<char> string_text;

    // the top level definitions, in source order
    std::vector<Node_Index> definitions;

    /**
     * Every string literal the unit holds, in the order it was reached.
     * Collected by the address pass, since a literal needs storage before
     * the backend can take its address.
     */
    std::vector<ast::String_Index> string_literals;

    /**
     * @brief Text behind an interned handle
     */
    std::string_view string(ast::String_Index handle) const;

    /**
     * @brief The name of a resolved symbol
     */
    std::string_view symbol_name(Symbol_Index index) const;

    /**
     * @brief The subtree at a node, as the range first through index
     *
     * The nodes in that range are in the order an operand stack consumes
     * them, so it doubles as the linear form of an expression.
     */
    Span subtree(Node_Index index) const;
};

/**
 * @brief One diagnostic raised by lowering or checking
 */
struct Diagnostic
{
    std::string message;
    std::uint32_t line{ 0 };
    std::uint32_t column{ 0 };
};

/**
 * @brief The result of a frontend pass
 */
struct Result
{
    Unit unit;
    std::vector<Diagnostic> diagnostics;

    bool failed() const { return !diagnostics.empty(); }
};

namespace detail {

/**
 * @brief The pass that walks a parsed tree into the HIR
 *
 * Resolves precedence, binds every name to a symbol, and drops the syntax
 * that has no meaning. See lower() for the entry point.
 */
class Lowering
{
  public:
    Lowering() = delete;
    explicit Lowering(ast::AST const& tree)
        : tree_(tree)
    {
    }

    Result run();

  private:
    void hoist_definition(ast::Node_Index index);
    Node_Index lower_definition(ast::Node_Index index);
    Node_Index lower_function(ast::Node_Index index);
    Node_Index lower_vector(ast::Node_Index index);
    Node_Index lower_union(ast::Node_Index index);

    Node_Index lower_statement(ast::Node_Index index);
    Node_Index lower_block(ast::Node_Index index);
    Node_Index lower_expression(ast::Node_Index index);

    Node_Index lower_binary_chain(ast::Node_Index index);
    Node_Index lower_dereference(ast::Node_Index index);
    Node_Index lower_call(ast::Node_Index index);

    /**
     * @brief Declare the names an auto or extrn statement introduces
     */
    Node_Index lower_declaration(ast::Node_Index index, Storage storage);

  private:
    Node_Index add(Node node, ast::Node_Index source);
    Span commit(std::size_t scratch_base);
    void error(ast::Node_Index source, std::string message);

    ast::Node const& node_at(ast::Node_Index index) const
    {
        return tree_.nodes[index];
    }

    /**
     * @brief The interned name of an AST identifier
     */
    ast::String_Index name_of(ast::Node_Index index) const
    {
        return tree_.nodes[index].data.string;
    }

  private:
    ast::AST const& tree_;
    Unit unit_{};
    std::vector<Diagnostic> diagnostics_{};
    std::vector<Node_Index> scratch_{};
};

} // namespace detail

/**
 * @brief Lower a parsed tree into the HIR
 *
 * Resolves precedence, binds names, and drops the syntax that has no
 * meaning. Types are left unresolved for the checker to fill in.
 */
Result lower(ast::AST const& tree);

/**
 * @brief The precedence of an operator, where a lower number binds tighter
 */
unsigned int precedence_of(ast::Operator op);

/**
 * @brief Whether an operator groups from the left
 */
bool is_left_associative(ast::Operator op);

} // namespace credence::frontend::hir
