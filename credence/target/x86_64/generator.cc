#include "generator.h"
#include "instructions.h"           // for get_size_from_table_rvalue, Imm...
#include <credence/assert.h>        // for CREDENCE_ASSERT
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for align_up_to_16
#include <credence/typeinfo.h>      // for LValue, RValue, Data_Type, ...
#include <credence/util.h>          // for substring_count_of, contains
#include <cstddef>                  // for size_t
#include <deque>                    // for deque
#include <format>                   // for format
#include <matchit.h>                // for pattern, PatternHelper, Pattern...
#include <memory>                   // for shared_ptr
#include <ostream>                  // for basic_ostream, operator<<, endl
#include <sstream>                  // for basic_stringstream
#include <string_view>              // for basic_string_view
#include <tuple>                    // for get, tuple
#include <type_traits>              // for underlying_type_t
#include <utility>                  // for pair, make_pair
#include <variant>                  // for get, holds_alternative, monostate

#define ir_i(name) credence::ir::ITA::Instruction::name

namespace credence::target::x86_64 {

namespace m = matchit;

void Code_Generator::emit(std::ostream& os)
{
    build();
    os << std::endl;
    os << ".intel_syntax noprefix" << std::endl;
    os << std::endl;
    // clang-format off
    for (auto const& inst : instructions_) {
        std::visit(
            util::overload{
            [&](detail::Instruction const& s) {
                Mnemonic inst = std::get<0>(s);
                os << "    " << inst;
                Storage dest = std::get<1>(s);
                Storage src = std::get<2>(s);
                if (!is_variant(std::monostate, dest))
                    os << " " << emit_storage_device(dest);
                if (!is_variant(std::monostate, src))
                    os << ", " << emit_storage_device(src);
                os << std::endl;
            },
            [&](typeinfo::semantic::Label const& s) {
                os << s << ":" << std::endl;
            } },
                inst);
    }
    os << std::endl;
}

void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    // clang-format on
    using namespace credence::ir;
    auto ita = ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table = std::make_unique<ir::Table>(Table{ symbols, instructions });
    table->build_vector_definitions_from_globals(ita.globals_);
    table->build_from_ita_instructions();
    auto generator = Code_Generator{ std::move(table) };
    generator.emit(os);
}

std::string Code_Generator::emit_storage_device(Storage const& storage)
{
    std::string sloc;
    std::visit(
        util::overload{
            [&]([[maybe_unused]] std::monostate s) {},
            [&](detail::Stack::Offset s) {
                auto size = stack.get_operand_size_from_offset(s);
                CREDENCE_ASSERT(size != Operand_Size::Empty);
                std::string prefix = m::match(size)(
                    m::pattern |
                        Operand_Size::Qword = [&] { return "qword ptr"; },
                    m::pattern |
                        Operand_Size::Dword = [&] { return "dword ptr"; },
                    m::pattern |
                        Operand_Size::Word = [&] { return "word ptr"; },
                    m::pattern |
                        Operand_Size::Byte = [&] { return "byte ptr"; },
                    m::pattern | m::_ = [&] { return "dword ptr"; });

                sloc = std::format("{} [rbp - {}]", prefix, s);
            },
            [&](Register s) {
                std::stringstream ss{};
                ss << s;
                sloc = ss.str();
            },
            [&](Immediate const& s) { sloc = std::get<0>(s); } },
        storage);
    return sloc;
}

void Code_Generator::build()
{
    auto instructions = table_->instructions;
    ita_index = 0UL;
    for (; ita_index < instructions.size(); ita_index++) {
        auto inst = instructions[ita_index];
        ir::ITA::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir_i(FUNC_START) =
                [&] {
                    auto symbol = std::get<1>(instructions.at(ita_index - 1));
                    auto name = typeinfo::get_label_as_human_readable(symbol);
                    current_frame = name;
                    set_table_stack_frame(name);
                    from_func_start_ita(name);
                },
            m::pattern | ir_i(FUNC_END) = [&] { from_func_end_ita(); },
            m::pattern | ir_i(MOV) = [&] { from_mov_ita(inst); },
            m::pattern | ir_i(LOCL) = [&] { from_locl_ita(inst); },
            m::pattern | ir_i(RETURN) = [&] { from_return_ita(Register::rax); },
            m::pattern | ir_i(LEAVE) = [&] { from_leave_ita(); },
            m::pattern | ir_i(LABEL) = [&] { from_label_ita(inst); },
            m::pattern | ir_i(PUSH) = [&] { from_push_ita(inst); }

        );
    }
}

