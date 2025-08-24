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
#pragma once

#include <list>              // for list
#include <roxas/operators.h> // for Operator
#include <roxas/types.h>     // for RValue
#include <variant>           // for variant
#include <vector>            // for vector

/**
 * o  o  o  TOOT  TOOT  o  o O  O             O  O o  o  TOOT  TOOT  o  o  o
 *                ,_____  ____    O         O    ____  _____.
 *                |     \_|[]|_'__Y         Y__`_|[]|_/     |
 *                |_______|__|_|__|}       {|__|_|__|_______|
 *================oo--oo==oo--OOO\\=======//OOO--oo==oo--oo==================
 *------------------------------------------------
 *
 */

namespace roxas {

using RValue_Queue =
    std::list<std::variant<type::Operator, type::RValue::Type_Pointer>>;

RValue_Queue* rvalues_to_queue(std::vector<type::RValue::Type_Pointer>& rvalues,
                               RValue_Queue* rvalues_queue);

RValue_Queue* rvalues_to_queue(type::RValue::Type_Pointer& rvalue,
                               RValue_Queue* rvalues_queue);

} // namespace roxas