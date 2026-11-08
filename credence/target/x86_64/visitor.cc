/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include "visitor.h"

#include "assembly.h"                        // for Register, Mnemonic, x86...
#include "credence/error.h"                  // for credence_assert
#include "credence/map.h"                    // for Ordered_Map
#include "credence/symbol.h"                 // for Symbol_Table
#include "credence/target/common/assembly.h" // for u32_int_immediate, Arch...
#include "credence/target/common/flags.h"    // for Instruction_Flag
#include "credence/target/common/memory.h"   // for is_parameter
#include "credence/target/common/types.h"    // for Table_Pointer
#include "credence/types.h"                  // for from_lvalue_offset, get...
#include "inserter.h"                        // for Expression_Inserter
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "stack.h"                           // for Stack
#include "syscall.h"                         // for syscall
#include <algorithm>                         // for any_of
#include <credence/language/hir/binding.h>   // for Object, Function, Label
#include <credence/target/common/runtime.h>  // for is_stdlib_function, is_...
#include <deque>                             // for deque
#include <matchit.h>                         // for App, Wildcard, Ds, app
#include <ranges>                            // for split
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get, tuple
#include <utility>                           // for pair
#include <variant>                           // for variant

/****************************************************************************
 * x86-64 IR Visitor
 *
 * Translates each ITA instruction into x86-64 instructions. Every local is
 * on the stack below rbp, and the value of an expression is in the
 * accumulator, eax or rax.
 *
 *  Example:
 *
 *    PUSH _p5_4;
 *    PUSH _p4_3;
 *    CALL add;
 *    POP 16;
 *    _t8 = RET;
 *
 *  Becomes:
 *
 *    mov edi, dword ptr [rbp - 4]
 *    mov r8d, 2
 *    call add
 *    mov dword ptr [rbp - 8], eax
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   rax      = The default "accumulator" register for expression expansion,
 *              the return value, and the number of a syscall
 *   rdi, r8, r9, rsi, rdx, rcx
 *            = The arguments of a call to a function of the program
 *   rdi, rsi, rdx, rcx, r8, r9
 *            = The arguments of a call to the standard library, System V
 *   rdi, rsi, rdx, r10, r8, r9
 *            = The arguments of a syscall
 *   xmm0 - xmm7
 *            = The float and double arguments of the standard library
 *   xmm7     = The store register of a float or double literal
 *   rcx      = The address register of the address-of operator, and the
 *              shift count of a shift by a register, in cl
 *   rdx      = The high half of the dividend and the remainder of `idiv`
 *   r10      = The address register of a dereference
 *   r11      = The scratch register of the right-hand-side of a binary
 *              expression, the divisor of `idiv`, and the value of a
 *              global argument
 *   r15      = The address of argc, argv on the stack
 *   rbp      = The frame pointer
 *
 *   Locals, parameters, vectors, and vector offsets are always on the
 *   stack below rbp
 *
 *****************************************************************************/

namespace credence::target::x86_64 {

namespace m = matchit;

/**
 * @brief IR Instruction Instruction::FUNC_START
 *
 * The frame pointer is pushed and set, and a frame with calls reserves its
 * locals below rsp, by the frame size that is set at the end of the function
 *
 *  Example:
 *
 *    __main():
 *     BeginFunc ;
 *
 *  Becomes, in a frame with calls:
 *
 *    push rbp
 *    mov rbp, rsp
 *    sub rsp, 16
 */
void IR_Instruction_Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.get_table();
    credence_assert(table->functions.contains(name));
    accessor_->stack->clear();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto& frame = table->functions.at(name).get();
    auto& inst = instruction_accessor->get_instructions();
    // function prologue
    x8664_add__asm(inst, push, rbp);
    x8664_add__asm(inst, mov_, rbp, rsp);
    // a frame with no calls keeps its locals below rsp, in the red zone
    if (!ir::stack_frame_contains_call_instruction(
            frame, accessor_->table_accessor.get_ir_instructions()))
        return;
    auto imm = u32_int_immediate(0);
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::Align, instruction_accessor->size());
    x8664_add__asm(inst, sub, rsp, imm);
    if (name == "main")
        return;
    // a call overwrites the argument registers, so each parameter of a
    // frame with calls is stored in its slot
    for (std::size_t index = 0; index < frame.parameters.size(); index++) {
        auto const& parameter = frame.parameters.at(index);
        auto value =
            parameter.starts_with("*")
                ? memory::registers::available_qword_register.at(index)
                : memory::registers::available_dword_register.at(index);
        auto slot = fmt::format(
            "__parameter_{}", type::get_unary_rvalue_reference(parameter));
        accessor_->stack->set_address_from_accumulator(slot, value);
        x8664_add__asm(inst, mov, accessor_->stack->get(slot).first, value);
    }
}