void Code_Generator::from_func_start_ita(typeinfo::semantic::Label const& name)
{
    CREDENCE_ASSERT(table_->functions.contains(name));
    stack.clear();
    current_frame = name;
    set_table_stack_frame(name);
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    adiild(instructions_, push, rbp);
    addiilrs(instructions_, mov_, rbp, rsp);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        auto imm = detail::make_u32_int_immediate(stack_alloc);
        addiill(instructions_, sub, rsp, imm);
    }
}

Code_Generator::Storage Code_Generator::get_storage_device(Operand_Size size)
{
    auto& registers = size == Operand_Size::Qword ? available_qword_register
                                                  : available_dword_register;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return stack.allocate(size);
    }
}

void Code_Generator::set_table_stack_frame(
    typeinfo::semantic::Label const& name)
{
    table_->set_stack_frame(name);
}

void Code_Generator::from_func_end_ita()
{
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        auto a_imm = detail::make_u32_int_immediate(stack_alloc);
        addiill(instructions_, add, rsp, a_imm);
    }
    reset_o_register();
}

void Code_Generator::from_push_ita(ITA_Inst const& inst)
{
    auto lvalue = std::get<1>(inst);
    auto frame = table_->functions[current_frame];
    auto& locals = table_->get_stack_frame_symbols();
    auto symbol = locals.get_symbol_by_name(lvalue);
    auto storage = get_storage_device();

    addiis(instructions_, mov, storage, symbol);
}

void Code_Generator::from_locl_ita(ITA_Inst const& inst)
{
    typeinfo::semantic::LValue lvalue = std::get<1>(inst);
    stack.make(lvalue);
}

void Code_Generator::from_cmp_ita([[maybe_unused]] ITA_Inst const& inst)
{
    table_->set_stack_frame(current_frame);
}

void Code_Generator::from_mov_ita(ITA_Inst const& inst)
{
    typeinfo::semantic::LValue lhs = std::get<1>(inst);
    auto symbols = table_->get_stack_frame_symbols();
    if (!typeinfo::is_temporary(lhs)) {
        CREDENCE_ASSERT(stack.contains(lhs));
        typeinfo::semantic::RValue rhs =
            ir::get_rvalue_from_mov_qaudruple(inst).first;
        if (typeinfo::is_rvalue_data_type(rhs)) {
            auto imm = typeinfo::get_symbol_type_size_from_rvalue_string(rhs);
            stack.set_address_from_immediate(lhs, imm);
            auto lhs_storage = stack.get(lhs).first;
            addiis(instructions_, mov, lhs_storage, imm);
        } else if (typeinfo::is_temporary(rhs)) {
            auto acc = get_accumulator_register_from_size();
            stack.set_address_from_accumulator(lhs, acc);
            auto lhs_storage = stack.get(lhs).first;
            addiis(instructions_, mov, lhs_storage, acc);
        } else if (stack.contains(rhs)) {
            auto acc =
                get_accumulator_register_from_size(stack.get(rhs).second);
            Storage lhs_storage = stack.get(lhs).first;
            Storage rhs_storage = stack.get(rhs).first;
            addiis(instructions_, mov, acc, rhs_storage);
            addiis(instructions_, mov, lhs_storage, acc);
        } else {
            Storage lhs_storage = stack.get(lhs).first;
            if (detail::is_unary_operator(rhs)) {
                auto symbol = symbols.get_symbol_by_name(lhs);
                auto unary_op = typeinfo::get_unary(rhs);
                from_ita_unary_expression(unary_op, lhs_storage);
            } else {
                std::cout << "hmm: " << rhs << std::endl;
                auto symbol = symbols.get_symbol_by_name(rhs);
                addiis(instructions_, mov, lhs_storage, symbol);
            }
        }
        temporary_expansion = false;
    } else {
        insert_from_temporary_table_rvalue(table_->from_temporary_lvalue(lhs));
    }
}

