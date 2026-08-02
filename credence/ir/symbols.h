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

#include <credence/frontend/hir/hir.h> // for Unit
#include <credence/util.h>             // for AST_Node
#include <easyjson.h>                  // for JSON, object

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

/**
 * @brief Build the hoisted symbol table of a lowered unit
 */
util::AST_Node hoisted_symbols(frontend::hir::Unit const& unit);

} // namespace credence::ir
