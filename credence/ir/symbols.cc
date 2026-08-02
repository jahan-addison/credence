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

#include <credence/ir/symbols.h>

#include <string> // for string

/****************************************************************************
 *
 * Hoisted symbol table
 *
 * The name to shape map that the object table and the backends read while
 * placing storage. It is a boundary format and not a working structure, and
 * its entire contract is four operations:
 *
 *    dump_keys()   every declared name
 *    ["type"]      what kind of storage the name needs
 *    ["size"]      how many elements a vector holds
 *    has_key()     whether a name was declared at all
 *
 * Keeping it in this form lets the standard library add its own names to it
 * without reaching into the string arena of the frontend, and a change to
 * the passes above shows up in code generation as a difference in
 * instructions and not a difference in format.
 *
 * The shapes a name may take:
 *
 *    function_definition   a function defined in this unit
 *    vector_definition     a vector defined at file scope, with a size
 *    vector_lvalue         a vector declared inside a function, with a size
 *    indirect_lvalue       a pointer
 *    lvalue                a scalar
 *    label                 a goto target
 *
 *****************************************************************************/

namespace credence::ir {

namespace hir = credence::frontend::hir;

namespace {

/**
 * @brief The shape a declared name takes in the table
 *
 * A vector keeps the same shape whether it was defined at file scope or
 * declared inside a function, since the object table tells the two apart by
 * where it meets them and not by what they are called.
 */
std::string_view shape_of(hir::Symbol const& symbol)
{
    switch (symbol.storage) {
        case hir::Storage::Function:
            return "function_definition";
        case hir::Storage::Vector:
            return symbol.depth == 0 ? "vector_definition" : "vector_lvalue";
        case hir::Storage::Label:
            return "label";
        default:
            break;
    }

    if (symbol.indirect)
        return "indirect_lvalue";

    return "lvalue";
}

/**
 * @brief Whether a function definition ends with a return statement
 *
 * The IR reads this to decide whether a call to the name leaves a value
 * behind, so it has to be recorded for every function this unit defines.
 */
bool ends_with_return(hir::Unit const& unit, hir::Node_Index definition)
{
    auto span = unit.nodes[definition].data.span;
    if (span.count == 0)
        return false;

    // [name, parameter..., body]
    auto body = unit.extra[span.start + span.count - 1];
    if (unit.nodes[body].type != hir::Type::Block)
        return false;

    auto statements = unit.nodes[body].data.span;
    if (statements.count == 0)
        return false;

    auto last = unit.extra[statements.start + statements.count - 1];
    return unit.nodes[last].type == hir::Type::Return;
}

} // namespace

util::AST_Node hoisted_symbols(hir::Unit const& unit)
{
    auto table = util::AST::object();

    for (auto const& symbol : unit.symbol_table.symbols()) {
        auto name = std::string{ unit.string(symbol.name) };
        if (name.empty())
            continue;

        // a name only a call assumed is not something this unit declares
        if (symbol.assumed)
            continue;

        auto shape = shape_of(symbol);

        // a name declared in more than one scope keeps the first shape it
        // was given, which is the one the object table placed storage for
        if (table.has_key(name))
            continue;

        table[name] = util::AST::object();
        auto& entry = table[name];
        entry["type"] = std::string{ shape };

        if (symbol.storage == hir::Storage::Vector)
            entry["size"] = static_cast<long>(symbol.count);
    }

    // a call reads this to know whether the name leaves a value behind
    for (auto definition : unit.definitions) {
        if (unit.nodes[definition].type != hir::Type::Function)
            continue;
        if (!ends_with_return(unit, definition))
            continue;
        auto span = unit.nodes[definition].data.span;
        auto name = std::string{ unit.symbol_name(
            unit.nodes[unit.extra[span.start]].data.symbol) };
        if (table.has_key(name))
            table[name]["void"] = false;
    }

    return table;
}

} // namespace credence::ir