Code_Generator::Storage Code_Generator::get_storage_from_temporary_lvalue(
    typeinfo::semantic::LValue const& lvalue,
    std::string const& op)
{
    Storage storage{};
    if (typeinfo::is_rvalue_data_type(lvalue)) {
        auto acc = get_accumulator_register_from_size();
        storage = typeinfo::get_symbol_type_size_from_rvalue_string(lvalue);
        if (!temporary_expansion) {
            temporary_expansion = true;
            auto lookbehind = table_->instructions[ita_index];
            typeinfo::semantic::RValue rvalue =
                ir::get_rvalue_from_mov_qaudruple(lookbehind).first;
            if (!typeinfo::is_binary_rvalue_data_expression(rvalue))
                addiis(instructions_, mov, acc, storage);
        }
    } else if (stack.contains(lvalue) and not stack.empty_at(lvalue)) {
        storage = get_accumulator_register_from_size(stack.get(lvalue).second);
        auto address = stack.get(lvalue).first;
        Storage_Operands operands = { storage, address };
        if (!temporary_expansion) {
            temporary_expansion = true;
            addiis(instructions_, mov, storage, address);
            return storage;
        } else {
            auto inst = from_arithmetic_expression_operands(operands, op);
            detail::insert(instructions_, inst.second);
            return storage;
        }
    } else
        storage = get_accumulator_register_from_size();

    return storage;
}

constexpr detail::Register Code_Generator::get_accumulator_register_from_size(
    Operand_Size size)
{
    namespace m = matchit;
    using namespace detail;
    if (special_register != Register::eax) {
        auto special = special_register;
        special_register = Register::eax;
        return special;
    }
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
        m::pattern | Operand_Size::Word = [&] { return Register::ax; },
        m::pattern | Operand_Size::Byte = [&] { return Register::al; },
        m::pattern | m::_ = [&] { return Register::eax; });
}

void Code_Generator::insert_from_temporary_immediate_rvalues(
    Storage& lhs,
    std::string const& op,
    Storage& rhs)
{
    auto imm_l = std::get<Immediate>(lhs);
    auto imm_r = std::get<Immediate>(rhs);
    if (std::ranges::find(detail::math_binary_operators, op) !=
        detail::math_binary_operators.end()) {
        auto imm = detail::get_result_from_trivial_integral_expression(
            imm_l, op, imm_r);
        auto acc = get_accumulator_register_from_size();
        addiis(instructions_, mov, acc, imm);
    } else if (
        std::ranges::find(detail::relation_binary_operators, op) !=
        detail::math_binary_operators.end()) {
        auto imm = detail::get_result_from_trivial_relational_expression(
            imm_l, op, imm_r);
        auto acc = get_accumulator_register_from_size(Operand_Size::Byte);
        special_register = acc;
        addiis(instructions_, mov, acc, imm);
    } else if (
        std::ranges::find(detail::bitwise_operators, op) !=
        detail::bitwise_operators.end()) {
        auto imm = detail::get_result_from_trivial_bitwise_expression(
            imm_l, op, imm_r);
        auto acc = get_accumulator_register_from_size();
        addiis(instructions_, mov, acc, imm);
    }
}

