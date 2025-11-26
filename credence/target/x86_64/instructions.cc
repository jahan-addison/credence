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

#include "instructions.h"
#include <credence/ir/table.h> // for Table
#include <credence/types.h>    // for RValue, LValue, ...
#include <matchit.h>           // for Match, Pattern

namespace credence::target::x86_64::detail {

namespace m = matchit;

Immediate get_result_from_trivial_relational_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    int result{ 0 };
    // note: operand type checking is done in the table
    auto type = type::get_type_from_local_lvalue(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    auto lhs_type = type::get_type_from_local_lvalue(lhs);
    auto rhs_type = type::get_type_from_local_lvalue(lhs);

    // clang-format off
    m::match(op) (
        make_trivial_immediate_binary_result(==),
        make_trivial_immediate_binary_result(!=),
        make_trivial_immediate_binary_result(<),
        make_trivial_immediate_binary_result(>),
        make_trivial_immediate_binary_result(&&),
        make_trivial_immediate_binary_result(||),
        make_trivial_immediate_binary_result(<=),
        make_trivial_immediate_binary_result(>=)
    );
    // clang-format on
    return detail::make_int_immediate(result, "byte");
}

std::string get_storage_as_string(Storage const& storage)
{
    std::ostringstream result{};
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](Stack_Offset const& s) {
                result << std::format("stack offset: {}", s);
            },
            [&](Register const& s) { result << s; },
            [&](Immediate const& s) {
                result << type::get_value_from_rvalue_data_type(s);
            },
        },
        storage);
    return result.str();
}

Immediate get_result_from_trivial_integral_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_local_lvalue(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result = trivial_arithmetic_from_numeric_table_type<int>(
            lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result = trivial_arithmetic_from_numeric_table_type<long>(
            lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<long>(result, "long");
    } else if (type == "float") {
        auto result = trivial_arithmetic_from_numeric_table_type<float>(
            lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<float>(result, "float");

    } else if (type == "double") {
        auto result = trivial_arithmetic_from_numeric_table_type<double>(
            lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<double>(result, "double");
    }
    credence_error("unreachable");
    return detail::make_int_immediate(0, "int");
}

Immediate get_result_from_trivial_bitwise_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_local_lvalue(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result =
            trivial_bitwise_from_numeric_table_type<int>(lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result =
            trivial_bitwise_from_numeric_table_type<long>(lhs_imm, op, rhs_imm);
        return detail::make_int_immediate<long>(result, "long");
    }
    credence_error("unreachable");
    return detail::make_int_immediate(0, "int");
}

Instruction_Pair add_2ary_inst(
    Mnemonic mnemonic,
    Storage const& dest,
    Storage const& src)
{
    auto instructions = make();
    add_i(instructions, mnemonic, dest, src);
    return { dest, instructions };
}

Instruction_Pair add_1ary_inst(Mnemonic mnemonic, Storage const& src)
{
    auto instructions = make();
    add_i(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(imul), dest, src);
}

Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_e(inst, cdq);
    add_i_s(inst, mov, dest, src);
    add_i_d(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_e(inst, cdq);
    add_i_s(inst, mov, dest, src);
    add_i_d(inst, idiv, dest);
    return { rr(edx), inst };
}

Instruction_Pair sub(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(Mnemonic::sub, dest, src);
}

Instruction_Pair add(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(Mnemonic::add, dest, src);
}

Instruction_Pair inc(Storage const& dest)
{
    return add_1ary_inst(mn(inc), dest);
}

Instruction_Pair dec(Storage const& dest)
{
    return add_1ary_inst(mn(dec), dest);
}

Instruction_Pair neg(Storage const& dest)
{
    return add_1ary_inst(mn(neg), dest);
}

Instruction_Pair r_eq(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, sete, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_neq(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, setne, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_lt(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, setl, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_gt(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, setg, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_le(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, setle, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_ge(Storage const& dest, Storage const& src)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, src);
    add_i_ld(inst, setge, al);
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair rshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(shr), dest, src);
}

Instruction_Pair lshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(shl), dest, src);
}

Instruction_Pair b_and(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(and_), dest, src);
}

Instruction_Pair b_or(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(or_), dest, src);
}

Instruction_Pair b_xor(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(xor_), dest, src);
}

Instruction_Pair b_not(Storage const& dest)
{
    return add_1ary_inst(mn(not_), dest);
}

Instruction_Pair u_not(Storage const& dest)
{
    auto inst = make();
    add_i_ll(inst, mov, eax, dest);
    add_i_ll(inst, cmp, eax, make_int_immediate(0));
    add_i_ld(inst, setne, al);
    add_i_s(inst, xor_, rr(al), make_int_immediate(-1));
    add_i_s(inst, and_, rr(al), make_int_immediate(1));
    add_i_llrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair lea(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(lea), dest, src);
}

} // namespace x86_64::detail