/**
 * @brief IR Instruction Instruction::FUNC_END
 *
 * The argument registers are reset for the next function
 */
void IR_Instruction_Visitor::from_func_end_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& frame = stack_frame_.get_stack_frame();
    if (ir::stack_frame_contains_call_instruction(
            frame, accessor_->table_accessor.get_ir_instructions())) {
        auto imm = u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Align, instruction_accessor->size());
    }
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief IR Instruction Instruction::LOCL
 *
 * Every local is allocated on the stack: a pointer is an aligned quadword,
 * a vector is one chunk of its elements, and any other local is the size of
 * its type
 *
 *  Example:
 *
 *    LOCL x;
 *    LOCL *p;
 *
 *  Becomes:
 *
 *    x  -> [rbp - 4]
 *    p  -> [rbp - 16]
 */
void IR_Instruction_Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto& table = accessor_->table_accessor.get_table();
    auto& stack = accessor_->stack;
    auto is_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                stack->set_address_from_address(lvalue);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto& vector = table->vectors.at(locl_lvalue).get();
                auto size = stack->get_stack_size_from_table_vector(vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        m::pattern | m::_ =
            [&] {
                auto type = table->get_type_from_object_data_type(locl_lvalue);
                stack->set_address_from_type(locl_lvalue, type);
            }

    );
}

/**
 * @brief IR Instruction Instruction::PUSH
 *
 * The arguments of a call are pushed in reverse order, so each is added to
 * the front of the argument stack, and the first argument is at the front.
 * An argument stored in a slot before the call is loaded from its slot.
 */
void IR_Instruction_Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.get_table();
    auto& frame = stack_frame_.get_stack_frame();
    auto argument = std::get<1>(inst);
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(argument, frame));
    stack_frame_.argument_slots.emplace_front(
        accessor_->stack->is_allocated(argument) ? argument : "");
}

/**
 * @brief IR Instruction Instruction::POP
 *
 * The arguments of the call are in registers, so the argument stack and
 * slots are cleared and nothing is emitted
 */
void IR_Instruction_Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    stack_frame_.argument_slots.clear();
    stack_frame_.call_stack.pop_back();
}

/**
 * @brief IR Instruction Instruction::CALL
 *
 * A call is to a syscall, a function of the standard library, or a
 * function of the program, and its arguments are loaded from the argument
 * stack into the argument registers
 *
 *  Example:
 *
 *    CALL add;
 *
 *  Becomes:
 *
 *    mov edi, dword ptr [rbp - 4]
 *    mov r8d, 2
 *    call add
 */
void IR_Instruction_Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_ };
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));
    auto is_syscall_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);
#elif defined(_WIN32) || defined(_WIN64)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);
#endif
    };
    auto is_stdlib_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);
