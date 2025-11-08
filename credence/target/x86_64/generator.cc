#include "generator.h"
#include "instructions.h"           // for get_size_from_table_rvalue, Imm...
#include <credence/assert.h>        // for CREDENCE_ASSERT
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for align_up_to_16
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
    build_from_ita_table();
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
                if (!std::holds_alternative<std::monostate>(dest))
                    os << " " << emit_storage_device(dest);
                if (!std::holds_alternative<std::monostate>(src))
                    os << ", " << emit_storage_device(src);
                os << std::endl;
            },
            [&](ir::Table::Label const& s) {
                os << s << ":" << std::endl;
            } },
                inst);
    }
    // clang-format on
    os << std::endl;
}

void Code_Generator::build_from_ita_table()
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
                    auto name = ir::Table::get_label_as_human_readable(symbol);
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

std::string Code_Generator::emit_storage_device(Storage const& storage)
{
    std::string sloc;
    std::visit(
        util::overload{
            [&]([[maybe_unused]] std::monostate s) {},
            [&](detail::Stack_Offset s) {
                sloc = std::format("dword ptr [rbp - {}]", s);
            },
            [&](Register s) {
                std::stringstream ss{};
                ss << s;
                sloc = ss.str();
            },
            [&](detail::Immediate const& s) { sloc = std::get<0>(s); } },
        storage);
    return sloc;
}

void Code_Generator::from_func_start_ita(ir::Table::Label const& name)
{
    CREDENCE_ASSERT(table_->functions.contains(name));
    stack.clear();
    stack_offset = 0;
    current_frame = name;
    set_table_stack_frame(name);
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    adiild(instructions_, push, rbp);
    addiilrs(instructions_, mov_, rbp, rsp);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        addiill(
            instructions_,
            sub,
            rsp,
            detail::make_u32_integer_immediate(stack_alloc));
    }
}

Code_Generator::Storage Code_Generator::get_storage_device(Operand_Size size)
{
    auto& registers =
        size == Operand_Size::Dword ? o_dword_register : o_qword_register;
    if (registers.size() > 0) {
        Storage storage = registers.front();
        registers.pop_front();
        return storage;
    } else {
        return get_stack_address(size);
    }
}

Code_Generator::Storage Code_Generator::get_stack_address(Operand_Size size)
{
    return stack_offset +
           static_cast<std::underlying_type_t<Operand_Size>>(size);
}

void Code_Generator::set_table_stack_frame(ir::Table::Label const& name)
{
    table_->set_stack_frame(name);
}

