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

Register get_accumulator_register_from_size(Operand_Size size)
{
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
        m::pattern | m::_ = [&] { return Register::eax; });
}

Instruction_Pair arithmetic_accumulator_two_form_instruction(
    Mnemonic mnemonic,
    Operand_Size size,
    Storage& dest,
    Storage& src)
{
    using namespace credence::target::x86_64::detail;
    auto instructions = make_inst();
    add_inst(instructions, mnemonic, size, dest, src);
    CREDENCE_ASSERT(!std::holds_alternative<std::monostate>(dest));
    return { dest, instructions };
}

Instruction_Pair mul(Operand_Size size, Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::imul, size, dest, src);
}

Instruction_Pair div(Operand_Size size, Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::idiv, size, dest, src);
}

Instruction_Pair sub(Operand_Size size, Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::sub, size, dest, src);
}

Instruction_Pair add(Operand_Size size, Storage& dest, Storage& src)
{
    return arithmetic_accumulator_two_form_instruction(
        Mnemonic::add, size, dest, src);
}

Instruction_Pair mod(Operand_Size size, Storage& dest, Storage& src)
{
    auto storage = Register::eax;
    auto inst = make_inst();
    if (size == Operand_Size::Qword) {
        storage = Register::rax;
        add_inst_s(inst, mov, size, storage, dest);
        add_inst_e(inst, cqo, size);
        add_inst_ll(inst, mov, size, rbx, src);
        add_inst_ld(inst, idiv, size, rbx);
        add_inst_lr(inst, mov, size, storage, rdx);
    } else {
        add_inst_s(inst, mov, size, storage, dest);
        add_inst_e(inst, cdq, size);
        add_inst_ll(inst, mov, size, ecx, src);
        add_inst_ld(inst, idiv, size, ecx);
        add_inst_lr(inst, mov, size, storage, edx);
    }
    return { storage, inst };
}

Instruction_Pair inc(Operand_Size size, Storage& dest)
{
    auto inst = make_inst();
    add_inst_s(inst, inc, size, dest, O_NUL);
    return { dest, inst };
}

Instruction_Pair dec(Operand_Size size, Storage& dest)
{
    auto inst = make_inst();
    add_inst_s(inst, dec, size, dest, O_NUL);
    return { dest, inst };
}

Instruction_Pair r_eq(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, sete, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_neq(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, setne, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair u_not(Operand_Size size, Storage& dest)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, make_integer_immediate(0));
    add_inst_ld(inst, setne, size, al);
    add_inst_s(inst, xor_, size, Register::al, make_integer_immediate(-1));
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_lt(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, setl, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_gt(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, setg, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_le(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, setle, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

Instruction_Pair r_ge(Operand_Size size, Storage& dest, Storage& src)
{
    auto inst = make_inst();
    add_inst_ll(inst, mov, size, eax, dest);
    add_inst_ll(inst, cmp, size, eax, src);
    add_inst_ld(inst, setge, size, al);
    add_inst_s(inst, and_, size, Register::al, make_integer_immediate(1));
    add_inst_lrs(inst, mov, size, eax, al);
    return { Register::eax, inst };
}

} // namespace x86_64