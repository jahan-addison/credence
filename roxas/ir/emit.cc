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

#include <map>       // for map
#include <matchit.h> // for match, pattern, MatchHelper, Pattern...
#include <ostream>   // for basic_ostream, operator<<, ostringst...
#include <roxas/ir/emit.h>
#include <roxas/ir/operators.h> // for Operator, operator<<
#include <roxas/ir/types.h>     // for Type_, Value_Type, Instruction, Inst...
#include <roxas/util.h>         // for overload
#include <sstream>              // for basic_ostringstream
#include <string>               // for char_traits, allocator, operator<<
#include <string_view>          // for operator<<, string_view
#include <tuple>                // for get, tuple
#include <utility>              // for pair
#include <variant>              // for get, visit, monostate
#include <vector>               // for vector

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

inline void emit_equal(Instruction const& inst, std::ostream& os)
{
    auto [op, arg1, arg2, arg3, arg4] = inst;
    os << arg1 << " = " << arg2 << Operator::EOL;
}

std::string emit_value(Value_Type const& type, std::string_view separator)
{
    std::ostringstream os;
    os << "(";
    std::visit(util::overload{
                   [&](int i) {
                       os << i << separator << Type_["int"].first << separator
                          << Type_["int"].second;
                   },
                   [&](long i) {
                       os << i << separator << Type_["long"].first << separator
                          << Type_["long"].second;
                   },
                   [&](float i) {
                       os << i << separator << Type_["float"].first << separator
                          << Type_["float"].second;
                   },
                   [&](double i) {
                       os << i << separator << Type_["double"].first
                          << separator << Type_["double"].second;
                   },
                   [&](bool i) {
                       os << std::boolalpha << i << separator
                          << Type_["bool"].first << separator
                          << Type_["bool"].second;
                   },
                   [&]([[maybe_unused]] std::monostate i) {
                       os << "null" << separator << Type_["null"].first
                          << separator << Type_["null"].second;
                   },
                   [&](type::Byte i) {
                       os << i << separator << Type_["byte"].first << separator
                          << type.second.second;
                   },
                   [&](char i) {
                       os << i << Type_["char"].first << separator
                          << Type_["char"].second;
                   },
                   [&]([[maybe_unused]] std::string const& s) {
                       if (s == "__WORD_") {
                           // pointer
                           os << "__WORD_" << separator << Type_["word"].first
                              << separator << Type_["word"].second;
                       } else {
                           os << std::get<std::string>(type.first) << separator
                              << "string" << separator
                              << std::get<std::string>(type.first).size();
                       }
                   },
               },
               type.first);
    os << ")";
    return os.str();
}

} // namespace ir

} // namespace roxas