#include "instructions.h"

#include <credence/assert.h>   // for CREDENCE_ASSER
#include <credence/ir/table.h> // for Table
#include <matchit.h>           // for Wildcard, pattern, Ds, App, _, Or, as
#include <variant>             // for variant, get

namespace credence::target::x86_64 {

namespace m = matchit;

namespace detail {

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

Operation_Pair imul(Operand_Size size, Storage const& lhs, Storage const& rhs)
{
    auto mul_inst = make_instructions();
    // clang-format off
    Register storage = m::match(lhs, rhs) (
        m::pattern | m::ds(m::as<Immediate>(m::_), m::as<Immediate>(m::_)) =
            [&] {
                auto acc = acc_register_from_size(size);
                mul_inst.emplace_back(Mnemonic::mov, size, lhs, acc);
                mul_inst.emplace_back(Mnemonic::imul, size, acc, rhs);
                return acc;
            },
        m::pattern | m::ds(m::as<Immediate>(m::_), m::as<Register>(m::_)) =
            [&] {
                mul_inst.emplace_back(Mnemonic::imul, size, lhs, rhs);
                return std::get<Register>(rhs);
            },
        m::pattern | m::ds(m::as<Register>(m::_), m::as<Register>(m::_)) =
            [&] {
                mul_inst.emplace_back(Mnemonic::imul, size, lhs, rhs);
                return std::get<Register>(rhs);
            },
        m::pattern | m::_ =
            [&] {
                return Register::noop;
            }
    );
    // clang-format on
    CREDENCE_ASSERT(storage != Register::noop);
    return { storage, mul_inst };
}

} // namespace detail

} // namespace x86_64