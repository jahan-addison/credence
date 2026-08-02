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

#include <credence/frontend/ast.h>      // for String_Index
#include <credence/frontend/hir/type.h> // for Type_Index
#include <cstdint>                      // for uint32_t
#include <vector>                       // for vector

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

// Handle into Symbol_Table
using Symbol_Index = std::uint32_t;

// A name that is not declared
inline constexpr Symbol_Index null_symbol_index = 0xFFFFFFFFu;

/**
 * @brief Where a symbol lives and how it may be used
 */
enum class Storage : std::uint32_t
{
    Auto,      // a local declared with auto
    Extrn,     // a name declared with extrn
    Parameter, // a function parameter
    Function,  // a function definition
    Vector,    // a vector definition, local or global
    Label,     // a goto target
    Global     // a definition at file scope that is not a function
};

/**
 * @brief One declared name
 */
struct Symbol
{
    ast::String_Index name{ ast::null_string_index };
    Type_Index type{ null_type_index };
    Storage storage{ Storage::Auto };

    // the declared length of a vector, and zero elsewhere
    std::uint32_t count{ 0 };

    // the scope depth the name was declared at, where zero is file scope
    std::uint32_t depth{ 0 };

    /**
     * Whether the declaration wrote the name with a leading "*". Type
     * inference may later give a scalar a pointer type, so what the
     * program asked for is kept apart from what was worked out.
     */
    bool indirect{ false };

    /**
     * Whether nothing declared the name and a call assumed it. The
     * standard library adds its own names after the frontend has run, so
     * such a name may still resolve, and only the IR can tell.
     */
    bool assumed{ false };
};

/**
 * @brief The declared names of a translation unit, in a stack of scopes
 */
class Symbol_Table
{
  public:
    Symbol_Table();

    /**
     * @brief Open a nested scope
     */
    void push_scope();

    /**
     * @brief Close the innermost scope, keeping its symbols addressable
     */
    void pop_scope();

    /**
     * @brief Declare a name in the innermost scope
     *
     * Redeclaring a name already visible in the same scope returns the
     * existing handle, so that the caller can report it instead of
     * silently shadowing.
     */
    Symbol_Index declare(ast::String_Index name,
        Type_Index type,
        Storage storage,
        std::uint32_t count = 0,
        bool indirect = false,
        bool assumed = false);

    /**
     * @brief The handle of a visible name, or null_symbol_index
     */
    Symbol_Index lookup(ast::String_Index name) const;

    /**
     * @brief Whether a name is already declared in the innermost scope
     */
    bool declared_in_current_scope(ast::String_Index name) const;

    Symbol const& at(Symbol_Index index) const { return symbols_[index]; }
    Symbol& at(Symbol_Index index) { return symbols_[index]; }

    std::vector<Symbol> const& symbols() const { return symbols_; }
    std::uint32_t depth() const { return depth_; }

  private:
    std::vector<Symbol> symbols_;

    // the first symbol belonging to each open scope, innermost last
    std::vector<std::uint32_t> scopes_;
    std::uint32_t depth_{ 0 };
};

/**
 * @brief The name of a storage class, as diagnostics print it
 */
std::string_view storage_to_string(Storage storage);

} // namespace credence::frontend::hir
