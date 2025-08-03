
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

#include <cassert>
#include <roxas/ir.h>

namespace roxas {

Quintdruple::Quint Quintdruple::get() noexcept
{
    return quintdruple_;
}

constexpr std::string_view Quintdruple::operator_to_string(Operator op) noexcept
{
    switch (op) {
        case Operator::LABEL:
        case Operator::VARIABLE:
        case Operator::NOOP:
            return "";

        case Operator::FUNC_START:
            return "BeginFunc";
        case Operator::FUNC_END:
            return "EndFunc";

        case Operator::MINUS:
            return "-";
        case Operator::PLUS:
            return "+";
        case Operator::LT:
            return "<";
        case Operator::GT:
            return ">";
        case Operator::LE:
            return "<=";
        case Operator::GE:
            return ">=";
        case Operator::XOR:
            return "^";
        case Operator::LSHIFT:
            return "<<";
        case Operator::RSHIFT:
            return ">>";
        case Operator::SUBTRACT:
            return "-";
        case Operator::ADD:
            return "+";
        case Operator::MOD:
            return "%";
        case Operator::MUL:
            return "*";
        case Operator::DIV:
            return "/";
        case Operator::INDIRECT:
            return "*";
        case Operator::ADDR_OF:
            return "&";
        case Operator::UMINUS:
            return "-";
        case Operator::UNOT:
            return "!";
        case Operator::UONE:
            return "~";
        case Operator::PUSH:
            return "Push";
        case Operator::POP:
            return "Pop";
        case Operator::CALL:
            return "Call";
        case Operator::GOTO:
            return "Goto";
        default:
            return "null";
    }
}

void Intermediate_Representation::from_number_literal(Node node)
{
    assert(node.hasKey("node"));
    assert(node["node"].ToString().compare("constant_literal") == 0);
}

} // namespace roxas