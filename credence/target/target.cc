/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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