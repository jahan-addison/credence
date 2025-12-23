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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "assembly.h"
#include <credence/error.h> // for credence_error
#include <credence/types.h> // for get_value_from_rvalue_data_type, get_typ...
#include <matchit.h>        // for match, MatchHelper

namespace credence::target::x86_64::assembly {

namespace m = matchit;

/**
 * @brief Compute the result from trivial relational expression
 */
Immediate get_result_from_trivial_relational_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    int result{ 0 };
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    auto lhs_type = type::get_type_from_rvalue_data_type(lhs);
    auto rhs_type = type::get_type_from_rvalue_data_type(lhs);

    m::match(op)(make_trivial_immediate_binary_result(==),
        make_trivial_immediate_binary_result(!=),
        make_trivial_immediate_binary_result(<),
        make_trivial_immediate_binary_result(>),
        make_trivial_immediate_binary_result(&&),
        make_trivial_immediate_binary_result(||),
        make_trivial_immediate_binary_result(<=),
        make_trivial_immediate_binary_result(>=));

    return make_numeric_immediate(result, "byte");
}

/**
 * @brief Get a storage device as string for emission
 */
std::string get_storage_as_string(Storage const& storage)
{
    std::ostringstream result{};
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](Stack_Offset const& s) {
                       result << fmt::format("stack offset: {}", s);
                   },
                   [&](Register const& s) { result << s; },
                   [&](Immediate const& s) {
                       result << type::get_value_from_rvalue_data_type(s);
                   },
               },
        storage);
    return result.str();
}

/**
 * @brief Compute the result from trivial integral expression
 */
Immediate get_result_from_trivial_integral_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result = trivial_arithmetic_from_numeric_table_type<int>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result = trivial_arithmetic_from_numeric_table_type<long>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<long>(result, "long");
    } else if (type == "float") {
        auto result = trivial_arithmetic_from_numeric_table_type<float>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<float>(result, "float");

    } else if (type == "double") {
        auto result = trivial_arithmetic_from_numeric_table_type<double>(
            lhs_imm, op, rhs_imm);
        return make_numeric_immediate<double>(result, "double");
    }
    credence_error("unreachable");
    return make_numeric_immediate(0, "int");
}

/**
 * @brief Compute the result from trivial bitwise expression
 */
Immediate get_result_from_trivial_bitwise_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto type = type::get_type_from_rvalue_data_type(lhs);
    auto lhs_imm = type::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = type::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result =
            trivial_bitwise_from_numeric_table_type<int>(lhs_imm, op, rhs_imm);
        return make_numeric_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result =
            trivial_bitwise_from_numeric_table_type<long>(lhs_imm, op, rhs_imm);
        return make_numeric_immediate<long>(result, "long");
    }
    credence_error("unreachable");
    return make_numeric_immediate(0, "int");
}

/**
 * @brief Helper function for trivial 2-ary mnemonic instructions
 *
 *   Example:
 *
 *    add rax, rdi
 *    imul rax, [rbp - 4]
 */
Instruction_Pair add_2ary_inst(Mnemonic mnemonic,
    Storage const& dest,
    Storage const& src)
{
    auto instructions = make_empty();
    add_asm__(instructions, mnemonic, dest, src);
    return { dest, instructions };
}

/**
 * @brief Helper function for trivial 1-ary mnemonic instructions
 *
 *   Example:
 *
 *    idiv edi
 */
Instruction_Pair add_1ary_inst(Mnemonic mnemonic, Storage const& src)
{
    auto instructions = make_empty();
    add_asm__(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

/*********************************/
/* x86-64 directive constructors */
/*********************************/

Directive_Pair asciz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_str{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::asciz, rvalue });
    return { label, directives };
}

Directive_Pair floatz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_float{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    return { label, directives };
}

Directive_Pair doublez(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_double{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    return { label, directives };
}

Directives quad(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::quad, rvalue });
    return directives;
}

Directives long_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::long_, rvalue });
    return directives;
}

Directives float_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    return directives;
}

Directives double_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    return directives;
}

Directives byte_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::byte_, rvalue });
    return directives;
}

// ---

/***********************************/
/* x86-64 instruction constructors */
/***********************************/

// Instruction_Pair is the destination storage device, and std::deque of
// instructions
// ---

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(imul), dest, src);
}

Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    asm__zero_o(inst, cdq);
    add_asm__as(inst, mov, dest, src);
    asm__dest(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    asm__zero_o(inst, cdq);
    add_asm__as(inst, mov, dest, src);
    asm__dest(inst, idiv, dest);
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

Instructions r_eq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, je, make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_neq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, jne, make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_lt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, jl, make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_gt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, jg, make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_le(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, jle, make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_ge(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    add_asm__as(inst, mov, with, dest);
    add_asm__as(inst, cmp, with, src);
    add_asm__as(inst, jge, make_direct_immediate(to), O_NUL);
    return inst;
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
    auto inst = make_empty();
    asm__dest_rs(inst, mov, eax, dest);
    asm__dest_rs(inst, cmp, eax, make_numeric_immediate(0));
    asm__dest_s(inst, setne, al);
    add_asm__as(inst, xor_, rr(al), make_numeric_immediate(-1));
    add_asm__as(inst, and_, rr(al), make_numeric_immediate(1));
    asm__short(inst, movzx, eax, al);
    return { rr(eax), inst };
}

Instruction_Pair lea(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(mn(lea), dest, src);
}

} // namespace x86_64::detail