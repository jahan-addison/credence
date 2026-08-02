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

#include "ast.h" // for Operator, Type

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

/**
 * @brief Which member of Node::Data a node type keeps live
 *
 * The switch is exhaustive and without a default, so that a
 * new Type fails to compile here until its payload is declared.
 */
Payload payload_of(Type type)
{
    switch (type) {
        case Type::Program:
        case Type::Function_Definition:
        case Type::Union_Definition:
        case Type::Vector_Definition:
        case Type::Block_Statement:
        case Type::Expression_Statement:
        case Type::If_Statement:
        case Type::Ternary_Expression:
        case Type::Extrn_Statement:
        case Type::Auto_Statement:
            return Payload::Span;

        case Type::While_Statement:
        case Type::Switch_Statement:
        case Type::Case_Statement:
        case Type::Function_Expression:
        case Type::Binary_Expression:
        case Type::Assignment_Expression:
        case Type::Vector_Identifier:
            return Payload::Binary;

        case Type::Return_Statement:
        case Type::Unary_Expression:
        case Type::Evaluated_Expression:
        case Type::Address_Of_Expression:
        case Type::Post_Inc_Dec_Expression:
        case Type::Pre_Inc_Dec_Expression:
        case Type::Indirect_Identifier:
            return Payload::Unary;

        case Type::Goto_Statement:
        case Type::Label_Statement:
        case Type::Identifier:
        case Type::Char_Literal:
        case Type::String_Literal:
        case Type::Bool_Literal:
            return Payload::String;

        case Type::Integer_Literal:
            return Payload::Integer;

        case Type::Double_Literal:
        case Type::Float_Literal:
            return Payload::Real;

        case Type::Break_Statement:
            return Payload::None;
    }
    return Payload::None;
}

std::string_view type_to_string(Type type)
{
    switch (type) {
        case Type::Program:
            return "program";
        case Type::Function_Definition:
            return "function_definition";
        case Type::Union_Definition:
            return "union_definition";
        case Type::Vector_Definition:
            return "vector_definition";
        case Type::Block_Statement:
            return "block_statement";
        case Type::Expression_Statement:
            return "expression_statement";
        case Type::While_Statement:
            return "while_statement";
        case Type::If_Statement:
            return "if_statement";
        case Type::Switch_Statement:
            return "switch_statement";
        case Type::Case_Statement:
            return "case_statement";
        case Type::Goto_Statement:
            return "goto_statement";
        case Type::Label_Statement:
            return "label_statement";
        case Type::Return_Statement:
            return "return_statement";
        case Type::Extrn_Statement:
            return "extrn_statement";
        case Type::Auto_Statement:
            return "auto_statement";
        case Type::Break_Statement:
            return "break_statement";
        case Type::Function_Expression:
            return "function_expression";
        case Type::Ternary_Expression:
            return "ternary_expression";
        case Type::Binary_Expression:
            return "binary_expression";
        case Type::Unary_Expression:
            return "unary_expression";
        case Type::Evaluated_Expression:
            return "evaluated_expression";
        case Type::Address_Of_Expression:
            return "address_of_expression";
        case Type::Post_Inc_Dec_Expression:
            return "post_inc_dec_expression";
        case Type::Pre_Inc_Dec_Expression:
            return "pre_inc_dec_expression";
        case Type::Assignment_Expression:
            return "assignment_expression";
        case Type::Identifier:
            return "identifier";
        case Type::Indirect_Identifier:
            return "indirect_identifier";
        case Type::Vector_Identifier:
            return "vector_identifier";
        case Type::Integer_Literal:
            return "integer_literal";
        case Type::Double_Literal:
            return "double_literal";
        case Type::Float_Literal:
            return "float_literal";
        case Type::Char_Literal:
            return "char_literal";
        case Type::String_Literal:
            return "string_literal";
        case Type::Bool_Literal:
            return "bool_literal";
    }
    return "unknown";
}

std::string_view operator_to_string(Operator op)
{
    switch (op) {
        case Operator::None:
            return "";
        case Operator::Or:
            return "||";
        case Operator::And:
            return "&&";
        case Operator::Bit_Or:
            return "|";
        case Operator::Bit_And:
            return "&";
        case Operator::Xor:
            return "^";
        case Operator::Eq:
            return "==";
        case Operator::Neq:
            return "!=";
        case Operator::Lt:
            return "<";
        case Operator::Lte:
            return "<=";
        case Operator::Gt:
            return ">";
        case Operator::Gte:
            return ">=";
        case Operator::Lshift:
            return "<<";
        case Operator::Rshift:
            return ">>";
        case Operator::Add:
            return "+";
        case Operator::Sub:
            return "-";
        case Operator::Mul:
            return "*";
        case Operator::Div:
            return "/";
        case Operator::Mod:
            return "%";
        case Operator::Minus:
            return "-";
        case Operator::Plus:
            return "+";
        case Operator::Not:
            return "!";
        case Operator::Ones_Complement:
            return "~";
        case Operator::Address_Of:
            return "&";
        case Operator::Indirection:
            return "*";
        case Operator::Inc:
            return "++";
        case Operator::Dec:
            return "--";
        case Operator::Assign:
            return "=";
    }
    return "unknown";
}

} // namespace credence::frontend::ast
