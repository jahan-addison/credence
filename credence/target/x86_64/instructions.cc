#include "instructions.h"

#include <credence/assert.h>   // for CREDENCE_ASSER
#include <credence/ir/table.h> // for Table
#include <matchit.h>           // for Wildcard, pattern, Ds, App, _, Or, as
#include <variant>             // for variant, get

namespace credence::target::x86_64 {

using namespace credence::target::x86_64::detail;

namespace m = matchit;

Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue)
{
    using T = ir::Table::Type;
    ir::Table::Type type = ir::Table::get_type_from_symbol(rvalue);
    return m::match(type)(
        m::pattern | m::or_(T{ "int" }, T{ "string" }) =
            [&] { return Operand_Size::Dword; },
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

Instruction_Pair arithmetic_accumulator_two_form_instruction(
    Mnemonic mnemonic,
    Storage& dest,
    Storage& src)
{
    CREDENCE_ASSERT(!is_variant(std::monostate, dest));
    auto instructions = make_inst();
    addii(instructions, mnemonic, dest, src);
    return { dest, instructions };
}

Instruction_Pair arithmetic_one_form_instruction(
    Mnemonic mnemonic,
    Storage& src)
{
    auto instructions = make_inst();
    addii(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

Instruction_Pair mul(Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::imul, dest, src);
}

Instruction_Pair div(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiie(inst, cdq);
    addiis(inst, mov, dest, src);
    addiid(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiie(inst, cdq);
    addiis(inst, mov, dest, src);
    addiid(inst, idiv, dest);
    return { Register::edx, inst };
}

Instruction_Pair sub(Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::sub, dest, src);
}

Instruction_Pair add(Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::add, dest, src);
}

Instruction_Pair inc(Storage& dest)
{
    auto inst = make_inst();
    addiis(inst, inc, dest, O_NUL);
    return { dest, inst };
}

Instruction_Pair dec(Storage& dest)
{
    auto inst = make_inst();
    addiis(inst, dec, dest, O_NUL);
    return { dest, inst };
}

Instruction_Pair r_eq(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, sete, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_neq(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setne, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair u_not(Storage& dest)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, make_integer_immediate(0));
    adiild(inst, setne, al);
    addiis(inst, xor_, Register::al, make_integer_immediate(-1));
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_lt(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setl, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_gt(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setg, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_le(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setle, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_ge(Storage& dest, Storage& src)
{
    auto inst = make_inst();
    addiill(inst, mov, eax, dest);
    addiill(inst, cmp, eax, src);
    adiild(inst, setge, al);
    addiis(inst, and_, Register::al, make_integer_immediate(1));
    addiilrs(inst, mov, eax, al);
    return { Register::eax, inst };
}

} // namespace x86_64