#elif defined(_WIN32) || defined(_WIN64)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);
#endif
    };
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::app(is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instruction_accessor->get_instructions());
            });
    stack_frame_.call_stack.emplace_back(function_name);
    stack_frame_.tail = function_name;
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief IR Instruction Instruction::MOV
 *
 * The lvalue and rvalue select the inserter:
 *
 *  * An argument slot, `_p1_1`, of a temporary or dereference is stored on
 *    the stack, since the accumulator is overwritten before the call
 *
 *  * A temporary is the value of its expression in the accumulator
 *
 *  * A dereference to a dereference, `*a = *b`, is a load then a store
 *
 *  * An element of a global vector is a load or store relative to rip
 *
 *  * Any other assignment is a mov between storage devices
 *
 *  Example:
 *
 *    _t2 = x + (10:int:4);
 *    y = _t2;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, 10
 *    mov dword ptr [rbp - 8], eax
 */
void IR_Instruction_Visitor::from_mov_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.get_table();
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->vectors.contains(rvalue_reference) and
               table->globals.is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        // an argument of a computed value is stored before the next
        // instruction, otherwise it's resolved when it's pushed
        m::pattern | m::ds(m::app(is_parameter, true), m::_) =
            [&] {
                auto const& held = accessor_->accumulator_temporary;
                if ((type::is_temporary(rhs) and
                        ((held.has_value() and held->first == rhs) or
                            accessor_->stack->is_allocated(rhs))) or
                    type::is_dereference_expression(rhs))
                    insert_temporary_to_stack(rhs, lhs);
            },
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
                auto const& held = accessor_->accumulator_temporary;
                if (held.has_value() and held->first == lhs and
                    is_temporary_used_after_next_instruction(lhs))
                    insert_temporary_to_stack(lhs, lhs);
            },
        m::pattern | m::ds(m::app(type::is_dereference_expression, true),
                         m::app(type::is_dereference_expression, true)) =
            [&] {
                operand_inserter.insert_from_accumulator_to_lvalue(
                    lhs, operand_inserter.insert_operand_into_accumulator(rhs));
            },
        m::pattern | m::ds(m::app(is_global_vector, true), m::_) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::ds(m::_, m::app(is_global_vector, true)) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });
}

/**
 * @brief Check if a temporary is an operand after the next instruction
 *
 * The accumulator holds a temporary until the next instruction, so a
 * temporary used after it is stored on the stack
 */
bool IR_Instruction_Visitor::is_temporary_used_after_next_instruction(
    LValue const& temporary)
{
    auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
    auto is_operand = [&](std::string_view operand) {
        return std::ranges::any_of(
            std::views::split(operand, ' '), [&](auto const& part) {
                return std::string_view{ part.begin(), part.end() } ==
                       temporary;
            });
    };
    for (auto index = iterator_index_ + 2; index < ir_instructions.size();
        index++) {
        auto const& inst = ir_instructions.at(index);
        if (std::get<0>(inst) == ir::Instruction::FUNC_END)
            break;
        if ((std::get<0>(inst) != ir::Instruction::MOV and
                is_operand(std::get<1>(inst))) or
            is_operand(std::get<2>(inst)) or is_operand(std::get<3>(inst)))
            return true;
    }
    return false;
}

/**
 * @brief Store the value of a temporary in a stack slot
 *
 *  Example:
 *
 *    _t2 = x;
 *    x = ++x;
 *    y = _t2;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    mov dword ptr [rbp - 12], eax
 *    inc dword ptr [rbp - 4]
 *    mov eax, dword ptr [rbp - 12]
 */
void IR_Instruction_Visitor::insert_temporary_to_stack(LValue const& temporary,
    LValue const& slot)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto value = operand_inserter.insert_operand_into_accumulator(temporary);
    if (!accessor_->stack->is_allocated(slot))
        accessor_->stack->set_address_from_accumulator(slot, value);
    x8664_add__asm(instructions, mov, accessor_->stack->get(slot).first, value);
}