void Code_Generator::from_ita_unary_expression(
    std::string const& op,
    Storage& dest)
{
    m::match(op)(
        m::pattern | std::string{ "++" } =
            [&] {
                auto inst = inc(dest);
                detail::insert(instructions_, inst.second);
            },
        m::pattern | std::string{ "--" } =
            [&] {
                auto inst = dec(dest);
                detail::insert(instructions_, inst.second);
            },
        m::pattern | std::string{ "~" } =
            [&] {
                auto inst = b_not(dest);
                detail::insert(instructions_, inst.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                auto inst = neg(dest);
                detail::insert(instructions_, inst.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                // unary "+" is a no-op
            });
}

void Code_Generator::from_temporary_unary_operator_expression(
    typeinfo::semantic::RValue const& expr)
{
    CREDENCE_ASSERT(detail::is_unary_operator(expr));
    auto op = typeinfo::get_unary(expr);
    typeinfo::semantic::RValue rvalue =
        typeinfo::get_unary_rvalue_reference(expr);
    get_storage_from_temporary_lvalue(rvalue, op);
    if (stack.contains(rvalue)) {
        auto size = stack.get(rvalue).second;
        Storage dest = get_accumulator_register_from_size(size);
        from_ita_unary_expression(op, dest);
    } else {
        auto size = detail::get_size_from_table_rvalue(
            typeinfo::get_symbol_type_size_from_rvalue_string(rvalue));
        Storage dest = get_accumulator_register_from_size(size);
        from_ita_unary_expression(op, dest);
    }
}

void Code_Generator::from_temporary_binary_operator_expression(
    typeinfo::semantic::RValue const& expr)
{
    CREDENCE_ASSERT(detail::is_binary_operator(expr));
    auto expression = typeinfo::from_rvalue_binary_expression(expr);
    auto lhs = std::get<0>(expression);
    auto rhs = std::get<1>(expression);
    auto op = std::get<2>(expression);
    auto lhs_s = get_storage_from_temporary_lvalue(lhs, op);
    auto rhs_s = get_storage_from_temporary_lvalue(rhs, op);
    if (is_variant(Immediate, lhs_s) and is_variant(Immediate, rhs_s)) {
        insert_from_temporary_immediate_rvalues(lhs_s, op, rhs_s);
        return;
    }
    if (lhs_s == rhs_s)
        return;
    if (is_variant(Immediate, lhs_s))
        std::swap(lhs_s, rhs_s);
    Storage_Operands operands = { lhs_s, rhs_s };
    if (detail::is_binary_math_operator(expr)) {
        auto inst = from_arithmetic_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    } else if (detail::is_relation_binary_operator(expr)) {
        auto inst = from_relational_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    } else if (detail::is_bitwise_operator(expr)) {
        auto inst = from_bitwise_expression_operands(operands, op);
        detail::insert(instructions_, inst.second);
    }
}

void Code_Generator::insert_from_temporary_table_rvalue(
    typeinfo::semantic::RValue const& expr)
{
    auto symbols = table_->get_stack_frame_symbols();
    if (detail::is_binary_operator(expr)) {
        from_temporary_binary_operator_expression(expr);
    } else if (detail::is_unary_operator(expr)) {
        from_temporary_unary_operator_expression(expr);
    } else if (typeinfo::is_rvalue_data_type(expr)) {
        auto imm = typeinfo::get_symbol_type_size_from_rvalue_string(expr);
        addiill(instructions_, mov, eax, imm);
    } else {
        auto imm = symbols.get_symbol_by_name(expr);
        addiill(instructions_, mov, eax, imm);
    }
}

void Code_Generator::from_return_ita(Storage const& dest)
{
    addiilr(instructions_, mov, dest, eax);
}

void Code_Generator::from_leave_ita()
{
    if (current_frame == "main")
        addiilrs(instructions_, xor_, eax, eax);
    adiild(instructions_, pop, rbp);
    addiie(instructions_, ret);
}

void Code_Generator::from_label_ita(ITA_Inst const& inst)
{
    instructions_.emplace_back(
        typeinfo::get_label_as_human_readable(std::get<1>(inst)));
}

Code_Generator::Instruction_Pair
Code_Generator::from_arithmetic_expression_operands(
    Storage_Operands& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "*" } =
            [&] { instructions = mul(operands.first, operands.second); },
        m::pattern | std::string{ "/" } =
            [&] {
                auto storage = get_storage_device(Operand_Size::Dword);
                addiis(instructions.second, mov, storage, operands.first);
                instructions = div(storage, operands.second);
            },
        m::pattern | std::string{ "-" } =
            [&] { instructions = sub(operands.first, operands.second); },
        m::pattern | std::string{ "+" } =
            [&] { instructions = add(operands.first, operands.second); },
        m::pattern | std::string{ "%" } =
            [&] {
                auto storage = get_storage_device(Operand_Size::Dword);
                special_register = Register::edx;
                addiis(instructions.second, mov, storage, operands.first);
                instructions = mod(storage, operands.second);
            });
    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_bitwise_expression_operands(
    Storage_Operands& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "<<" } =
            [&] { instructions = lshift(operands.first, operands.second); },
        m::pattern | std::string{ ">>" } =
            [&] { instructions = rshift(operands.first, operands.second); },
        m::pattern | std::string{ "^" } =
            [&] { instructions = b_xor(operands.first, operands.second); },
        m::pattern | std::string{ "&" } =
            [&] { instructions = b_and(operands.first, operands.second); },
        m::pattern | std::string{ "|" } =
            [&] { instructions = b_or(operands.first, operands.second); });
    return instructions;
}

