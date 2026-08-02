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

#include <credence/frontend/hir/symbol.h>

/****************************************************************************
 *
 * Symbols and scopes
 *
 * Every name a program declares becomes one Symbol, and every use of that
 * name in the HIR is a Symbol_Index and not a string. Resolution happens
 * once during lowering, so no later pass repeats a name lookup.
 *
 * Scopes are a stack of index ranges over one flat symbol array. Entering
 * a function pushes a scope, leaving it pops back to the enclosing one, and
 * a lookup walks the stack from the innermost outward. Popping does not
 * erase the symbols, so a resolved Symbol_Index stays valid for the life of
 * the unit even after its scope has closed.
 *
 * The storage class is what the backend needs to place a symbol, and what
 * the checker needs to reject a use that its declaration does not allow,
 * such as subscripting a scalar.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

Symbol_Table::Symbol_Table()
{
    // file scope, which is never popped
    scopes_.push_back(0);
}

void Symbol_Table::push_scope()
{
    scopes_.push_back(static_cast<std::uint32_t>(symbols_.size()));
    ++depth_;
}

/**
 * @brief Close the innermost scope, keeping its symbols addressable
 *
 * Only the boundary is dropped. The symbols stay in the array so that a
 * Symbol_Index resolved while the scope was open still names the same
 * declaration afterwards.
 */
void Symbol_Table::pop_scope()
{
    if (scopes_.size() <= 1)
        return;
    scopes_.pop_back();
    --depth_;
}

Symbol_Index Symbol_Table::declare(ast::String_Index name,
    Type_Index type,
    Storage storage,
    std::uint32_t count,
    bool indirect,
    bool assumed)
{
    if (auto existing = lookup(name);
        existing != null_symbol_index and symbols_[existing].depth == depth_) {
        return existing;
    }

    symbols_.push_back(
        Symbol{ name, type, storage, count, depth_, indirect, assumed });
    return static_cast<Symbol_Index>(symbols_.size() - 1);
}

/**
 * @brief The handle of a visible name, or null_symbol_index
 *
 * The search runs from the most recent declaration backwards, so an inner
 * scope shadows an outer one, and only symbols belonging to a scope that
 * is still open are considered.
 */
Symbol_Index Symbol_Table::lookup(ast::String_Index name) const
{
    if (symbols_.empty())
        return null_symbol_index;

    for (auto index = static_cast<Symbol_Index>(symbols_.size());
        index-- > 0;) {
        if (symbols_[index].name != name)
            continue;
        if (symbols_[index].depth > depth_)
            continue; // belongs to a scope that has closed
        return index;
    }
    return null_symbol_index;
}

bool Symbol_Table::declared_in_current_scope(ast::String_Index name) const
{
    auto found = lookup(name);
    return found != null_symbol_index and symbols_[found].depth == depth_;
}

std::string_view storage_to_string(Storage storage)
{
    switch (storage) {
        case Storage::Auto:
            return "auto";
        case Storage::Extrn:
            return "extrn";
        case Storage::Parameter:
            return "parameter";
        case Storage::Function:
            return "function";
        case Storage::Vector:
            return "vector";
        case Storage::Label:
            return "label";
        case Storage::Global:
            return "global";
    }
    return "unknown";
}

} // namespace credence::frontend::hir