void Code_Generator::from_func_end_ita()
{
    auto frame = table_->functions[current_frame];
    auto stack_alloc = credence::target::align_up_to_16(frame->allocation);
    if (table_->stack_frame_contains_ita_instruction(
            current_frame, ir::ITA::Instruction::CALL)) {
        addiill(
            instructions_,
            add,
            rsp,
            detail::make_u32_integer_immediate(stack_alloc));
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
    ir::Table::LValue lvalue = std::get<1>(inst);
    stack_offset +=
        static_cast<std::underlying_type_t<Operand_Size>>(Operand_Size::Dword);
    stack[lvalue] = stack_offset;
}

void Code_Generator::from_cmp_ita([[maybe_unused]] ITA_Inst const& inst)
{
    table_->set_stack_frame(current_frame);
}

void Code_Generator::from_mov_ita(ITA_Inst const& inst)
{
    ir::Table::LValue lhs = std::get<1>(inst);
    auto symbols = table_->get_stack_frame_symbols();
    if (!ir::Table::is_temporary(lhs)) {
        CREDENCE_ASSERT(stack.contains(lhs));
        auto lhs_storage = stack[lhs];
        ir::Table::RValue rhs =
            table_->get_rvalue_from_mov_instruction(inst).first;
        if (ir::Table::is_rvalue_data_type(rhs)) {
            auto imm = ir::Table::get_symbol_type_size_from_rvalue_string(rhs);
            addiis(instructions_, mov, lhs_storage, imm);
        } else if (ir::Table::is_temporary(rhs)) {
            auto acc = get_accumulator_register_from_size();
            addiis(instructions_, mov, lhs_storage, acc);
        } else {
            addiis(
                instructions_,
                mov,
                lhs_storage,
                symbols.get_symbol_by_name(rhs));
        }
        temporary_expansion = false;
    } else {
        insert_from_temporary_table_rvalue(table_->from_temporary_lvalue(lhs));
    }
}

Code_Generator::Storage Code_Generator::get_storage_from_temporary_lvalue(
    ir::Table::LValue const& lvalue,
    std::string const& op)
{
    Storage storage{};
    if (ir::Table::is_rvalue_data_type(lvalue)) {
        auto acc = get_accumulator_register_from_size();
        storage = ir::Table::get_symbol_type_size_from_rvalue_string(lvalue);
        if (!temporary_expansion) {
            temporary_expansion = true;
            auto lookbehind = table_->instructions[ita_index];
            ir::Table::RValue rvalue =
                table_->get_rvalue_from_mov_instruction(lookbehind).first;
            if (!ir::Table::is_binary_rvalue_data_expression(rvalue))
                addiis(instructions_, mov, acc, storage);
        }
    } else if (temporary.contains(lvalue)) {
        storage = temporary[lvalue];
    } else if (stack.contains(lvalue)) {
        storage = get_accumulator_register_from_size();
        Storage_Operands operands = { storage, stack[lvalue] };
        if (!temporary_expansion) {
            temporary_expansion = true;
            addiis(instructions_, mov, storage, stack[lvalue]);
            return storage;
        } else {
            auto inst = from_storage_arithmetic_expression(operands, op);
            detail::insert_inst(instructions_, inst.second);
            return storage;
        }
    } else
        storage = get_accumulator_register_from_size();

    return storage;
}

template<typename T>
T trivial_operation_from_numeric_table_type(
    std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T result{ 0 };
    T imm_l = type::integral_from_type<T>(lhs);
    T imm_r = type::integral_from_type<T>(rhs);
    switch (op[0]) {
        case '+':
            return imm_l + imm_r;
        case '-':
            return imm_l - imm_r;
        case '*':
            return imm_l * imm_r;
        case '/':
            return imm_l / imm_r;
    }
    return result;
}

detail::Immediate Code_Generator::get_result_from_trivial_integral_expression(
    detail::Immediate const& lhs,
    std::string const& op,
    detail::Immediate const& rhs)
{
    auto type = ir::Table::get_type_from_symbol(lhs);
    auto lhs_imm = ir::Table::get_value_from_rvalue_data_type(lhs);
    auto rhs_imm = ir::Table::get_value_from_rvalue_data_type(rhs);
    if (type == "int") {
        auto result = trivial_operation_from_numeric_table_type<int>(
            lhs_imm, op, rhs_imm);
        return detail::make_integer_immediate<int>(result, "int");
    } else if (type == "long") {
        auto result = trivial_operation_from_numeric_table_type<long>(
            lhs_imm, op, rhs_imm);
        return detail::make_integer_immediate<long>(result, "long");
    } else if (type == "float") {
        auto result = trivial_operation_from_numeric_table_type<float>(
            lhs_imm, op, rhs_imm);
        return detail::make_integer_immediate<float>(result, "float");

    } else if (type == "double") {
        auto result = trivial_operation_from_numeric_table_type<double>(
            lhs_imm, op, rhs_imm);
        return detail::make_integer_immediate<double>(result, "double");
    }
    return detail::make_integer_immediate(0, "int");
}

void Code_Generator::insert_from_temporary_immediate_rvalues(
    Storage& lhs,
    std::string const& op,
    Storage& rhs)
{
    auto imm_l = std::get<detail::Immediate>(lhs);
    auto imm_r = std::get<detail::Immediate>(rhs);
    auto imm = get_result_from_trivial_integral_expression(imm_l, op, imm_r);
    auto acc = get_accumulator_register_from_size();
    addiis(instructions_, mov, acc, imm);
}

void Code_Generator::insert_from_temporary_table_rvalue(
    ir::Table::RValue const& expr)
{
    Storage lhs_s{};
    Storage rhs_s{};
    auto symbols = table_->get_stack_frame_symbols();

    if (is_binary_math_operator(expr)) {
        auto expression = table_->from_rvalue_binary_expression(expr);
        auto lhs = std::get<0>(expression);
        auto rhs = std::get<1>(expression);
        auto op = std::get<2>(expression);
        lhs_s = get_storage_from_temporary_lvalue(lhs, op);
        rhs_s = get_storage_from_temporary_lvalue(rhs, op);
        if (is_variant(detail::Immediate, lhs_s) and
            is_variant(detail::Immediate, rhs_s)) {
            insert_from_temporary_immediate_rvalues(lhs_s, op, rhs_s);
            return;
        }
        if (lhs_s == rhs_s)
            return;
        if (is_variant(detail::Immediate, lhs_s))
            std::swap(lhs_s, rhs_s);
        Storage_Operands operands = { lhs_s, rhs_s };
        auto inst = from_storage_arithmetic_expression(operands, op);
        detail::insert_inst(instructions_, inst.second);
    } else if (is_relation_binary_operators(expr)) {
        auto expression = table_->from_rvalue_binary_expression(expr);
        auto lhs = std::get<0>(expression);
        auto rhs = std::get<1>(expression);
        auto op = std::get<2>(expression);
        lhs_s = get_storage_from_temporary_lvalue(lhs, op);
        rhs_s = get_storage_from_temporary_lvalue(rhs, op);
        if (is_variant(detail::Immediate, lhs_s) and
            is_variant(detail::Immediate, rhs_s)) {
            insert_from_temporary_immediate_rvalues(lhs_s, op, rhs_s);
            return;
        }
        if (lhs_s == rhs_s)
            return;
        if (is_variant(detail::Immediate, lhs_s))
            std::swap(lhs_s, rhs_s);
        Storage_Operands operands = { lhs_s, rhs_s };
        auto inst = from_storage_relational_expression(operands, op);
    } else if (ir::Table::is_rvalue_data_type(expr)) {
        auto imm = ir::Table::get_symbol_type_size_from_rvalue_string(expr);
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
    ir::Table::Label label =
        ir::Table::get_label_as_human_readable(std::get<1>(inst));
    instructions_.emplace_back(label);
}

Code_Generator::Instruction_Pair
Code_Generator::from_storage_arithmetic_expression(
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
Code_Generator::from_storage_relational_expression(
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

void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    using namespace credence::ir;
    auto ita = ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table = std::make_unique<ir::Table>(Table{ symbols, instructions });
    table->build_vector_definitions_from_globals(ita.globals_);
    table->build_from_ita_instructions();
    auto generator = Code_Generator{ std::move(table) };
    generator.emit(os);
}

} // namespace x86_64