Code_Generator::Instruction_Pair
Code_Generator::from_relational_expression_operands(
    Storage_Operands& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        // cppcheck-suppress syntaxError
        m::pattern | std::string{ "==" } =
            [&] { instructions = r_eq(operands.first, operands.second); },
        m::pattern | std::string{ "!=" } =
            [&] { instructions = r_neq(operands.first, operands.second); },
        m::pattern | std::string{ "<" } =
            [&] { instructions = r_gt(operands.first, operands.second); },
        m::pattern | std::string{ ">" } =
            [&] { instructions = r_lt(operands.first, operands.second); },
        m::pattern | std::string{ "<=" } =
            [&] { instructions = r_le(operands.first, operands.second); },
        m::pattern | std::string{ ">=" } =
            [&] { instructions = r_ge(operands.first, operands.second); });

    return instructions;
}

constexpr detail::Stack::Entry detail::Stack::get(LValue const& lvalue)
{
    return stack_address[lvalue];
}

constexpr void detail::Stack::make(LValue const& lvalue)
{
    stack_address[lvalue] = { 0, Operand_Size::Empty };
}

constexpr detail::Stack::Offset detail::Stack::allocate(Operand_Size operand)
{
    auto alloc = get_size_from_operand_size(operand);
    size += alloc;
    return size;
}

constexpr detail::Stack::Entry detail::Stack::get(Offset offset)
{
    auto find = std::find_if(
        stack_address.begin(), stack_address.end(), [&](Pair const& entry) {
            return entry.second.first == offset;
        });
    if (find == stack_address.end())
        return { 0, Operand_Size::Empty };
    else
        return find->second;
}

constexpr detail::Operand_Size detail::Stack::get_operand_size_from_offset(
    Offset offset)
{
    return std::accumulate(
        stack_address.begin(),
        stack_address.end(),
        Operand_Size::Empty,
        [&](Operand_Size size, Pair const& entry) {
            if (entry.second.first == offset)
                return entry.second.second;
            return size;
        });
}

constexpr void detail::Stack::set_address_from_immediate(
    LValue const& lvalue,
    Immediate const& rvalue)
{
    auto operand_size = detail::get_size_from_table_rvalue(rvalue);
    auto value_size = detail::get_size_from_operand_size(operand_size);
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    size += value_size;
    stack_address[lvalue].first = size;
    stack_address[lvalue].second = operand_size;
}

constexpr void detail::Stack::set_address_from_accumulator(
    LValue const& lvalue,
    Register acc)
{
    auto register_size = detail::get_size_from_accumulator_register(acc);
    if (stack_address[lvalue].second != Operand_Size::Empty)
        return;
    size += detail::get_size_from_operand_size(register_size);
    stack_address[lvalue].first = size;
    stack_address[lvalue].second = register_size;
}

} // namespace x86_64