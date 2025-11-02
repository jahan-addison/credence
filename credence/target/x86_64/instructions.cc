#include "instructions.h"

#include <credence/assert.h>   // for CREDENCE_ASSER
#include <credence/ir/table.h> // for Table
#include <matchit.h>           // for Wildcard, pattern, Ds, App, _, Or, as
#include <variant>             // for variant, get

#define add_inst_s(inst, op, size, lhs, rhs)           \
  do {                                                 \
    inst.emplace_back(Mnemonic::op, size, lhs, rhs);   \
  } while(0)

#define add_inst_ll(inst, op, size, lhs, rhs)                    \
  do {                                                           \
    inst.emplace_back(Mnemonic::op, size, Register::lhs, rhs);   \
  } while(0)

#define add_inst_lr(inst, op, size, lhs, rhs)                    \
  do {                                                           \
    inst.emplace_back(Mnemonic::op, size, lhs, Register::rhs);   \
  } while(0)

#define add_inst_lrs(inst, op, size, lhs, rhs)                    \
  do {                                                           \
    inst.emplace_back(Mnemonic::op, size, Register::lhs, Register::rhs);   \
  } while(0)

#define add_inst(inst, op, size, lhs, rhs)           \
  do {                                               \
    inst.emplace_back(op, size, lhs, rhs);           \
  } while(0)

namespace credence::target::x86_64 {

namespace m = matchit;

Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue)
{
    using T = ir::Table::Type;
    ir::Table::Type type = ir::Table::get_type_from_symbol(rvalue);
    // clang-format off
    return m::match(type)(
        m::pattern | m::or_(T{ "int" }, T{ "string" }) =
            [&] {
                return Operand_Size::Dword;
            },
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] {
                return Operand_Size::Qword;
            },
        m::pattern | T{ "float" } =
            [&] {
            return Operand_Size::Dword;
        },
        m::pattern | T{ "char" } =
            [&] {
                return Operand_Size::Byte;
        },
        m::pattern | m::_ =
            [&] {
                return Operand_Size::Dword;
        }
    );
    // clang-format on
}

Register acc_register_from_size(Operand_Size size)
{
    // clang-format off
    return m::match(size) (
        m::pattern | Operand_Size::Qword = [&] {
            return Register::rax;
        },
        m::pattern | m::_ = [&] {
            return Register::eax;
        }
    );
    // clang-format on
}

Operation_Pair instructions_from_mov_and_mnemonic(
    Mnemonic mnemonic,
    Operand_Size size,
    Storage& src,
    Storage& dest)
{
    auto instructions = make_inst();
    m::match(src, dest)(
        m::pattern | m::ds(m::as<Immediate>(m::_), m::as<Immediate>(m::_)) =
            [&] {
                auto acc = acc_register_from_size(size);
                add_inst_s(instructions, mov, size, src, acc);
                add_inst(instructions, mnemonic, size, acc, dest);
            },
        m::pattern | m::ds(m::as<Immediate>(m::_), m::as<Register>(m::_)) =
            [&] { add_inst(instructions, mnemonic, size, src, dest); },
        m::pattern | m::ds(m::as<Register>(m::_), m::as<Register>(m::_)) =
            [&] { add_inst(instructions, mnemonic, size, src, dest); });
    CREDENCE_ASSERT(!std::holds_alternative<std::monostate>(dest));
    return { dest, instructions };
}

Operation_Pair imul(Operand_Size size, Storage& lhs, Storage& rhs)
{
    return instructions_from_mov_and_mnemonic(Mnemonic::imul, size, lhs, rhs);
}

Operation_Pair idiv(Operand_Size size, Storage& lhs, Storage& rhs)
{
    return instructions_from_mov_and_mnemonic(Mnemonic::idiv, size, lhs, rhs);
}

Operation_Pair sub(Operand_Size size, Storage& lhs, Storage& rhs)
{
    return instructions_from_mov_and_mnemonic(Mnemonic::sub, size, lhs, rhs);
}

Operation_Pair add(Operand_Size size, Storage& lhs, Storage& rhs)
{
    return instructions_from_mov_and_mnemonic(Mnemonic::add, size, lhs, rhs);
}

Operation_Pair mod(Operand_Size size, Storage& lhs, Storage& rhs)
{
    Storage storage = O_NUL;
    auto inst = make_inst();
    if (size == Operand_Size::Qword) {
        // cppcheck-suppress redundantInitialization
        storage = Register::rax;
        add_inst_s(inst, mov, size, lhs, storage);
        add_inst_s(inst, cqo, size, O_NUL, O_NUL);
        add_inst_lr(inst, mov, size, rhs, rbx);
        add_inst_ll(inst, idiv, size, rbx, O_NUL);
        add_inst_ll(inst, mov, size, rdx, storage);
    } else {
        // cppcheck-suppress redundantInitialization
        storage = Register::eax;
        add_inst_s(inst, mov, size, lhs, storage);
        add_inst_s(inst, cdq, size, O_NUL, O_NUL);
        add_inst_lr(inst, mov, size, rhs, ecx);
        add_inst_ll(inst, idiv, size, ecx, O_NUL);
        add_inst_ll(inst, mov, size, edx, storage);
    }
    return { storage, inst };
}

Operation_Pair inc(Operand_Size size, Storage& dest)
{
    auto inst = make_inst();
    add_inst_s(inst, inc, size, dest, O_NUL);
    return { dest, inst };
}

Operation_Pair dec(Operand_Size size, Storage& dest)
{
    auto inst = make_inst();
    add_inst_s(inst, dec, size, dest, O_NUL);
    return { dest, inst };
}

} // namespace x86_64