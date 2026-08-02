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

#include <cstdint>     // for uint32_t, int64_t
#include <string_view> // for string_view
#include <vector>      // for vector

/****************************************************************************
 *
 * Flat AST
 *
 * The tree is a literal array of structs. A Node is a trivially copyable
 * 16 byte POD that owns nothing, and holds a Type tag with one 8 byte
 * payload of handles. There is no pointer, no std::string, and no
 * allocation anywhere inside a node.
 *
 * Everything a node needs to point at lives in a side arena:
 *
 *    - nodes, the tree itself, where a child is a Node_Index into this array
 *    - metadata, parallel to nodes, the source position of nodes[i]
 *    - extra, flattened child lists that a Span looks into
 *    - strings, interned string handles mapped to an offset and length
 *    - string_text, the interned character bytes
 *
 * Child lists, such as function parameters, block bodies, and switch cases,
 * are stored as a contiguous run in extra and referenced by a Span, so a
 * node of any arity still costs 8 bytes of payload.
 *
 * Identifiers and literals are interned, where equal text yields the same
 * String_Index handle. Comparing two names is a uint32_t compare, and the
 * text is stored exactly once.
 *
 * Nodes are appended in post-order, children before parents, so a bottom-up
 * pass over the array never visits a parent before its children.
 *
 *****************************************************************************/

namespace credence::frontend::ast {

// Handle into AST::nodes
using Node_Index = std::uint32_t;
// Handle into AST::strings
using String_Index = std::uint32_t;

// An absent child or an absent name
inline constexpr Node_Index null_node_index = 0xFFFFFFFFu;
inline constexpr String_Index null_string_index = 0xFFFFFFFFu;

enum class Type : std::uint32_t
{
    Program,

    Function_Definition,
    Union_Definition,
    Vector_Definition,

    Block_Statement,
    Expression_Statement,
    While_Statement,
    If_Statement,
    Switch_Statement,
    Case_Statement,
    Goto_Statement,
    Label_Statement,
    Return_Statement,
    Extrn_Statement,
    Auto_Statement,
    Break_Statement,

    Function_Expression,
    Ternary_Expression,
    Binary_Expression,
    Unary_Expression,
    Evaluated_Expression,
    Address_Of_Expression,
    Post_Inc_Dec_Expression,
    Pre_Inc_Dec_Expression,
    Assignment_Expression,

    Identifier,
    Indirect_Identifier,
    Vector_Identifier,

    Integer_Literal,
    Double_Literal,
    Float_Literal,
    Char_Literal,
    String_Literal,
    Bool_Literal
};

/**
 * @brief The operator carried by an expression node
 *
 * Kept out of Type so that an operator node still has both payload slots
 * free for its operands.
 */
enum class Operator : std::uint32_t
{
    None,

    // binary
    Or,
    And,
    Bit_Or,
    Bit_And,
    Xor,
    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte,
    Lshift,
    Rshift,
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    // unary
    Minus,
    Plus,
    Not,
    Ones_Complement,
    Address_Of,
    Indirection,

    Inc,
    Dec,

    Assign
};

/**
 * @brief Which member of Node::Data a node type keeps live
 *
 * Every pass that walks the tree switches on this instead of repeating
 * the type to member mapping. Adding a Type without naming it in
 * payload_of is a compile error, so the two cannot drift apart.
 */
enum class Payload : std::uint32_t
{
    None,
    Binary,
    Span,
    Unary,
    String,
    Integer,
    Real
};

Payload payload_of(Type type);
std::string_view operator_to_string(Operator op);
std::string_view type_to_string(Type type);

/**
 * @brief Source location of the node at the same index in AST::nodes
 */
struct Meta
{
    std::uint32_t start_pos;
    std::uint32_t size;
    std::uint32_t line;
    std::uint32_t column;
};

/**
 * @brief A contiguous run of child indices inside AST::extra
 */
struct Span
{
    std::uint32_t start;
    std::uint32_t count;
};

/**
 * @brief One tree node, a tag and 8 bytes of handles and nothing else
 *
 * Which union member is live is determined entirely by the type tag. See
 * the notes on each member below.
 */
struct Node
{
    Type type;
    // a node is trivially constructible on purpose, so the arrays it
    // lives in cost nothing to grow. Every field is written at append
    // cppcheck-suppress uninitMemberVarNoCtor
    Operator op; // None unless the node carries an operator

    union Data
    {
        /**
         * Binary_Expression      lhs and rhs operands
         * Assignment_Expression  lhs target, rhs value
         * Vector_Identifier      lhs base, rhs subscript
         * While_Statement        lhs condition, rhs body
         * Case_Statement         lhs constant, rhs body block
         * Function_Expression    lhs callee, rhs argument-list block
         * Switch_Statement       lhs condition, rhs case block
         */
        struct
        {
            Node_Index lhs;
            Node_Index rhs;
        } binary;

        /**
         * A child list living in AST::extra. Carries any arity, so nodes
         * with three or more fields use this instead of widening Node.
         * Where the order is fixed it is spelled out here:
         *
         * Function_Definition  [name identifier, parameters, body]
         * Vector_Definition    [name identifier, size or null, values]
         * Union_Definition     [name identifier, entry...]
         * If_Statement         [condition, then branch, else branch or null]
         * Ternary_Expression   [condition, then value, else value]
         *
         * Otherwise the run is a homogeneous list:
         *
         * Program, Block_Statement, Expression_Statement, Auto_Statement,
         * Extrn_Statement, and the blocks holding call arguments and
         * switch cases.
         */
        Span span;

        /**
         * Unary_Expression, Address_Of_Expression, Pre_Inc_Dec_Expression,
         * Post_Inc_Dec_Expression, Evaluated_Expression,
         * Indirect_Identifier, Return_Statement
         */
        Node_Index unary;

        /**
         * Interned text.
         *
         * Identifier, Label_Statement, Goto_Statement, String_Literal,
         * Char_Literal, Bool_Literal
         */
        String_Index string;

        // Inline literal payloads
        std::int64_t integer;
        double real;
        // cppcheck-suppress uninitMemberVarNoCtor
    } data;
};

static_assert(sizeof(Node) == 16, "Node must stay a compact POD");

/**
 * @brief An interned string as a window into AST::string_text
 */
struct String_Entry
{
    std::uint32_t offset;
    std::uint32_t length;
};

/**
 * @brief The whole tree as parallel arenas with no per-node ownership
 */
struct AST
{
    std::vector<Node> nodes;
    std::vector<Meta> metadata;    // parallel to nodes
    std::vector<Node_Index> extra; // flattened child lists
    std::vector<String_Entry> strings;
    std::vector<char> string_text;

    Node_Index root{ null_node_index };

    /**
     * @brief Text behind an interned handle
     */
    std::string_view string(String_Index handle) const
    {
        if (handle == null_string_index or handle >= strings.size())
            return {};
        auto const& entry = strings[handle];
        return std::string_view(
            string_text.data() + entry.offset, entry.length);
    }

    /**
     * @brief The child indices a Span refers to
     */
    Node_Index const* children(Span span) const
    {
        return span.count == 0 ? nullptr : extra.data() + span.start;
    }
};

static_assert(alignof(Node) == 8, "Node is not aligned to 8-byte boundaries");

} // namespace credence::frontend::ast
