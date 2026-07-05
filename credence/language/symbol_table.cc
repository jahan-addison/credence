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

#include <credence/language/symbol_table.h>

#include <easyjson.h> // for JSON
#include <matchit.h>  // for match, pattern

namespace credence::language {

namespace m = matchit;

/**
 * @brief Recurse into "left"/"right" before dispatching on the node's shape
 *
 * Children are visited before their parent acts on them, matching the
 * bottom-up order augur's Lark transform.
 */
void Symbol_Table_Builder::visit(util::AST_Node const& node)
{
    m::match(node.JSON_type())(
        m::pattern | util::AST_Node::Class::Array =
            [&] {
                for (auto& child : node.array_range())
                    visit(child);
            },
        m::pattern | util::AST_Node::Class::Object =
            [&] {
                if (node.has_key("left"))
                    visit(node["left"]);
                if (node.has_key("right"))
                    visit(node["right"]);

                m::match(node["node"].to_string())(
                    m::pattern | "function_definition" =
                        [&] { visit_function_definition(node); },
                    m::pattern | "vector_definition" =
                        [&] { visit_vector_definition(node); },
                    m::pattern | "lvalue" = [&] { visit_lvalue(node); },
                    m::pattern | "indirect_lvalue" =
                        [&] { visit_indirect_lvalue(node); },
                    m::pattern |
                        "vector_lvalue" = [&] { visit_vector_lvalue(node); },
                    m::pattern | "statement" =
                        [&] {
                            if (node["root"].to_string() == "label")
                                visit_label_statement(node);
                        },
                    m::pattern | m::_ = [&] {});
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Create a function_definition entry and whether it returns a value
 */
void Symbol_Table_Builder::visit_function_definition(util::AST_Node const& node)
{
    auto name = node["root"].to_string();
    auto body = node["right"]["left"].to_deque();

    table_[name] = util::AST::object();
    auto& entry = table_[name];
    entry["type"] = "function_definition";
    // augur only ever sets "void": false when the body's last statement is
    // a return. There is no corresponding "void": true case, so a function
    // without a trailing return simply has no "void" key at all.
    if (body.size() > 0 && body.back()["root"].to_string() == "return")
        entry["void"] = false;
}

/**
 * @brief Create a vector_definition entry and its size, if given
 */
void Symbol_Table_Builder::visit_vector_definition(util::AST_Node const& node)
{
    auto name = node["root"].to_string();
    table_[name] = util::AST::object();
    auto& entry = table_[name];
    entry["type"] = "vector_definition";
    if (node.has_key("left"))
        entry["size"] = node["left"]["root"].to_int();
}

/**
 * @brief Create a label entry
 */
void Symbol_Table_Builder::visit_label_statement(util::AST_Node const& node)
{
    auto name = node["left"].to_deque().front().to_string();
    table_[name] = util::AST::object();
    table_[name]["type"] = "label";
}

/**
 * @brief Create a plain lvalue entry the first time a name is referenced
 */
void Symbol_Table_Builder::visit_lvalue(util::AST_Node const& node)
{
    auto name = node["root"].to_string();
    if (!table_.has_key(name)) {
        table_[name] = util::AST::object();
        table_[name]["type"] = "lvalue";
    }
}

/**
 * @brief Resolve a simple lvalue operand's entry to indirect_lvalue
 */
void Symbol_Table_Builder::visit_indirect_lvalue(util::AST_Node const& node)
{
    auto operand = node["left"];
    if (operand["root"].JSON_type() != util::AST_Node::Class::String)
        return;
    auto name = operand["root"].to_string();
    if (table_.has_key(name))
        table_[name]["type"] = "indirect_lvalue";
}

/**
 * @brief Resolve a vector base name's entry to vector_lvalue and its size
 *
 * Parser::parse_lvalue collapses a vector's base identifier into a plain
 * string, so unlike indirect_lvalue there is no separate lvalue node left
 * in the tree to have already seeded this entry.
 */
void Symbol_Table_Builder::visit_vector_lvalue(util::AST_Node const& node)
{
    auto name = node["root"].to_string();
    if (!table_.has_key(name)) {
        table_[name] = util::AST::object();
        table_[name]["type"] = "lvalue";
    }
    auto& entry = table_[name];
    entry["type"] = "vector_lvalue";

    auto index = node["left"]["root"];
    if (index.JSON_type() == util::AST_Node::Class::Integral &&
        !entry.has_key("size"))
        entry["size"] = index.to_int();
}

} // namespace credence::language