/**
 * @brief IR Instruction Instruction::CMP
 *
 * A comparison is a temporary, `_t3 = CMP x`, and the JMP_E that follows it
 * makes the comparison, so there is no CMP instruction to translate
 */
void IR_Instruction_Visitor::from_cmp_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief IR Instruction Instruction::IF
 *
 * A relational condition inserted its own conditional jump, otherwise the
 * jump is taken when the condition is not zero
 */
void IR_Instruction_Visitor::from_if_ita(ir::Quadruple const& inst)
{
    if (accessor_->relational_jump) {
        accessor_->relational_jump = false;
        return;
    }
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto condition =
        operand_inserter.insert_operand_into_accumulator(std::get<1>(inst));
    auto label = direct_immediate(
        assembly::make_label(std::get<3>(inst), stack_frame_.symbol));
    x8664_add__asm(instructions, cmp, condition, u32_int_immediate(0));
    x8664_add__asm(instructions, jne, label);
}

/**
 * @brief IR Instruction Instruction::JMP_E
 *
 * A case of a switch statement, the jump is taken when the operand of its
 * CMP temporary is equal to the case
 *
 *  Example:
 *
 *    _t3 = CMP x;
 *    JMP_E _t3 (1:int:4) _L5;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    cmp eax, 1
 *    je ._L5__main
 */
void IR_Instruction_Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto& frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame.temporary.at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage = accessor_->address_accessor
                                 .get_lvalue_address_and_insertion_instructions(
                                     of_comparator, instructions.size())
                                 .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::eax);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief IR Instruction Instruction::GOTO
 *
 *  Example:
 *
 *    GOTO _L4;
 *
 *  Becomes:
 *
 *    jmp ._L4__main
 */
void IR_Instruction_Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    x8664_add__asm(instructions, goto_, label);
}

/**
 * @brief IR Instruction Instruction::RETURN
 *
 * The value of the return statement is moved to the return register, eax or
 * rax. A function may return from more than one statement.
 */
void IR_Instruction_Visitor::from_return_ita(ir::Quadruple const& inst)
{
    auto inserter = Expression_Inserter{ accessor_ };
    auto& frame = accessor_->table_accessor.get_table()
                      ->functions.at(stack_frame_.symbol)
                      .get();
    if (!frame.ret.has_value())
        return;
    // each return statement returns its own rvalue
    auto rvalue = util::str_trim_ws(std::get<1>(inst));
    inserter.insert_from_return_rvalue(
        std::make_pair(frame.ret->first, rvalue));
}

/**
 * @brief IR Instruction Instruction::LEAVE
 *
 * A function restores rsp and rbp with `leave` and returns, and `main`
 * releases its frame and exits with the exit syscall
 *
 *  Example:
 *
 *    LEAVE;
 *
 *  Becomes, in `main` on Linux:
 *
 *    add rsp, 16
 *    mov rax, 60
 *    mov rdi, 0
 *    syscall
 */
void IR_Instruction_Visitor::from_leave_ita()
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    // `main` is the entry point, so there is no caller to return to
    if (stack_frame_.symbol == "main") {
        if (ir::stack_frame_contains_call_instruction(
                stack_frame_.get_stack_frame(),
                accessor_->table_accessor.get_ir_instructions())) {
            auto size = u32_int_immediate(
                accessor_->stack->get_stack_frame_allocation_size());
            x8664_add__asm(accessor_->instruction_accessor->get_instructions(),
                add,
                rsp,
                size);
        }
        syscall_ns::common::exit_syscall(instructions, 0);
    } else {
        // `leave` restores the stack pointer of a frame with calls
        x8664_add__asm(instructions, leave);
        x8664_add__asm(instructions, ret);
    }
}

/**
 * @brief IR Instruction Instruction::LABEL
 *
 * A label is local to its function, `_L4` in `main` is `._L4__main`
 */
void IR_Instruction_Visitor::from_label_ita(ir::Quadruple const& inst)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    instructions.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

}