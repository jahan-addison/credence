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
#include <credence/rvalue.h>   // for RValue, LValue, ...
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
    auto type = symbol::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = symbol::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = symbol::get_value_from_rvalue_data_type(rhs);
    auto lhs_type = symbol::get_type_from_rvalue_data_type(lhs);
    auto rhs_type = symbol::get_type_from_rvalue_data_type(lhs);

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

Immediate get_result_from_trivial_integral_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = symbol::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = symbol::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = symbol::get_value_from_rvalue_data_type(rhs);
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
    auto type = symbol::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = symbol::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = symbol::get_value_from_rvalue_data_type(rhs);
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

Instruction_Pair add_2ary_inst(Mnemonic mnemonic, Storage& dest, Storage& src)
{
    auto instructions = make();
    addii(instructions, mnemonic, dest, src);
    return { dest, instructions };
}

Instruction_Pair add_1ary_inst(Mnemonic mnemonic, Storage& src)
{
    auto instructions = make();
    addii(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

Instruction_Pair mul(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(imul), dest, src);
}

Instruction_Pair div(Storage& dest, Storage& src)
{
    auto inst = make();
    addiie(inst, cdq);
    addiis(inst, mov, dest, src);
    addiid(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage& dest, Storage& src)
{
    auto inst = make();
    addiie(inst, cdq);
    addiis(inst, mov, dest, src);
    addiid(inst, idiv, dest);
    return { rr(edx), inst };
}

Instruction_Pair sub(Storage& dest, Storage& src)
{
    return add_2ary_inst(Mnemonic::sub, dest, src);
}

Instruction_Pair add(Storage& dest, Storage& src)
{
    return add_2ary_inst(Mnemonic::add, dest, src);
}

Instruction_Pair inc(Storage& dest)
{
    return add_1ary_inst(mn(inc), dest);
}

Instruction_Pair dec(Storage& dest)
{
    return add_1ary_inst(mn(dec), dest);
}

Instruction_Pair neg(Storage& dest)
{
    return add_1ary_inst(mn(neg), dest);
}

Instruction_Pair r_eq(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, sete, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_neq(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setne, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_lt(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setl, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_gt(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setg, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_le(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setle, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair r_ge(Storage& dest, Storage& src)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setge, al);
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair rshift(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(shr), dest, src);
}

Instruction_Pair lshift(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(shl), dest, src);
}

Instruction_Pair b_and(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(and_), dest, src);
}

Instruction_Pair b_or(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(or_), dest, src);
}

Instruction_Pair b_xor(Storage& dest, Storage& src)
{
    return add_2ary_inst(mn(or_), dest, src);
}

Instruction_Pair b_not(Storage& dest)
{
    return add_1ary_inst(mn(not_), dest);
}

Instruction_Pair u_not(Storage& dest)
{
    auto inst = make();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, make_int_immediate(0));
    adiild(inst, setne, al);
    addiis(inst, xor_, rr(al), make_int_immediate(-1));
    addiis(inst, and_, rr(al), make_int_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { rr(eax), inst };
}

} // namespace x86_64::detail