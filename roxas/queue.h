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

namespace roxas {

using RValue_Evaluation_Queue =
    std::list<std::variant<type::Operator, type::RValue::Type_Pointer>>;

RValue_Evaluation_Queue* rvalues_to_queue(
    std::vector<type::RValue::Type_Pointer>& rvalues,
    RValue_Evaluation_Queue* rvalues_queue);

} // namespace roxas