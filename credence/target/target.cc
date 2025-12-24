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

#include "target.h"
#include "x86_64/runtime.h" // for runtime
#include <credence/util.h>  // for AST_Node

namespace credence::target {

/**
 * @brief Add the standard library and platform syscall routines to symbols
 */
void add_stdlib_functions_to_symbols(util::AST_Node& symbols, Platform platform)
{
    switch (platform) {
        case Platform::credence_x86_64_platform:
            x86_64::runtime::add_stdlib_functions_to_symbols(symbols);
            break;
        default:
            x86_64::runtime::add_stdlib_functions_to_symbols(symbols);
    }
}

} // namespace target