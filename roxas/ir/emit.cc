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

#include <matchit.h>            // for match, pattern, MatchHelper
#include <ostream>              // for char_traits, ostream
#include <roxas/ir/emit.h>      // emit_equal, etc
#include <roxas/ir/operators.h> // for Operato
#include <roxas/ir/types.h>     // for Instructions
#include <tuple>                // for get, tuple

namespace roxas {

namespace ir {

void emit(Instructions const& instructions, std::ostream& os)
{
    using namespace matchit;
    for (auto const& inst : instructions) {
        /* clang-format off */
        match(std::get<0>(inst))(
            pattern | Operator::EQUAL = [&] { emit_equal(inst, os); }
        );
        /* clang-format on */
    }
}

constexpr void emit_equal(Instruction const& inst, std::ostream& os)
{
    auto [op, arg1, arg2, arg3, arg4] = inst;
    os << arg1 << " = " << arg2 << Operator::EOL;
}

} // namespace ir

} // namespace roxas