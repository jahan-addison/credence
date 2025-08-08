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

#include <ostream> // for basic_ostream
#include <roxas/ir/emit.h>
#include <string> // for basic_string
#include <tuple>  // for get

#include <roxas/ir/operators.h> // for operator<<
#include <roxas/ir/types.h>     // for Instructions

namespace roxas {

namespace ir {

void emit(Instructions instructions, std::ostream& os)
{
    for (auto const& inst : instructions) {
        auto op = std::get<0>(inst);
        os << op;
    }
}

} // namespace ir

} // namespace roxas